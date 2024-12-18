#ifndef LOCATIONPINGWARNER_H_
#define LOCATIONPINGWARNER_H_

#include <inet/transportlayer/contract/udp/UDPSocket.h>
#include <omnetpp/csimplemodule.h>

// forward declaration
class BlackIceResponse;
class LocationPingResponse;
class MQTTPacket;

namespace traci { class VehicleController; }

class LocationPingWarner : public omnetpp::cSimpleModule
{
public:
    ~LocationPingWarner();

protected:
    int numInitStages() const override;
    void initialize(int stage) override;
    void finish() override;
    void handleMessage(omnetpp::cMessage*) override;

private:
    void pollCentral();
    void ConfigNode();
    void processResponse(LocationPingResponse&);
    void processMQTTPublish(MQTTPacket&);
    char* MqttPayloadExtractor(const char* ,int position);
    void CheckForCluster();

    inet::UDPSocket socket;
    double pollingRadius; //We can use this to detect clusters , example: "We store the locations and everytime we get a topic update we compare the number of nodes inside each onde radius (ex 100m)"
    omnetpp::simtime_t PingPollingInterval;
    omnetpp::simtime_t StartConfigTime;
    omnetpp::cMessage* PingPollingTrigger = nullptr;
    omnetpp::cMessage* StartConfigTrigger = nullptr;
    omnetpp::cMessage* CheckForClusters = nullptr;
    traci::VehicleController* vehicleController = nullptr;

    omnetpp::cHistogram warningStats;
    omnetpp::cOutVector warningsVector;
    std::list<MQTTPacket> Positions_Storage;

    bool reducedSpeed = false;
    int numWarningsCentral;
    int numMQTTPacketsReceived;
    int known_nodes;
    int numPositionsUpdated;
    int Num_Bycicles_Cluster;
    int detected_within_range;
    int clusters_detected;
    bool more_requests; //This variable is useful to avoid checking for clusters before all topics have arrived

};

#endif 

