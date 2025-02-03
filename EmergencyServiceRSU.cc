
#include "artery/application/Asn1PacketVisitor.h"
#include "artery/application/DenmObject.h"
#include "artery/application/Timer.h"
#include "artery/application/StoryboardSignal.h"
#include "artery/application/VehicleDataProvider.h"
#include "artery/utility/FilterRules.h"
#include <omnetpp/checkandcast.h>
#include <omnetpp/ccomponenttype.h>
#include <omnetpp/cxmlelement.h>
#include <vanetza/asn1/denm.hpp>
#include <vanetza/btp/ports.hpp>

#include "EmergencyServiceRSU.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/utility/PointerCheck.h"
#include "artery/utility/InitStages.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>
#include <traci/sumo/libsumo/TraCIConstants.h>
#include "veins/modules/mobility/traci/TraCICommandInterface.h"
#include "veins/modules/mobility/traci/TraCIScenarioManager.h"

using namespace omnetpp;

namespace artery
{

Define_Module(EmergencyServiceRSU)

static const simsignal_t denmReceivedSignal = cComponent::registerSignal("DenmReceived");
static const simsignal_t denmSentSignal = cComponent::registerSignal("DenmSent");
//static const simsignal_t storyboardSignal = cComponent::registerSignal("StoryboardSignal");
static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT = cComponent::registerSignal("SOS_Sent");

EmergencyServiceRSU::EmergencyServiceRSU() :
    mTimer(nullptr), mSequenceNumber(0)
{
}

void EmergencyServiceRSU::initialize()
{
    ItsG5BaseService::initialize();
    mTimer = &getFacilities().get_const<Timer>();
    mMemory.reset(new artery::den::Memory(*mTimer));
    
    initUseCases();
    subscribe(SharedSignal_MQTT_To_Den);
    
    cModule* currentModule = getParentModule();
    StartConfigTrigger = new cMessage("ConfiguringNode");
    
    scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);
} 

void EmergencyServiceRSU::initUseCases()
{
    omnetpp::cXMLElement* useCases = par("useCases").xmlValue();
    for (omnetpp::cXMLElement* useCaseElement : useCases->getChildrenByTagName("usecase")) {
        omnetpp::cModuleType* useCaseType = omnetpp::cModuleType::get(useCaseElement->getAttribute("type"));
        omnetpp::cXMLElement* filter = useCaseElement->getFirstChildWithTag("filters");

        bool useCaseApplicable = true;
        if (filter) {
            artery::FilterRules rules(getRNG(0), getFacilities().get_const<artery::Identity>());
            useCaseApplicable = rules.applyFilterConfig(*filter); 
        }

        if (useCaseApplicable) {
            const char* useCaseName = useCaseElement->getAttribute("name") ?
                useCaseElement->getAttribute("name") : useCaseType->getName();
            omnetpp::cModule* module = useCaseType->create(useCaseName, this);
            // do not call initialize here! omnetpp::cModule initializes submodules on its own!
            module->buildInside();
            den::AdaptedUseCase* useCase = dynamic_cast<den::AdaptedUseCase*>(module);
            if (useCase) {
                mUseCases.push_front(useCase);
            }
        }
    }
}

void EmergencyServiceRSU::finish()
{
    //AmbulanceSocket.close();
}

void EmergencyServiceRSU::receiveSignal(cComponent*, simsignal_t signal, cObject* obj, cObject*)
{
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"Taga");
    getSimulation()->getActiveEnvir()->alert(text1);
    /*if (signal == storyboardSignal) {
        StoryboardSignal* storyboardSignalObj = check_and_cast<StoryboardSignal*>(obj);
        for (auto use_case : mUseCases) {
            use_case->handleStoryboardTrigger(*storyboardSignalObj);
        }
    }*/
    if (signal == SharedSignal_MQTT_To_Den){
        auto manager = veins::TraCIScenarioManagerAccess().get();
        auto traci = manager->getCommandInterface();
        EV_WARN << "SOS FROM MQTT PART" << "\n" ;
        char text[128];
        strcat(text,"Taga");
        getSimulation()->getActiveEnvir()->alert(text);
        //Verificar quantas ambulâncias estão disponiveís
        /*auto parkedVehicles = traci->parkingArea.getVehicleIDs("parking_1");
        if (!parkedVehicles.empty()) {
            std::string vehicleId = parkedVehicles[0]; // Get the first vehicle
        }*/
    }
}

void EmergencyServiceRSU::indicate(const vanetza::btp::DataIndication& indication, std::unique_ptr<vanetza::UpPacket> packet)
{
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"Taga");
    getSimulation()->getActiveEnvir()->alert(text1);
    Asn1PacketVisitor<vanetza::asn1::Denm> visitor;
    const vanetza::asn1::Denm* denm = boost::apply_visitor(visitor, *packet);
    const auto egoStationID = this->getParentModule()->getId();;
    //const auto egoStationID = getFacilities().get_const<VehicleDataProvider>().station_id();

    if (denm && (*denm)->header.stationID != egoStationID) {
        DenmObject obj = visitor.shared_wrapper;
        mMemory->received(obj);
        emit(denmReceivedSignal, &obj);

        for (auto use_case : mUseCases) {
            use_case->indicate(obj);
        }
    }
}

void EmergencyServiceRSU::trigger()
{
    mMemory->drop();

    for (auto use_case : mUseCases) {
        use_case->check();
    }
}

ActionID_t EmergencyServiceRSU::requestActionID()
{
    ActionID_t id;
    //id.originatingStationID = getFacilities().get_const<VehicleDataProvider>().station_id();
    id.sequenceNumber = ++mSequenceNumber;
    return id;
}

const Timer* EmergencyServiceRSU::getTimer() const
{
    return mTimer;
}

std::shared_ptr<const den::Memory> EmergencyServiceRSU::getMemory() const
{
    return mMemory;
}

void EmergencyServiceRSU::sendDenm(vanetza::asn1::Denm&& message, vanetza::btp::DataRequestB& request)
{
    fillRequest(request);
    DenmObject obj { std::move(message) };
    emit(denmSentSignal, &obj);

    using namespace vanetza;
    using DenmConvertible = vanetza::convertible::byte_buffer_impl<vanetza::asn1::Denm>;
    std::unique_ptr<geonet::DownPacket> payload { new geonet::DownPacket };
    std::unique_ptr<vanetza::convertible::byte_buffer> denm { new DenmConvertible { obj.shared_ptr() } };
    payload->layer(OsiLayer::Application) = vanetza::ByteBufferConvertible { std::move(denm) };
    this->request(request, std::move(payload));
}

void EmergencyServiceRSU::fillRequest(vanetza::btp::DataRequestB& request)
{
    using namespace vanetza;

    request.destination_port = btp::ports::DENM;
    request.gn.its_aid = aid::DEN;
    request.gn.transport_type = geonet::TransportType::GBC;
    request.gn.communication_profile = geonet::CommunicationProfile::ITS_G5;
}

int EmergencyServiceRSU::numInitStages() const
{
    return artery::InitStages::Total;
}

void EmergencyServiceRSU::ProcessSOS(MQTTPacket& SubscribePacket){
    //We need to extract the position of the bycicle and then create and send a emergency vehicle towards it
    
}



/*void EmergencyServiceRSU::processPacket(cPacket* pkt)
{
    auto ctrl = pkt->getControlInfo();
    const char* MQTT_Classname_Checker = pkt->getName();
    if (auto udp = dynamic_cast<inet::UDPDataIndication*>(ctrl)) {
        
        if (udp->getDestPort() == EmergPort){
            EV_WARN << "SOS occured, contacted by Server"  << "\n";
            emit(SharedSignal_MQTT_To_Den, pkt);  // Send data via signal
        } else {
            //throw cRuntimeError("Unknown UDP destination port %d", udp->getDestPort());
        }
    }
}*/

void EmergencyServiceRSU::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        if(msg == StartConfigTrigger){
            ConfigNode();
        }
    }
}

void EmergencyServiceRSU::TestAccessMQTT(){
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"CONNECTION SUCEFULL YAY");
    getSimulation()->getActiveEnvir()->alert(text1);
}

void EmergencyServiceRSU::ConfigNode()
{
    //AmbulancePort = par("AmbulanceSystemPort");
    //AmbulanceSocket.setOutputGate(gate("udpOut"));
    //AmbulanceSocket.bind(inet::L3Address(), AmbulancePort);
}


} // namespace artery


