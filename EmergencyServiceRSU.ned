package artery.application;

simple EmergencyServiceRSU like ItsG5Service
{
    parameters:
        @class(EmergencyServiceRSU);
        @signal[SOS_Sent](type=long);
        @signal[SOS_To_DEN](type=long);
        @signal[DenmSent](type=artery::DenmObject);
        @signal[DenmReceived](type=artery::DenmObject);
        int centralPort = default(9326);
        int EmergPort = default(9322);
        string centralAddress = default("server");
        string middlewareModule = default("^.middleware") ;
        string serviceModule = default("^.middleware.EmergencyServiceRSU");
        double pollingRadius @unit(m) = default(100 m);
        double pollingInterval @unit(s) = default(1.0 s);
        double StartConfigTime @unit(s) = default(0.1 s);
        int Num_Bycicles_Cluster = 3; //How many bikes need to be within 100m of each other to be considered a cluster;

        xml useCases;
        
    gates:
        output udpOut;
        input udpIn;
}
