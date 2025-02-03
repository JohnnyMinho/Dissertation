package artery.application;

simple EmergencyServiceRSU_EMS like ItsG5Service
{
    parameters:
        @class(EmergencyServiceRSU_EMS);
        @signal[DenmSent](type=artery::DenmObject);
        @signal[DenmReceived](type=artery::DenmObject);
        int centralPort = default(9326);
        int AmbulanceSystemPort = default (9400);
        string centralAddress = default("server");
        string middlewareModule = default("^.middleware") ;
        string serviceModule = default("^.middleware.EmergencyServiceRSU");
        double StartConfigTime @unit(s) = default(0.1 s);
        double CheckSOSTimer @unit(s) = default(0.5 s);
        string traciCoreModule = default("World.traci.core"); //This is used to access the TraCI manager core on the simulation
        string AmbulanceModule @mutable = default("World.node[1].middleware.AmbulanceSystem");

    gates:
        output udpOut;
        input udpIn;

}
