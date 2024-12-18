packet BlackIceReport
{
    double positionX;
    double positionY;
    double speed;
    simtime_t time;
}

packet LocationPing
{
    string VehicleId;
    double positionX;
    double positionY;
    simtime_t time;
}

packet BlackIceQuery
{
    double positionX;
    double positionY;
    double radius;
}

packet LocationPingQuery
{
    double positionX;
    double positionY;
    double radius;
}

packet BlackIceResponse
{
    int warnings;
}

packet LocationPingResponse
{
    int pings;
}

packet MQTTPacket
{
    int cmd;
    bool retain;
    int qos;
    bool duplicated;
    int length;
    string topic;
    string payload;
}

packet MQTTSubscribe
{
    int placeholder;
}

packet MQTTPublish
{
    int placegolder2;
}