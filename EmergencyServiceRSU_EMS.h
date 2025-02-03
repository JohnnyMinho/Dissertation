

#ifndef EmergencyServiceRSU_EMS_H_
#define EmergencyServiceRSU_EMS_H_

#include "artery/application/den/Memory.h"
#include "artery/application/den/AdaptedUseCase.h"
#include "artery/application/ItsG5BaseService.h"
#include <inet/transportlayer/contract/udp/UDPSocket.h>
#include <omnetpp/clistener.h>
#include "artery/application/ItsG5BaseService.h"
#include <omnetpp/csimplemodule.h>
#include <vanetza/asn1/denm.hpp>
#include <vanetza/btp/data_indication.hpp>
#include <vanetza/btp/data_request.hpp>
#include <cstdint>
#include <list>
#include <memory>
#include "traci/Angle.h"
#include "traci/Boundary.h"
#include "traci/NodeManager.h"
#include "traci/Listener.h"
#include "traci/Position.h"
#include "traci/API.h"
#include "traci/SubscriptionManager.h"
#include "artery/application/AmbulanceSystem.h"

class MQTTPacket;

namespace artery
{

class Timer;

class EmergencyServiceRSU_EMS : public ItsG5BaseService, public traci::Listener
{
    public:
        EmergencyServiceRSU_EMS();
        void initialize() override;
        void receiveSignal(omnetpp::cComponent*, omnetpp::simsignal_t, omnetpp::cObject*, omnetpp::cObject*) override;
        std::string CheckAvailableAmbulance(MQTTPacket&,bool);
        //void handleMessage(omnetpp::cMessage*) override;
        void indicate(const vanetza::btp::DataIndication&, std::unique_ptr<vanetza::UpPacket>) override;
        void trigger() override;
        
        using ItsG5BaseService::getFacilities;
        const Timer* getTimer() const;
        std::shared_ptr<const artery::den::Memory> getMemory() const;

        ActionID_t requestActionID();
        void sendDenm(vanetza::asn1::Denm&&, vanetza::btp::DataRequestB&);
        void sendAmbulanceRequest(); //vanetza::btp::DataRequestB& request
        void sendAmbulanceRequest_Temp(std::vector<std::string> Route_Steps_Temp,std::string AmbulanceID);
        void TestAccessMQTT();

    protected:
        int numInitStages() const override;
        void finish() override;
        void handleMessage(cMessage* msg);
        void ConfigNode();
        //void fillRequest()
        //void sen
        vanetza::btp::DataRequestB createRequest();

        void AddAwaitingSOSRequest(MQTTPacket&);
        void CheckAndAnsSosRequest();
        std::vector<char*> Location_And_ID_Extractor(const char * );
        //void TestAccessMQTT();
        //void isSubscribed(simsignal_t, omnetpp::cListener);

    private:
        
        void fillRequest(vanetza::btp::DataRequestB&);
        void initUseCases();
        void ProcessSOS(MQTTPacket& SubscribePacket);
        
        inet::UDPSocket AmbulanceSocket;
        int AmbulancePort;

        const Timer* mTimer;
        uint16_t mSequenceNumber;
        std::shared_ptr<artery::den::Memory> mMemory;
        std::list<artery::den::AdaptedUseCase*> mUseCases;
        omnetpp::cListener *Listener;
        omnetpp::cMessage* StartConfigTrigger = nullptr;
        omnetpp::simtime_t StartConfigTime;
        omnetpp::cMessage* StartCheckingSOS = nullptr;
        omnetpp::simtime_t CheckSOSTimer;
        std::list<MQTTPacket*> AwaitingSOS;


        AmbulanceSystem* Ambulance_To_Request;

        int SOS_to_Ambulance;
};

} // namespace artery

#endif
