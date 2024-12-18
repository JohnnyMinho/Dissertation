#ifndef LOCATIONPINGREPORTER_H_
#define LOCATIONPINGREPORTER_H_

#include <inet/transportlayer/contract/udp/UDPSocket.h>
//#include "artery/application/ItsG5BaseService.h"
#include <omnetpp/clistener.h>
//#include "artery/application/Middleware.h"
#include <omnetpp/csimplemodule.h>
//#include "triggerHandler.h"
#include <omnetpp/simtime.h>
// forward declaration

namespace traci { class VehicleController; }

class Timer;

class LocationPingReporter : public omnetpp::cSimpleModule, public omnetpp::cListener
{
public:
    ~LocationPingReporter();
protected:
    int numInitStages() const override;
    void initialize(int stage) override;
    void finish() override;
    void receiveSignal(omnetpp::cComponent*, omnetpp::simsignal_t, omnetpp::cObject*, omnetpp::cObject*) override;
    void handleMessage(omnetpp::cMessage*) override;
    //void trigger() override;

private:
    void checkTriggeringConditions(const omnetpp::SimTime&);
    void sendReport();
    void sendPing();
    void sendSOS();

    const Timer* mTimer = nullptr; //We will adapt the reporter code to send a message after 1 minute has passed since the last message

    omnetpp::SimTime mLastPingTimestamp; //Timestamp of the last ping sent
    omnetpp::simtime_t PingInterval; //We will use this variable to define the time until we trigger a new ping
    omnetpp::cMessage* PingTrigger = nullptr;

    inet::UDPSocket socket;
    
    traci::VehicleController* vehicleController = nullptr;
    unsigned tractionLosses;
    bool SOS_Status;
    bool Already_got_fall_sig;

    int test;
};

#endif /* BLACKICEREPORTER_H_COLABPR9 */

