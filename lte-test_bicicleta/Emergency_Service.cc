#include "Emergency_Service.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/application/Middleware.h"
#include "artery/traci/VehicleController.h"
#include "artery/utility/InitStages.h"
#include "artery/utility/PointerCheck.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>

//The emergency service must be able to communicate trough WLAN and PPP / Ethernet so it will be a ITS-G5 service
//We need to be able to generate a vehicle as soon as the Station RSU receives a signal
//For simplicity sake we will be assuming that the server has a direct connection to the emergency station (initially)

using namespace omnetpp;

//Since we receive the SOS warnings in the MQTT UdpApp we use signals between the two components as a way to warn this adapted DEN service 
//Using signals is a somewhat complex way since we had some overhead but it allows us to keep the udpapp and the service somewhat uncoupled allowing for
//Bigger freedoms and less hassel when dealing with changes in any of the two

static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT = cComponent::registerSignal("SOS_Sent"); //This is a way to tell the MQTT server that the emergency was answered and we don't need to keep the slot occupied

Define_Module(Emergency_Service)

Emergency_Service::~Emergency_Service()
{
    cancelAndDelete(PingPollingTrigger);
    cancelAndDelete(StartConfigTrigger);
}

void Emergency_Service::initialize(int stage)
{
    //ItsG5BaseService::initialize();
    if (stage == artery::InitStages::Prepare) {
        ItsG5BaseService::initialize();
        StartConfigTrigger = new cMessage("ConfiguringNode");

    } else if (stage == artery::InitStages::Self) {
        subscribe(SharedSignal_MQTT_To_Den);
        scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);
    }
}

int Emergency_Service::numInitStages() const
{
    return artery::InitStages::Total;
}

void Emergency_Service::finish()
{
    socket.close();
    warningsVector.record(numWarningsCentral);
	warningStats.collect(numWarningsCentral);
    recordScalar("numWarningsCentral", numWarningsCentral);
    recordScalar("numMQTTPacketsReceivedFromCentral", numMQTTPacketsReceived);
    recordScalar("clusters_detected_warner",clusters_detected);
    recordScalar("Positions Updated:",numPositionsUpdated);
}

void Emergency_Service::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        if(msg == StartConfigTrigger){
            ConfigNode();
        }
    } else if (msg->getKind() == inet::UDP_I_DATA) {
        const char* Message_Name_Checker = msg->getName();
        EV_INFO << "Message_Name_Checker-> " << Message_Name_Checker << "\n";
        std::string aux_string_checker = Message_Name_Checker;
        if(aux_string_checker.find("Publish") != std::string::npos){
            // 
        }else{
            //
        }
        delete msg;
    } else {
        throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
}

void Emergency_Service::ConfigNode()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(par("centralPort"));
    socket.setBroadcast(true);
    auto centralAddress = inet::L3AddressResolver().resolve(par("centralAddress"));
    socket.connect(centralAddress, par("centralPort"));

    auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);
    //vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());
}
