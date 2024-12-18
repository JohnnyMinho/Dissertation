import inet.applications.contract.IUDPApp;

simple LocationPingCentral like IUDPApp
{
    parameters:
        int reportPort = default(9320);
        int queryPort = default(9321);
        int EmergPort = default(9322);
        double StartConfigTime @unit(s) = default(0.1 s);
        string centralAddress = default("Myrsu[1]");

    gates:
        output udpOut;
        input udpIn;
}
