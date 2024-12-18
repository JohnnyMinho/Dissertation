#ifndef EMERGENCY_SERVICE_MQTT_H_
#define EMERGENCY_SERVICE_MQTT_H_

#include <inet/transportlayer/contract/udp/UDPSocket.h>
#include "artery/application/ItsG5BaseService.h"
#include <omnetpp/csimplemodule.h>
#include "artery/application/EmergencyServiceRSU.h"
#include "traci/Angle.h"
#include "traci/Boundary.h"
#include "traci/NodeManager.h"
#include "traci/Listener.h"
#include "traci/Position.h"
#include "traci/API.h"
#include "traci/SubscriptionManager.h"
#include <list>

// forward declaration
class BlackIceResponse;
class MQTTPacket;
class API;
class EmergencyServiceRSU;

namespace traci { class VehicleController; }

class Emergency_Service_MQTT : public artery::ItsG5BaseService, public traci::Listener
{
public:
    ~Emergency_Service_MQTT();

protected:
    int numInitStages() const override;
    void initialize(int stage) override;
    void finish() override;
    void handleMessage(omnetpp::cMessage*) override;
    void AmbulanceDispatch(MQTTPacket&);

    // DenService.h part
    //void receiveSignal(omnetpp::cComponent*, omnetpp::simsignal_t, omnetpp::cObject*, omnetpp::cObject*) override;
    //void indicate(const vanetza::btp::DataIndication&, std::unique_ptr<vanetza::UpPacket>) override;
    //void trigger() override;
    
private:

    void pollCentral();
    void ConfigNode();
    //void AnsMachine();
    void processPacket(omnetpp::cPacket*);
    void processMQTTPacket(MQTTPacket&);
    void ProcessSOS(MQTTPacket& SubscribePacket);
    std::vector<char*> Location_And_ID_Extractor(const char * Sos_Packet_content);
    //void processMQTTSubscribe(MQTTPacket&, const inet::L3Address&, int port);

    inet::UDPSocket socket;
    inet::UDPSocket EmergSocket;
    double pollingRadius; //We can use this to detect clusters , example: "We store the locations and everytime we get a topic update we compare the number of nodes inside each onde radius (ex 100m)"
    omnetpp::simtime_t PingPollingInterval;
    omnetpp::simtime_t StartConfigTime;
    omnetpp::cMessage* PingPollingTrigger = nullptr;
    omnetpp::cMessage* StartConfigTrigger = nullptr;
    traci::VehicleController* vehicleController = nullptr;

    omnetpp::cHistogram warningStats;
    omnetpp::cOutVector warningsVector;

    bool reducedSpeed = false;
    int numWarningsCentral;
    int numMQTTPacketsReceived;
    int known_nodes;
    int numPositionsUpdated;
    int Num_Bycicles_Cluster;
    int detected_within_range;
    int clusters_detected;
    bool more_requests; //This variable is useful to avoid checking for clusters before all topics have arrived

    int EmergPort;

    int numReceivedMQTTPublish;
    int numReceivedMQTTSubscribe;

    std::list<MQTTPacket> MqttPackets;
    inet::L3Address current_sub_addr;
    int current_sub_port;
    inet::UDPDataIndication* subscriber_udp_socket;
    std::list<MQTTPacket*> Mqtt_Queue;
    int Current_Packet_Position;
    omnetpp::cMessage* SubAnsMchTrigger = nullptr;

    artery::EmergencyServiceRSU* Emerg_Service;

    std::list<std::string> Ambulances_Used;
    bool Ambulance_in_use;
    // DENService.h part

};

#endif 

