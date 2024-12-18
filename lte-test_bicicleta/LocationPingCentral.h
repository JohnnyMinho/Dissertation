#ifndef LOCATIONPINGCENTRAL_H_
#define LOCATIONPINGCENTRAL_H_

#include <inet/networklayer/common/L3Address.h>
#include <inet/transportlayer/contract/udp/UDPSocket.h>
#include <omnetpp/csimplemodule.h>
#include "traci/Angle.h"
#include "traci/Boundary.h"
#include "traci/NodeManager.h"
#include "traci/Listener.h"
#include "traci/Position.h"
#include "traci/SubscriptionManager.h"
#include <list>

// forward declaration
class BlackIceQuery;
class BlackIceReport;
class LocationPing;
class LocationPingQuery;
class MQTTPacket;

class LocationPingCentral : public omnetpp::cSimpleModule
{
public:
    ~LocationPingCentral();

protected:
    void initialize() override;
    void finish() override;
    void handleMessage(omnetpp::cMessage*) override;
    void ConfigNode();

private:
    void processPacket(omnetpp::cPacket*);
    void processReport(BlackIceReport&);
    void processLocationPing(LocationPing&);
    void processQuery(BlackIceQuery&, const inet::L3Address&, int port);
    void processLocationPingQuery(LocationPingQuery&, const inet::L3Address&, int port);
    void processMQTTPacket(MQTTPacket&);
    void processMQTTSubscribe(MQTTPacket&, const inet::L3Address&, int port);
    void SendSOS(MQTTPacket& );
    void AnsMachine();
    void disseminateWarning();

    int reportPort;
    int queryPort;
    int EmergPort;
    int numReceivedWarnings;
    int numReceivedQueries;
    int numReceivedMQTTPublish;
    int numReceivedMQTTSubscribe;
    inet::UDPSocket reportSocket;
    inet::UDPSocket querySocket;
    inet::UDPSocket EmergSocket;
    std::list<BlackIceReport> reports;
    std::list<LocationPing> pings;
    std::list<MQTTPacket> MqttPackets;
    omnetpp::cMessage* SubAnsMchTrigger = nullptr;
    omnetpp::cMessage* StartConfigTrigger = nullptr;
    omnetpp::simtime_t StartConfigTime;

    inet::L3Address current_sub_addr;
    int current_sub_port;
    inet::UDPDataIndication* subscriber_udp_socket;
    std::list<MQTTPacket*> Mqtt_Queue;
    int Current_Packet_Position; //Same thing as the lower variable, always 0 unless it's being used by the subcribe answer part
    //MQTTPacket Current_Packet; //This will only be different from nullptr when in use by the subcriber answer part
};

#endif /* BLACKICECENTRAL_H_3LKZ0NOB */

