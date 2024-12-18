#include "Emergency_ServiceRSU.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/application/Middleware.h"
#include "artery/traci/VehicleController.h"
#include "artery/utility/InitStages.h"
#include "artery/utility/PointerCheck.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>
#include "veins/modules/mobility/traci/TraCICommandInterface.h"
#include "veins/modules/mobility/traci/TraCIScenarioManager.h"

//The emergency service must be able to communicate trough WLAN and PPP / Ethernet so it will be a ITS-G5 service
//We need to be able to generate a vehicle as soon as the Station RSU receives a signal
//For simplicity sake we will be assuming that the server has a direct connection to the emergency station (initially)

using namespace omnetpp;

static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT = cComponent::registerSignal("SOS_Sent");

Define_Module(Emergency_ServiceRSU)

Emergency_ServiceRSU::~Emergency_ServiceRSU()
{
    cancelAndDelete(PingPollingTrigger);
    cancelAndDelete(StartConfigTrigger);
}

void Emergency_ServiceRSU::initialize(int stage)
{
    ItsG5BaseService::initialize();
    
    if (stage == artery::InitStages::Prepare) {
        //ItsG5BaseService::initialize();
        pollingRadius = par("pollingRadius");
        PingPollingInterval = par("pollingInterval");
        PingPollingTrigger = new cMessage("poll central for query");
        StartConfigTrigger = new cMessage("ConfiguringNode");
        subscribe(SharedSignal_MQTT_To_Den);

    } else if (stage == artery::InitStages::Self) {
        EV_INFO << "RSU emergency Service initializing" << "\n";
        scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);

    }
}

int Emergency_ServiceRSU::numInitStages() const
{
    return artery::InitStages::Total;
}

void Emergency_ServiceRSU::finish()
{
    socket.close();
    warningsVector.record(numWarningsCentral);
	warningStats.collect(numWarningsCentral);
    recordScalar("numWarningsCentral", numWarningsCentral);
    recordScalar("numMQTTPacketsReceivedFromCentral", numMQTTPacketsReceived);
    recordScalar("clusters_detected_warner",clusters_detected);
    recordScalar("Positions Updated:",numPositionsUpdated);
}

void Emergency_ServiceRSU::handleMessage(cMessage* msg)
{
    EV_WARN << "Message Received on RSU Service" << "\n";
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

void Emergency_ServiceRSU::ConfigNode()
{

    auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);
    //vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());
}

void Emergency_ServiceRSU::addEmergency_Vehicle() {
    
    /*TraCIScenarioManager* manager = TraCIScenarioManagerAccess().get();
    TraCICommandInterface* traci = manager->getCommandInterface();

    
    std::string vehicleId = "new_vehicle_1";
    std::string routeId = "route_0"; // The route ID must exist in the SUMO configuration
    std::string vehicleTypeId = "car"; // This ID must be defined in SUMO

    double startPosition = 0.0;
    double startSpeed = 10.0;

    traci->addVehicle(vehicleId, routeId, vehicleTypeId, startPosition, startSpeed);*/
}