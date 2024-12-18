import inet.applications.contract.IUDPApp;

simple Emergency_Service like IUDPApp
{
    parameters:
        int centralPort = default(9326);
        string centralAddress = default("server");
        string middlewareModule = default("^.middleware");
        double pollingRadius @unit(m) = default(100 m);
        double pollingInterval @unit(s) = default(1.0 s);
        double StartConfigTime @unit(s) = default(0.1 s);
        int Num_Bycicles_Cluster = 3; //How many bikes need to be within 100m of each other to be considered a cluster;

    gates:
        output udpOut;
        input udpIn;
}
