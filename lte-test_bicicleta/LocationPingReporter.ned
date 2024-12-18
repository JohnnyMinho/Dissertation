import inet.applications.contract.IUDPApp;

simple LocationPingReporter like IUDPApp
{
    parameters:
        int centralPort = default(9320);
        string centralAddress;
        string middlewareModule;
        double PingInterval @unit(s) = default(1.0 s);

    gates:
        output udpOut;
        input udpIn;
}
