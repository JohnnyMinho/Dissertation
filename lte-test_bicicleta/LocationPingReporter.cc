#include "LocationPingReporter.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/application/Middleware.h"
#include "artery/application/StoryboardSignal.h"
#include "artery/traci/VehicleController.h"
#include "artery/utility/InitStages.h"
#include "artery/application/VehicleDataProvider.h"
#include "artery/utility/simtime_cast.h"
#include "veins/base/utils/Coord.h"
#include "artery/utility/PointerCheck.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>
#include <cstdlib>  // For rand() and srand()
#include <ctime>
#include <chrono>

using namespace omnetpp;

Define_Module(LocationPingReporter)

LocationPingReporter::~LocationPingReporter()
{
    cancelAndDelete(PingTrigger);
}

static const simsignal_t storyboardSignal = cComponent::registerSignal("StoryboardSignal");

void LocationPingReporter::initialize(int stage)
{
    if (stage == artery::InitStages::Prepare) {
        tractionLosses = 0;
        test = 0;
        SOS_Status = false;
        Already_got_fall_sig = false;
        PingInterval = par("PingInterval");
        PingTrigger = new cMessage("Ping central");
        WATCH(tractionLosses);
        WATCH(test);
    } else if (stage == artery::InitStages::Self) {
        socket.setOutputGate(gate("udpOut"));
        auto centralAddress = inet::L3AddressResolver().resolve(par("centralAddress"));
        socket.connect(centralAddress, par("centralPort"));

        auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);
        mw->subscribe(storyboardSignal, this);
        vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());

        scheduleAt(simTime() + uniform(0.0, PingInterval), PingTrigger);
    }
}

int LocationPingReporter::numInitStages() const
{
    return artery::InitStages::Total;
}

void LocationPingReporter::finish()
{
    socket.close();
    recordScalar("traction losses", tractionLosses);
}

void LocationPingReporter::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        sendPing();
    }  else if (msg->getKind() == inet::UDP_I_DATA) {
        delete msg;
    } else {
        throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
}

void LocationPingReporter::receiveSignal(cComponent*, simsignal_t sig, cObject* obj, cObject*)
{
    if (sig == storyboardSignal) {
        auto sigobj = check_and_cast<artery::StoryboardSignal*>(obj);
        if (sigobj->getCause() == "fall") {

            //Even though we got a signal to generate a fall we should have a 1/100 to have a fall in a regular use, for testing I will use a 1/10 chance to generate a fall
            //This condition should stay until a emergency vehicle treats the cyclist or takes him to the hospital.
            //After being sent to the hospital or being treated the cyclist should attempt to return to his cluster or to the flow path.
            //I'm still missing the knowledge on how to generate both the ambulance and send it to the emergency and how to return the cyclist to it's flow
            //To generate the ambulance I'll probably have to make a service that works with the TraCI manager
            //The ambulance generation is already done and I can probably do it trough 2 ways
            //This method of generating an accident needs to be changed because it's starting to cause problems when 2 bycicles have an accident way too close to each other in the same edge

            test++;
            Already_got_fall_sig = true;
            srand(static_cast<unsigned>(time(0)));
            int fall = rand() % 10;
            if (fall == 0) {
                //The bycicle stops
                EV_WARN << "This Bycicle fell!" << "\n";
                vehicleController->setSpeedFactor(0.0);
                if(!SOS_Status){
                    SOS_Status = true;
                    sendSOS();
                }
            } else {
                //Nothing happens
            }

        }
    }
}

void LocationPingReporter::sendReport()
{
    Enter_Method_Silent();
    using boost::units::si::meter;
    using boost::units::si::meter_per_second;
    auto report = new BlackIceReport("reporting black ice");
    report->setPositionX(vehicleController->getPosition().x / meter);
    report->setPositionY(vehicleController->getPosition().y / meter);
    report->setSpeed(vehicleController->getSpeed() / meter_per_second);
    report->setTime(simTime());
    socket.send(report);
}

void LocationPingReporter::sendSOS()
{
    Enter_Method("Send SOS");
    using boost::units::si::meter;
    using boost::units::si::meter_per_second;
    auto ping = new LocationPing("New SOS");
    auto New_MQTT_Message = new MQTTPacket("Publish");
    ping->setVehicleId(vehicleController->getVehicleId().c_str());
    ping->setPositionX(vehicleController->getPosition().x / meter);
    ping->setPositionY(vehicleController->getPosition().y / meter);
    ping->setTime(simTime());

    New_MQTT_Message->setCmd(0); //0-> Publish | 1-> Subscribe
    New_MQTT_Message->setRetain(true);
    New_MQTT_Message->setQos(0);
    New_MQTT_Message->setDuplicated(false);
    New_MQTT_Message->setLength(256); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
    New_MQTT_Message->setTopic((ping->getVehicleId()));
    char msg[256];
    char msg_Topic[256];
    int size_posX = snprintf(NULL, 0, "%f", ping->getPositionX());
    int size_posY = snprintf(NULL, 0, "%f", ping->getPositionY());
    char posX[size_posX+1];
    char posY[size_posY+1];
    snprintf(posX, size_posX+1,"%f",ping->getPositionX());
    snprintf(posY, size_posY+1,"%f",ping->getPositionY());
    strncpy(msg,ping->getVehicleId(),sizeof(msg));
    strcat(msg,"|");
    strcat(msg,posX);
    strcat(msg,"|");
    strcat(msg,posY);
    strcat(msg,"|");
    strcat(msg_Topic,"SOS");
    strcat(msg_Topic,ping->getVehicleId());
    New_MQTT_Message->setPayload(msg);
    New_MQTT_Message->setTopic("SOS");
    //New_MQTT_Message->setPayload(ping->getVehicleId()+"|"+ping->getPositionX()+"|"+ping->getPositionY()+"|"+ping->getTime());

    mLastPingTimestamp = simTime();
    socket.send(New_MQTT_Message);
    char text[128];
    sprintf(text,"SOS SENT-Event number: %lld", getSimulation()->getEventNumber());
    getSimulation()->getActiveEnvir()->alert(text);
}

void LocationPingReporter::sendPing()
{
    Enter_Method("Send Ping");
    char test[126];
    sprintf(test,"MY ID -> %s | MY Type -> %s",vehicleController->getVehicleId().c_str(), vehicleController->getVehicleType().getTypeId().c_str());

    EV_INFO << test << "\n";
    using boost::units::si::meter;
    using boost::units::si::meter_per_second;
    auto ping = new LocationPing("Updating Location");
    auto New_MQTT_Message = new MQTTPacket("Publish");
    ping->setVehicleId(vehicleController->getVehicleId().c_str());
    ping->setPositionX(vehicleController->getPosition().x / meter);
    ping->setPositionY(vehicleController->getPosition().y / meter);
    ping->setTime(simTime());

    //MQTT Part (If I can't find out how to activate a MQTT_client instance to run at the same time or to be activated when it is required
    //A easy solution would either be pipes or  )

    //MQTT Packet true constituiton (not fields but byte and hex values)
    //https://cedalo.com/blog/mqtt-packet-guide/

    New_MQTT_Message->setCmd(0); //0-> Publish | 1-> Subscribe
    New_MQTT_Message->setRetain(true);
    New_MQTT_Message->setQos(0);
    New_MQTT_Message->setDuplicated(false);
    New_MQTT_Message->setLength(256); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
    New_MQTT_Message->setTopic((ping->getVehicleId()));
    char msg[256];
    int size_posX = snprintf(NULL, 0, "%f", ping->getPositionX());
    int size_posY = snprintf(NULL, 0, "%f", ping->getPositionY());
    char posX[size_posX+1];
    char posY[size_posY+1];
    snprintf(posX, size_posX+1,"%f",ping->getPositionX());
    snprintf(posY, size_posY+1,"%f",ping->getPositionY());
    strncpy(msg,ping->getVehicleId(),sizeof(msg));
    strcat(msg,"|");
    strcat(msg,posX);
    strcat(msg,"|");
    strcat(msg,posY);
    strcat(msg,"|");
    New_MQTT_Message->setPayload(msg);
    New_MQTT_Message->setTopic(ping->getVehicleId());
    //New_MQTT_Message->setPayload(ping->getVehicleId()+"|"+ping->getPositionX()+"|"+ping->getPositionY()+"|"+ping->getTime());

    mLastPingTimestamp = simTime();
    socket.send(New_MQTT_Message);
    scheduleAt(simTime() + uniform(0.0, PingInterval), PingTrigger);
}
