import inet.applications.contract.IUDPApp;

simple Emergency_Service_MQTT like IUDPApp
{
    parameters:
        @signal[SOS_To_DEN](type=long);  // Declare the signal in the NED file
        @signal[SOS_Sent](type=long);
        int centralPort = default(9320);
        int EmergPort = default(9322);
        string centralAddress = default("server");
        string middlewareModule = default("^.middleware");
        string serviceModule = default("^.middleware.EmergencyServiceRSU"); //Desta maneira estamos a formar uma "conexão" direta ao serviço, podendo chamar então os métodos do mesmo
        double pollingRadius @unit(m) = default(100 m);
        double pollingInterval @unit(s) = default(1.0 s);
        double StartConfigTime @unit(s) = default(0.1 s);
        int Num_Bycicles_Cluster = 3; //How many bikes need to be within 100m of each other to be considered a cluster;
        string traciCoreModule = default("World.traci.core"); //This is used to access the TraCI manager core on the simulation

    gates:
        output udpOut;
        input udpIn;
}
