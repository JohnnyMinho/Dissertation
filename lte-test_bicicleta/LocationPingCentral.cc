#include "LocationPingCentral.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include <inet/networklayer/common/L3AddressResolver.h>
#include <algorithm>
#include <inet/transportlayer/contract/udp/UDPControlInfo.h>
#include <inet/common/ModuleAccess.h>
#include <traci/BasicNodeManager.h>
#include "traci/Core.h"
#include <traci/API.h>

using namespace omnetpp;

Define_Module(LocationPingCentral)

static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT  = cComponent::registerSignal("SOS_Sent");

LocationPingCentral::~LocationPingCentral()
{
}

void LocationPingCentral::initialize()
{
    
    StartConfigTrigger = new cMessage("ConfiguringNode");

    reportPort = par("reportPort");
    reportSocket.setOutputGate(gate("udpOut"));
    reportSocket.bind(inet::L3Address(), reportPort);

    queryPort = par("queryPort");
    querySocket.setOutputGate(gate("udpOut"));
    querySocket.bind(inet::L3Address(), queryPort);

    EmergPort = par("EmergPort");

    SubAnsMchTrigger = new cMessage("AnswerMachineSubscribe");
    StartConfigTime = par("StartConfigTime");

    numReceivedWarnings = 0;
    numReceivedQueries = 0;
    numReceivedMQTTPublish = 0;
    numReceivedMQTTSubscribe = 0;
    WATCH(numReceivedWarnings);
    WATCH(numReceivedQueries);
    WATCH(numReceivedMQTTPublish);
    WATCH(numReceivedMQTTSubscribe);

    scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);
}

void LocationPingCentral::finish()
{
    reportSocket.close();
    querySocket.close();
    EmergSocket.close();

    recordScalar("numReceivedWarnings", numReceivedWarnings);
    recordScalar("numReceivedQueries", numReceivedQueries);
    recordScalar("numReceivedMQTTPublish", numReceivedMQTTPublish);
}

void LocationPingCentral::handleMessage(cMessage* msg)
{
    EV_INFO << "Message kind: " << msg->getKind() << " | Message class: " << msg->getClassName() << "\n";
    if (msg->isSelfMessage()) {
        AnsMachine();
        if(msg == StartConfigTrigger){
            ConfigNode();
        }
    }
    else if (msg->getKind() == inet::UDP_I_DATA) {
        processPacket(PK(msg));
    } else if (msg->getKind() == inet::UDP_I_ERROR) {
        EV_ERROR << "UDP error occurred"<< "\n";
        delete msg;
    } else {
        EV_INFO << "hello" << "\n";
        //throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
}

void LocationPingCentral::processPacket(cPacket* pkt)
{
    auto ctrl = pkt->getControlInfo();
    const char* MQTT_Classname_Checker = pkt->getName();
    if (auto udp = dynamic_cast<inet::UDPDataIndication*>(ctrl)) {
        if (udp->getDestPort() == reportPort) {
            if(strcmp(MQTT_Classname_Checker, "Publish") == 0){
                EV_INFO << "Received a Publish"  << "\n";
                processMQTTPacket(*check_and_cast<MQTTPacket*>(pkt));
            }
        } else if (udp->getDestPort() == queryPort) {
            //processQuery(*check_and_cast<BlackIceQuery*>(pkt), udp->getSrcAddr(), udp->getSrcPort());
            if(strcmp(MQTT_Classname_Checker, "Subscribe") == 0){
                EV_INFO << "Received a Subscribe"  << "\n";
                processMQTTSubscribe(*check_and_cast<MQTTPacket*>(pkt), udp->getSrcAddr(), udp->getSrcPort());
            }else{
                processLocationPingQuery(*check_and_cast<LocationPingQuery*>(pkt), udp->getSrcAddr(), udp->getSrcPort());
            }
        } else if (udp->getDestPort() == EmergPort){
                EV_WARN << "Contacted by Emergency Station"  << "\n";
        } else {
            throw cRuntimeError("Unknown UDP destination port %d", udp->getDestPort()); //THIS IS WHAT WAS PRODUCING THE ERROR REMOVE THIS OR ADD THE EMERGPORT DIPSHIT ON BOTH SIDES
        }
    }
    delete pkt;
}

void LocationPingCentral::processReport(BlackIceReport& report)
{
    ++numReceivedWarnings;
    reports.push_back(report);
}

void LocationPingCentral::processLocationPing(LocationPing& ping)
{
    ++numReceivedWarnings;
    pings.push_back(ping);

}

void LocationPingCentral::processMQTTPacket(MQTTPacket& MQTTP)
{
    ++numReceivedMQTTPublish;
    EV_INFO << "Publishes to the server done: " << numReceivedMQTTPublish << "\n";
    EV_INFO << "Publish Topic: " << MQTTP.getTopic() << "\n";
    bool operation_done = false;
    std::list<MQTTPacket>::iterator topic_finder = std::find_if(MqttPackets.begin(), MqttPackets.end(),
                                                      [&MQTTP](const MQTTPacket& packet) {
                                                            return strcmp(packet.getTopic(), MQTTP.getTopic()) == 0;;  // Compare topics
                                                      });
    if(strcmp(MQTTP.getTopic(),"SOS") == 0){
        //This is just a placeholder for a better function, SOS might be to generalized and not a good way to handle multiple SOS if that might be the case
        //We need to look into a use-case where we are able to handle a big cluster sized accident (Maybe make a way to if a SOS is received we keep checking if there are more coming from near-by positions)
        EV_WARN << "SOS" << "\n";
        char text[128];
        sprintf(text,"SOS RECEIVED-Event number: %lld", getSimulation()->getEventNumber());
        getSimulation()->getActiveEnvir()->alert(text);
        char testbuffer[100];
        /*auto response = new MQTTPacket("Test");
        //response->setName(MQTTPacket_Obj_Name);
        response->setCmd(0); //0-> Publish | 1-> Subscribe
        response->setRetain(0);
        response->setQos(0);
        response->setDuplicated(0);
        response->setLength(256); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
        response->setTopic("Hello");
        EmergSocket.send(response);*/
        //SendSOS(MQTTP.getPayload(),MQTTP.getTopic());

        SendSOS(MQTTP);
        //From here we should try to contact the DEN service to send the DENM message to the Emg Station 
        //In accordance to what me and the concelours agreed the following should happen:

        //The Emergency Station RSU receives a MQTT Publish with the SOS information and thus he must inform an available ambulance of the accident and dispatch it to the accident location
        //After that, the server should attempt to research and find out which RSU is the closest to the accident and then send a MQTT publish to it to inform it that he should start
        //Producing DENM messages to all vehicles in it's range. This way, the other vehicles should be able to avoid the location reducing the chance of jams and slowing down of emergency services
    }else{
        if(MqttPackets.size() == 0){
            EV_INFO << "Topic Table empty: " << MQTTP.getTopic() << "\n";
            MqttPackets.push_front(MQTTP); //We don't have any topics
            operation_done = true;
        }else if (topic_finder != MqttPackets.end()) {
            EV_INFO << "Updating Topic: " << MQTTP.getTopic() << "\n";
            topic_finder = MqttPackets.erase(topic_finder);
            MqttPackets.insert(topic_finder,MQTTP);
            operation_done = true;
        }else{
            EV_INFO << "New Topic: " << MQTTP.getTopic() << "\n";
            MqttPackets.push_front(MQTTP); //We don't have any topics
        }

        
    }
    

    //MqttPackets.push_back(MQTTP);
}

void LocationPingCentral::processQuery(BlackIceQuery& query, const inet::L3Address& addr, int port)
{
    ++numReceivedQueries;
    int warnings = 0;
    for (auto& report : reports) {
        double dx = query.getPositionX() - report.getPositionX();
        double dy = query.getPositionY() - report.getPositionY();
        if (dx * dx + dy * dy < query.getRadius() * query.getRadius()) {
            ++warnings;
        }
    }

    auto response = new BlackIceResponse("black ice response");
    response->setWarnings(warnings);
    querySocket.sendTo(response, addr, port);
}

void LocationPingCentral::processLocationPingQuery(LocationPingQuery& query, const inet::L3Address& addr, int port)
{
    ++numReceivedQueries;
    int num_pings = 0;
    for (auto& ping : pings) {
        //double dx = query.getPositionX() - report.getPositionX();
        //double dy = query.getPositionY() - report.getPositionY();
        ++num_pings;
    }

    auto response = new LocationPingResponse("Location Ping Query response");
    response->setPings(num_pings);
    querySocket.sendTo(response, addr, 9321);
}

void LocationPingCentral::processMQTTSubscribe(MQTTPacket& SubscribePacket, const inet::L3Address& addr, int port)
{
    ++numReceivedMQTTSubscribe;
    int testing = 0;
    int all_topics_sub = 0;
    Current_Packet_Position = 0;
    //auto response = new MQTTPacket("Publish");

    current_sub_addr = addr;
    current_sub_port = port;

    char previous_topic[100] = "";
    if(strcmp(SubscribePacket.getTopic(), "#") == 0){
        //scheduleAt(simTime() + 0.1, SubAnsMchTrigger);
    }
    for (auto& Topic : MqttPackets) {
        if(strcmp(SubscribePacket.getTopic(), "#") == 0){
            //break;
            char MQTTPacket_Obj_Name[100] = ""; //Since there might be some conflicts due to the sending of packets with the same object name we should make it dynamic
            strcat(MQTTPacket_Obj_Name,"Publish");
            strcat(MQTTPacket_Obj_Name,Topic.getTopic());
            MQTTPacket_Obj_Name[strlen(MQTTPacket_Obj_Name)+1] = '\0';
            EV_INFO << "Preparing All Topic Answer: "<< MQTTPacket_Obj_Name << "\n"; //We need to do a recurrent schedule to give time to the socker to completely send the packet
            auto response = new MQTTPacket(MQTTPacket_Obj_Name);
            //response->setName(MQTTPacket_Obj_Name);
            response->setCmd(Topic.getCmd()); //0-> Publish | 1-> Subscribe
            response->setRetain(Topic.getRetain());
            response->setQos(Topic.getQos());
            response->setDuplicated(Topic.getDuplicated());
            response->setLength(Topic.getLength()); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
            response->setPayload(Topic.getPayload());
            response->setTopic((Topic.getTopic()));
            querySocket.sendTo(response, addr, port);
            //++Current_Packet_Position;
        }
        if(strcmp(SubscribePacket.getTopic(), Topic.getTopic()) == 0){
            auto response = new MQTTPacket("Publish");
            response = &Topic; //This is a lot easier since we will only send one packet
            EV_INFO << "Topic sent: " << testing << "\n";
            querySocket.sendTo(response, addr, 9321);
        }
        testing++;
    }
}

void LocationPingCentral::AnsMachine(){
    int counter = 0;
    for (auto& Topic : MqttPackets){
            if(counter == Current_Packet_Position){
                char MQTTPacket_Obj_Name[100] = ""; //Since there might be some conflicts due to the sending of packets with the same object name we should make it dynamic
                strcat(MQTTPacket_Obj_Name,"Publish");
                strcat(MQTTPacket_Obj_Name,Topic.getTopic());
                MQTTPacket_Obj_Name[strlen(MQTTPacket_Obj_Name)+1] = '\0';
                EV_INFO << "Preparing All Topic Answer: "<< MQTTPacket_Obj_Name << "\n"; //We need to do a recurrent schedule to give time to the socker to completely send the packet
                auto response = new MQTTPacket(MQTTPacket_Obj_Name);
                //response->setName(MQTTPacket_Obj_Name);
                response->setCmd(Topic.getCmd()); //0-> Publish | 1-> Subscribe
                response->setRetain(Topic.getRetain());
                response->setQos(Topic.getQos());
                response->setDuplicated(Topic.getDuplicated());
                response->setLength(Topic.getLength()); //256 byte messages format-> "NodeID|PosX|PosY|Timestamp"
                response->setTopic((Topic.getTopic()));
                querySocket.sendTo(response, current_sub_addr, current_sub_port);
                ++Current_Packet_Position;                
                scheduleAt(simTime() + 0.1, SubAnsMchTrigger);
                break;
            }
            if(counter == MqttPackets.size()){
                break;
            }
        counter++;
    }
}

void LocationPingCentral::SendSOS(MQTTPacket& SOS_Packet)
{
    Enter_Method("Treating SOS");
    auto publish = new MQTTPacket("Publish");
    publish->setCmd(1); 
    publish->setRetain(true);
    publish->setQos(0);
    publish->setDuplicated(false);
    publish->setLength(256); 
    publish->setTopic("SOS");
    publish->setPayload(SOS_Packet.getPayload());
    EmergSocket.send(publish);
}

void LocationPingCentral::ConfigNode()
{ 

    EmergSocket.setOutputGate(gate("udpOut"));
    EmergSocket.bind(EmergPort);
    EmergSocket.setBroadcast(true);
    auto centralAddress = inet::L3AddressResolver().resolve(par("centralAddress"));
    EmergSocket.connect(centralAddress, EmergPort);
}