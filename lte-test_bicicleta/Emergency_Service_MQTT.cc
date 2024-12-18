#include "Emergency_Service_MQTT.h"
#include "artery/application/EmergencyServiceRSU.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/application/Middleware.h"
#include "artery/traci/VehicleController.h"
#include "artery/utility/InitStages.h"
#include "artery/utility/PointerCheck.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>
#include "traci/sumo/utils/traci/TraCIAPI.h"
#include "traci/Core.h"
#include <traci/API.h>

//The emergency service must be able to communicate trough WLAN and PPP / Ethernet so it will be a ITS-G5 service
//We need to be able to generate a vehicle as soon as the Station RSU receives a signal
//For simplicity sake we will be assuming that the server has a direct connection to the emergency station (initially)

using namespace omnetpp;

//Since we receive the SOS warnings in the MQTT UdpApp we use signals between the two components as a way to warn this adapted DEN service 
//Using signals is a somewhat complex way since we had some overhead but it allows us to keep the udpapp and the service somewhat uncoupled allowing for
//Bigger freedoms and less hassel when dealing with changes in any of the two

static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT = cComponent::registerSignal("SOS_Sent"); //This is a way to tell the MQTT server that the emergency was answered and we don't need to keep the slot occupied

Define_Module(Emergency_Service_MQTT)

Emergency_Service_MQTT::~Emergency_Service_MQTT()
{
    cancelAndDelete(PingPollingTrigger);
    cancelAndDelete(StartConfigTrigger);
}

void Emergency_Service_MQTT::initialize(int stage)
{

    if (stage == artery::InitStages::Prepare) {
        
        StartConfigTrigger = new cMessage("ConfiguringNode");
        

    } else if (stage == artery::InitStages::Self) {
        //subscribe(SharedSignal_MQTT_To_Den, this);
        EmergPort = par("EmergPort");
        EmergSocket.setOutputGate(gate("udpOut"));
        EmergSocket.bind(inet::L3Address(), EmergPort);
        
        scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);
        SubAnsMchTrigger = new cMessage("AnswerMachineSubscribe");
        Ambulance_in_use = false;
        cModule* currentModule = getParentModule();
        
        Emerg_Service = inet::getModuleFromPar<artery::EmergencyServiceRSU>(par("serviceModule"), this);
        Emerg_Service->TestAccessMQTT();
    }

    if(hasListeners(SharedSignal_MQTT_To_Den)){
        char text1[128];
        text1[128] = '\0';
        strcat(text1,"Taga1");
        getSimulation()->getActiveEnvir()->alert(text1);
    }
}

int Emergency_Service_MQTT::numInitStages() const
{
    return artery::InitStages::Total;
}

void Emergency_Service_MQTT::finish()
{
    socket.close();
    EmergSocket.close();
    warningsVector.record(numWarningsCentral);
	warningStats.collect(numWarningsCentral);
    recordScalar("numWarningsCentral", numWarningsCentral);
    recordScalar("numMQTTPacketsReceivedFromCentral", numMQTTPacketsReceived);
    recordScalar("clusters_detected_warner",clusters_detected);
    recordScalar("Positions Updated:",numPositionsUpdated);
}

void Emergency_Service_MQTT::handleMessage(cMessage* msg)
{
    EV_WARN << "Message Received" << "\n";
    EV_INFO << "Message kind: " << msg->getKind() << " | Message class: " << msg->getClassName() << "\n";
    if (msg->isSelfMessage()) {
        //AnsMachine();
        if(msg == StartConfigTrigger){
            ConfigNode();
        }
    }
    else if (msg->getKind() == inet::UDP_I_DATA) {
        processPacket(PK(msg));
    }else {
        throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
    delete msg;
}

void Emergency_Service_MQTT::processPacket(cPacket* pkt)
{
    auto ctrl = pkt->getControlInfo();
    const char* MQTT_Classname_Checker = pkt->getName();
    if (auto udp = dynamic_cast<inet::UDPDataIndication*>(ctrl)) {
        
        if (udp->getDestPort() == EmergPort){
            EV_WARN << "SOS occured, contacted by Server"  << "\n";
            long test = 0;
            if(hasListeners(SharedSignal_MQTT_To_Den)){
                char text1[128];
                text1[128] = '\0';
                strcat(text1,"Taga1");
                getSimulation()->getActiveEnvir()->alert(text1);
            }
            emit(SharedSignal_MQTT_To_Den, test);  // Send data via signal
            processMQTTPacket(*check_and_cast<MQTTPacket*>(pkt));

        } else {
            //throw cRuntimeError("Unknown UDP destination port %d", udp->getDestPort());
        }
    }
}

void Emergency_Service_MQTT::ConfigNode()
{

    EV_INFO << getParentModule()->getFullName() << "\n";
    EV_INFO << inet::L3Address() << "\n";

    char testbuffer[100];
    auto response = new MQTTPacket("Test");

    auto mw = inet::getModuleFromPar<artery::Middleware>(par("middlewareModule"), this);

    //vehicleController = artery::notNullPtr(mw->getFacilities().get_mutable_ptr<traci::VehicleController>());
}

void Emergency_Service_MQTT::processMQTTPacket(MQTTPacket& MQTTP)
{
    ++numReceivedMQTTPublish;
    EV_INFO << "Publishes to the server done: " << numReceivedMQTTPublish << "\n";
    EV_INFO << "Publish Topic: " << MQTTP.getTopic() << "\n";
    bool operation_done = false;
    std::list<MQTTPacket>::iterator topic_finder = std::find_if(MqttPackets.begin(), MqttPackets.end(),
                                                      [&MQTTP](const MQTTPacket& packet) {
                                                            return strcmp(packet.getTopic(), MQTTP.getTopic()) == 0;;  // Compare topics
                                                      });
    EV_INFO << MQTTP.getTopic() << "\n";
    if(strcmp(MQTTP.getTopic(),"SOS") == 0){
        AmbulanceDispatch(MQTTP);
    }else{
        if(MqttPackets.size() == 0){
            EV_INFO << "Topic Table empty: " << MQTTP.getTopic() << "\n";
            MqttPackets.push_front(MQTTP); //We don't have any topics
            operation_done = true;
        }else if (topic_finder != MqttPackets.end()) {
            EV_INFO << "Updating Topic: " << MQTTP.getTopic() << "\n";
            topic_finder = MqttPackets.erase(topic_finder);
            MqttPackets.insert(topic_finder,MQTTP);
            operation_done = true;
        }else{
            EV_INFO << "New Topic: " << MQTTP.getTopic() << "\n";
            MqttPackets.push_front(MQTTP); //We don't have any topics
        }
    }
}

void Emergency_Service_MQTT::ProcessSOS(MQTTPacket& SubscribePacket){
    //We need to extract the position of the bycicle and then create and send a emergency vehicle towards it
    
}

//This function is going to fetch the position of the vehicle which sent the SOS, then we are going to use the findRoute() function in the TraCIApi to get a route to the accident based on the best effort metric
//This can be changed by us designing our own function or just giving the ambulance the edge to where it needs to go and then using an algorithm on the ambulance itself.
//In the broad sense of location we can't turn the lat and long pos from the simulator into a location he can interpert, we can however use the vehicle ID to get the edge where it currently is
//And then use the setStop from the API to establish the place where the ambulance should try to stop to assist the fallen cyclist.
//In a real life scenario the lat and long would probably be more than enough for the emergency services to assist the injured cyclist.
std::vector<char*> Emergency_Service_MQTT::Location_And_ID_Extractor(const char * Sos_Packet_content){
    char buffer[256];  
    std::vector<char *> Result_Storage;
    strncpy(buffer, Sos_Packet_content, sizeof(buffer));
    char* payload_copy = strtok(buffer,"|");
    for(int tokens_done = 0; payload_copy != NULL; tokens_done++){
        if(tokens_done == 0){
            //strcat(CyclistIdBuffer, payload_copy);
            Result_Storage.push_back(payload_copy);
        }
        if(tokens_done == 1){
            //posX = atof(payload_copy);
            Result_Storage.push_back(payload_copy);
        }
        if(tokens_done == 2){
            //posY = atof(payload_copy);
            Result_Storage.push_back(payload_copy);
            break; //We already have everything we need
        }
        payload_copy = strtok(NULL, "|");
    }
    EV_INFO <<  "SOS occured at the following posX: " << Result_Storage[2] << "and posY: " << Result_Storage[1] << "To the cyclist with the ID: " << Result_Storage[0] <<"\n";

    return Result_Storage;
}

void Emergency_Service_MQTT::AmbulanceDispatch(MQTTPacket& SOS_Packet){
    //This is just a placeholder for a better function, SOS might be to generalized and not a good way to handle multiple SOS if that might be the case
    //We need to look into a use-case where we are able to handle a big cluster sized accident (Maybe make a way to if a SOS is received we keep checking if there are more coming from near-by positions)
    EV_WARN << "SOS" << "\n";
    char text[128];
    sprintf(text,"SOS RECEIVED-Event number: %lld", getSimulation()->getEventNumber());
    getSimulation()->getActiveEnvir()->alert(text);

    //Importante -> Aparentemente cada flow faz com que se avançe uma unidade no modo de identificação (f_0.0 -> f_1.0 -> f_2.0)
    //Logo as ambulâncias são todas identicadas de f_1.0 a f_1.10.

    auto traci = inet::getModuleFromPar<traci::Core>(par("traciCoreModule"), this); //To do this I adapted the use of the BasicNodeManager and the StationaryPositionProvider codes as to learn and gain access to the TraCI Api controller
        traci::Listener::subscribeTraCI(traci);
    auto api = traci->getAPI();

    /*for(const std::string& id : api->vehicle.getIDList()){
        EV_INFO << "Hello there " << id << "\n";
    }*/
    
    //Fazer com que o carro vá para um determinada posição, isto literalmente é um teleport do veículo

    //api->vehicle.moveToXY("f_1.0", "E23", 0, 239.59, 157.30, 100.0, 0);
    
    //A maneira mais correta de fazer o que se pretende é provavelmente através da geração de uma nova rota, libsumo::TraCIStage TraCIAPI::SimulationScope::findRoute
    //A função da API do TraCI mencionada anteriormente é uma das maneiras de gerar um rota nova, outra maneira seria através da construção de um algoritmo
    //Capaz de aceder às ruas possivelmente afetas por tráfego / acidentes de modo a sugerir as melhores rotas possíveis. Como o objetivo neste momento é velocidade de produção de uma solução, o caminho de menor esforço vai ser usado.
    //E após a geração da mesma a utilização da função setRoute() da api do TraCI, void TraCIAPI::VehicleScope::setRoute 
    //A função setRoute() será relativamente difícil de implementar porque ela precisa de todos os Edges (Ruas) para chegar aos destino, ou seja, é preciso encontrar uma maneira de dizer ao programa para fornecer todas as ruas até ao destino
    //Outra coisa a fazer seria uma função para a verificação rápida de seleção automática das ambulâncias.

    //Estou a ter problemas em conseguir com que o veículo sai do parque. O movetoxy aparenta não funcionar porque o programa assume que o veículo está no E22, logo não mexe o veículo para fora do parque
    //A Falta de comandos para o controlo de alguns elementos adicionais (parque e afins) de uma maneira simplistica para a linguagem C++ torna
    //O Exemplo sob o qual se está a trabalhar mais complicado de tornar uma realidade, mesmo o uso de paragens e afins é um pouco mais complicado
    //Par além disso, o resumo do movimento é bastante complicado visto que não existe um resume() tal como na API para python

    //A rota é alterada mas a ambulância não sai do parque, para tentar resolver este problema vai ser adicionada uma lane extra para a saida dos veículos
    //Apesar de ter adicionado uma rota extra o veículo continua "preso" no parque, por este motivo o parque foi movido para se encontrar dentro da via, tal como sugerio aqui https://github.com/eclipse-sumo/sumo/issues/13509
    //Muito provavelmente isto têm a haver com o facto de que a rota é alterada mas o estado de stop do veículo não o é.

    if(Ambulance_in_use == false){

    double laneLength = api->lane.getLength("E22_0");

    //EV_INFO << "Lane length -> " << laneLength << "\n";

    //api->vehicle.setStop("f_1.0", "E22", 26.0, 0, 0 , 0, 21.0);

    //api->vehicle.StopAndRouteReplace("f_1.0","E22", 26.0, 0, api->vehicle.getStopState("f_1.0") , 21.0 ,0.0,api->vehicle.getRouteID("f_1.0"));

    //replaceStop(vehID, stopIndex, "") -> Sugerido em https://github.com/eclipse-sumo/sumo/issues/12630

    //api->vehicle.resume("f_1.0"); //Esta é a melhor solução visto que não faz qualquer tipo de alteração e simplesmente indica que a paragem têm de terminar e o veículo têm de voltar a se movimentar
                                //Enquanto que as outras soluções envolviam a alteração direta do estado (variáveis) do veículo, sendo que aqui, o SUMO intepreta o comando e realiza estas alterações por si mesmo.

    //api->vehicle.setRoute("f_1.0",std::vector<std::string>{"E22","E23","E24","E25","-E18","C1C2"});
    
    //From here we should try to contact the DEN service to send the DENM message to the Emg Station 

    std::vector<char*> SOS_Info = Location_And_ID_Extractor(SOS_Packet.getPayload());

    std::string Vehicle_Id_String = SOS_Info[0];

    std::string Accident_Road = api->vehicle.getRoadID(Vehicle_Id_String);

    /*EV_INFO << "Accident happened in the following road -> " << Accident_Road << "\n";

    EV_INFO << "From:" << api->vehicle.getRoadID("f_1.0") << "To: " << Accident_Road << "Vehicle Type: " << api->vehicle.getTypeID("f_1.0") << "\n";*/

    std::vector<std::string> Route_Steps = api->simulation.findRoute("E22",Accident_Road,"Ambulance").edges;


    for(std::string& route_edge : Route_Steps){
        //EV_WARN << "Ambulance Route description ->" << route_edge << "\n";
    }

    api->vehicle.setRoute("f_1.0",Route_Steps);

    api->vehicle.resume("f_1.0");

    //Now that the ambulance is going to the accident location we should probably make a service that only the ambulance has access to to receive this instead 
    //Of doing it all in here (specially to when he gets to the bycicle).
    //We should probably make the ambulance stop right in front of the bycicle so as to not impide the traffic.

    double Accident_Bycicle_Position_Lane = api->vehicle.getLanePosition(Vehicle_Id_String);

    //api->lane.getLength(api->vehicle.getLaneID(Vehicle_Id_String))-

    double Stop_Position_Ambulance_Start = Accident_Bycicle_Position_Lane + 1 + api->vehicle.getLength("f_1.0");
    double Stop_Position_Ambulance_End = Stop_Position_Ambulance_Start + api->vehicle.getLength("f_1.0");

    //EV_INFO << "Accident_Bycicle_Position_Lane -> " << Accident_Bycicle_Position_Lane << " Stop_Position_Ambulance_Start -> " << Stop_Position_Ambulance_Start << " Stop_Position_Ambulance_End -> " << Stop_Position_Ambulance_End <<  "\n";

    api->vehicle.setStop("f_1.0", Accident_Road, Stop_Position_Ambulance_End, api->vehicle.getLaneIndex(Vehicle_Id_String), 5 , 0, Stop_Position_Ambulance_Start);
    
    Ambulance_in_use = true;

    }
}