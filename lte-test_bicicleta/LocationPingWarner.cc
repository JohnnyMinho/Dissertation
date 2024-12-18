#include "LocationPingWarner.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/application/Middleware.h"
#include "artery/traci/VehicleController.h"
#include "artery/utility/InitStages.h"
#include "artery/utility/PointerCheck.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>

using namespace omnetpp;

Define_Module(LocationPingWarner)

LocationPingWarner::~LocationPingWarner()
{
    cancelAndDelete(PingPollingTrigger);
    cancelAndDelete(StartConfigTrigger);
}

void LocationPingWarner::initialize(int stage)
{
    if (stage == artery::InitStages::Prepare) {
        pollingRadius = par("pollingRadius");
        PingPollingInterval = par("pollingInterval");
        PingPollingTrigger = new cMessage("poll central for query");
        StartConfigTrigger = new cMessage("ConfiguringNode");
        Num_Bycicles_Cluster = par("Num_Bycicles_Cluster");
        CheckForClusters = new cMessage("CheckForClusters");

        numWarningsCentral = 0;
        clusters_detected = 0;
        numPositionsUpdated = 0;
        numMQTTPacketsReceived = 0;
        known_nodes = 0;
        
        WATCH(clusters_detected);
        WATCH(numPositionsUpdated);
        WATCH(numWarningsCentral);
        WATCH(known_nodes);
        WATCH(numMQTTPacketsReceived);

    } else if (stage == artery::InitStages::Self) {
        scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);

        scheduleAt(simTime() + uniform(0.0, PingPollingInterval), PingPollingTrigger);
    }/* else if (stage == artery::InitStages::Propagate){
        socket.setOutputGate(gate("udpOut"));
        auto centralAddress = inet::L3AddressResolver().resolve(par("centralAddress"));
        socket.connect(centralAddress, par("centralPort"));

        auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);
        vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());

        scheduleAt(simTime() + uniform(0.0, PingPollingInterval), PingPollingTrigger);
    }*/
}

int LocationPingWarner::numInitStages() const
{
    return artery::InitStages::Total;
}

void LocationPingWarner::finish()
{
    socket.close();
    warningsVector.record(numWarningsCentral);
	warningStats.collect(numWarningsCentral);
    recordScalar("numWarningsCentral", numWarningsCentral);
    recordScalar("numMQTTPacketsReceivedFromCentral", numMQTTPacketsReceived);
    recordScalar("clusters_detected_warner",clusters_detected);
    recordScalar("Positions Updated:",numPositionsUpdated);
}

void LocationPingWarner::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        if(msg == StartConfigTrigger){
            ConfigNode();
        }else if(msg == CheckForClusters){
            CheckForCluster();
        }else{
            pollCentral();
        }
    } else if (msg->getKind() == inet::UDP_I_DATA) {
        const char* Message_Name_Checker = msg->getName();
        EV_INFO << "Message_Name_Checker-> " << Message_Name_Checker << "\n";
        std::string aux_string_checker = Message_Name_Checker;
        if(aux_string_checker.find("Publish") != std::string::npos){
            processMQTTPublish(*check_and_cast<MQTTPacket*>(msg));
        }else{
            processResponse(*check_and_cast<LocationPingResponse*>(msg));
        }
        delete msg;
    } else {
        throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
}

void LocationPingWarner::ConfigNode()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(par("centralPort"));
    socket.setBroadcast(true);
    auto centralAddress = inet::L3AddressResolver().resolve(par("centralAddress"));
    socket.connect(centralAddress, par("centralPort"));

    auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);
    //vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());
}

void LocationPingWarner::pollCentral()
{
    Enter_Method("polling Central");
    auto query = new LocationPingQuery("poll location pings");
    auto subscribe = new MQTTPacket("Subscribe");
    //query->setPositionX(vehicleController->getPosition().x / boost::un its::si::meter);
    //query->setPositionY(vehicleController->getPosition().y / boost::units::si::meter);
    query->setRadius(pollingRadius);
    subscribe->setCmd(1); //0-> Publish | 1-> Subscribe
    subscribe->setRetain(true);
    subscribe->setQos(0);
    subscribe->setDuplicated(false);
    subscribe->setLength(256); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
    subscribe->setTopic("#"); //According to the MQTT parameters, a subscription to the topic # will allow us to subscribe to all topics except the ones beginning with the $ symbol
    //char msg[256];
    socket.send(subscribe);
    scheduleAt(simTime() + PingPollingInterval, PingPollingTrigger);
}

char* LocationPingWarner::MqttPayloadExtractor(const char* full_payload,int position)
{
    char buffer[256];  // A temporary buffer to hold the copied payload. Adjust size as needed.
    strncpy(buffer, full_payload, sizeof(buffer));
    char* payload_copy = strtok(buffer,"|");
    //EV_INFO << "token: " << payload_copy << "\n";
    //delay(10);    
    if(position == 0){
        char* result = new char[strlen(payload_copy) + 1];
        strcpy(result, payload_copy);
        return result;
    }
    for(int tokens_done = 0; payload_copy != NULL; tokens_done++){
        if(tokens_done == position){
            EV_INFO << "token: " << payload_copy << "\n";
            char* result = new char[strlen(payload_copy) + 1];
            strcpy(result, payload_copy);
            return result;
        }else{
            payload_copy = strtok(NULL,"|");
        }
    }
    return nullptr;
}

void LocationPingWarner::processResponse(LocationPingResponse& response)
{
    EV_INFO << "LocationPings done: " << response.getPings() << "\n";
    if (response.getPings() >= 2 && !reducedSpeed) {
        //vehicleController->setSpeedFactor(0.5);
        //reducedSpeed = true;
        
    } else if (response.getPings() == 0 && reducedSpeed) {
        //vehicleController->setSpeedFactor(1.0);
        reducedSpeed = false;
    }
    ++numWarningsCentral;
}

void LocationPingWarner::CheckForCluster(){

    for(auto& vehicle : Positions_Storage){
        detected_within_range = 0;
        EV_INFO << "Vehicle: " << vehicle.getPayload() << "\n";
        for(auto& Vehicle_Compared : Positions_Storage){
            if(strcmp(vehicle.getTopic(),Vehicle_Compared.getTopic())!= 0){   
                EV_INFO << "Vehicle being compared: " << Vehicle_Compared.getTopic() << "\n";
                char* x_str = MqttPayloadExtractor(vehicle.getPayload(), 1);
                char* y_str = MqttPayloadExtractor(vehicle.getPayload(), 2);
                char* x_str_compared = MqttPayloadExtractor(Vehicle_Compared.getPayload(), 1);
                char* y_str_compared = MqttPayloadExtractor(Vehicle_Compared.getPayload(), 2);
                
                if(y_str != nullptr && x_str != nullptr && x_str_compared != nullptr && y_str_compared != nullptr){
                    double dx = std::atof(x_str_compared) - std::atof(x_str); //Pythagoras 
                    double dy = std::atof(y_str_compared) - std::atof(y_str);
                    EV_INFO << "Result X: "<< dx <<"\n";
                    EV_INFO << "Result Y:" << dy <<"\n";
                    if( dx*dx + dy*dy < pollingRadius * pollingRadius){ //We basicly want the hypotenuse to be higher than the hypotenuse given by the 2 points
                        ++detected_within_range;
                        EV_WARN << "Vehicle in Range"<< "\n";
                    }
                }else{
                    EV_WARN << "Vehicle positions were invalid! " << "\n";
                }
            }
            if(detected_within_range >= Num_Bycicles_Cluster){
                EV_INFO << "Cluster Detected" << "\n";
                ++clusters_detected;
            }
        }
    }
}

void LocationPingWarner::processMQTTPublish(MQTTPacket& response)
{
    detected_within_range = 0;
    bool register_new_topic = true;
    EV_INFO << "MQTT Packet Payload: " << response.getPayload() << "\n";
    EV_INFO << "Postions Stored: " << Positions_Storage.size() << "\n";
    if(Positions_Storage.size() != 0){
        for(auto& elementInStorage : Positions_Storage){
            EV_INFO << "element Packet Payload: " << elementInStorage.getPayload() << "\n";
            const char* NewPacketTopic = MqttPayloadExtractor(response.getPayload(),0);
            if(strcmp(NewPacketTopic,elementInStorage.getTopic()) == 0){
                elementInStorage = response;
                register_new_topic = false;
                ++numPositionsUpdated;
            } //We need to check if the element if already in storage
        }
        if(register_new_topic){
            EV_WARN << "New Element added" <<"\n";
            ++known_nodes;
            Positions_Storage.push_back(response);
        }
    }else{
        ++known_nodes;
        Positions_Storage.push_back(response);
    }
    ++numMQTTPacketsReceived;
    cancelEvent(CheckForClusters);
    scheduleAt(simTime() + 0.2, CheckForClusters);
    
}
