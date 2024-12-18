

#ifndef EMERGENCYSERVICERSU_H_
#define EMERGENCYSERVICERSU_H_

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

class MQTTPacket;

namespace artery
{

class Timer;

class EmergencyServiceRSU : public ItsG5BaseService
{
    public:
        EmergencyServiceRSU();
        void initialize() override;
        void receiveSignal(omnetpp::cComponent*, omnetpp::simsignal_t, omnetpp::cObject*, omnetpp::cObject*) override;
        //void handleMessage(omnetpp::cMessage*) override;
        void indicate(const vanetza::btp::DataIndication&, std::unique_ptr<vanetza::UpPacket>) override;
        void trigger() override;
        
        using ItsG5BaseService::getFacilities;
        const Timer* getTimer() const;
        std::shared_ptr<const artery::den::Memory> getMemory() const;

        ActionID_t requestActionID();
        void sendDenm(vanetza::asn1::Denm&&, vanetza::btp::DataRequestB&);
        void TestAccessMQTT();

    protected:
        int numInitStages() const override;
        void finish() override;
        void handleMessage(cMessage* msg);
        //void TestAccessMQTT();
        //void isSubscribed(simsignal_t, omnetpp::cListener);

    private:
        void fillRequest(vanetza::btp::DataRequestB&);
        void initUseCases();
        void ProcessSOS(MQTTPacket& SubscribePacket);
        

        inet::UDPSocket EmergSocket;
        int EmergPort;
        //cIListener *signal_listener;

        const Timer* mTimer;
        uint16_t mSequenceNumber;
        std::shared_ptr<artery::den::Memory> mMemory;
        std::list<artery::den::AdaptedUseCase*> mUseCases;
        omnetpp::cListener *Listener;
};

} // namespace artery

#endif
