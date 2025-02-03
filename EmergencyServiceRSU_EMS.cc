
#include "artery/application/Asn1PacketVisitor.h"
#include "artery/application/DenmObject.h"
#include "artery/application/Timer.h"
#include "artery/application/StoryboardSignal.h"
#include "artery/application/VehicleDataProvider.h"
#include "artery/utility/FilterRules.h"
#include <omnetpp/checkandcast.h>
#include <omnetpp/ccomponenttype.h>
#include <omnetpp/cxmlelement.h>
#include <vanetza/asn1/denm.hpp>
#include <vanetza/btp/ports.hpp>
#include <vanetza/asn1/cam.hpp>

#include "EmergencyServiceRSU_EMS.h"
#include "lte_msgs2/BlackIceWarning_m.h"
#include "artery/utility/PointerCheck.h"
#include "artery/utility/InitStages.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <omnetpp/checkandcast.h>
#include <traci/BasicNodeManager.h>
#include "traci/Core.h"
#include <traci/API.h>
#include "inet/applications/udpapp/UDPBasicBurst.h"
#include <string>

#include "artery/application/AmbulanceSystem.h"

using namespace omnetpp;

namespace artery
{

//como a estação de ems é um poi, podemos ver se o uso das mensagens do tipo poi, definidas no ETSI TS 102 890-1
//Esta norma especifica como o anunciamento de serviços deve ser feito no ambito do ITS-G5.
//Apesar de não ser o anuncio do acidente, este pode ser "adaptado" da seguinte maneira, anuncio de um serviço de salvamento para a localização x.
//podem ser usadas para a notificação das amb. para estas iniciarem a marcha para as mesmas (falta ver o formado e especificações do mesmo)
//se isto nao funcionar deve se usar uma app udp no rsu e na amb para notificar a msm da necessidade de ir assistir o ciclista
//De qualquer maneira, o uso de um serviço que não assente na stack ITS-G5 parece ser o mais indicado visto que não há um caso-uso em específico para este tipo de situação
//Visto que o DENM de acidente não parece indicado para este fim.
//Outra maneira de fazer isto seria provavelmente através da adaptação de um dos serviços
//Para já a manira usada vai ser através do IoT com mensagens básicas mas diretas do que se passa
//Isto até poderia ser entendido como um bombeiro a ser enviado da estação para a ambulância
//O ETSI TS 103 301 pode conter uma explicação melhor de como este ponto deve ser tratado.

Define_Module(EmergencyServiceRSU_EMS)

static const simsignal_t denmReceivedSignal = cComponent::registerSignal("DenmReceived");
static const simsignal_t denmSentSignal = cComponent::registerSignal("DenmSent");
//static const simsignal_t storyboardSignal = cComponent::registerSignal("StoryboardSignal");
static const simsignal_t SharedSignal_MQTT_To_Den = cComponent::registerSignal("SOS_To_DEN");
static const simsignal_t SharedSignal_DEN_TO_MQTT = cComponent::registerSignal("SOS_Sent");

EmergencyServiceRSU_EMS::EmergencyServiceRSU_EMS() :
    mTimer(nullptr), mSequenceNumber(0)
{
}

void EmergencyServiceRSU_EMS::initialize()
{
    ItsG5BaseService::initialize();
    SOS_to_Ambulance = 0;
    WATCH(SOS_to_Ambulance);
    mTimer = &getFacilities().get_const<Timer>();
    mMemory.reset(new artery::den::Memory(*mTimer));
    
    //initUseCases();
    subscribe(SharedSignal_MQTT_To_Den);
    
    cModule* currentModule = getParentModule();
    StartConfigTrigger = new cMessage("ConfiguringNode");
    
    scheduleAt(simTime() + uniform(0.0, StartConfigTime), StartConfigTrigger);
} 

void EmergencyServiceRSU_EMS::initUseCases()
{
    omnetpp::cXMLElement* useCases = par("useCases").xmlValue();
    for (omnetpp::cXMLElement* useCaseElement : useCases->getChildrenByTagName("usecase")) {
        omnetpp::cModuleType* useCaseType = omnetpp::cModuleType::get(useCaseElement->getAttribute("type"));
        omnetpp::cXMLElement* filter = useCaseElement->getFirstChildWithTag("filters");

        bool useCaseApplicable = true;
        if (filter) {
            artery::FilterRules rules(getRNG(0), getFacilities().get_const<artery::Identity>());
            useCaseApplicable = rules.applyFilterConfig(*filter); 
        }

        if (useCaseApplicable) {
            const char* useCaseName = useCaseElement->getAttribute("name") ?
                useCaseElement->getAttribute("name") : useCaseType->getName();
            omnetpp::cModule* module = useCaseType->create(useCaseName, this);
            // do not call initialize here! omnetpp::cModule initializes submodules on its own!
            module->buildInside();
            den::AdaptedUseCase* useCase = dynamic_cast<den::AdaptedUseCase*>(module);
            if (useCase) {
                mUseCases.push_front(useCase);
            }
        }
    }
}

void EmergencyServiceRSU_EMS::finish()
{
    //AmbulanceSocket.close();
}

void EmergencyServiceRSU_EMS::receiveSignal(cComponent*, simsignal_t signal, cObject* obj, cObject*)
{
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"Taga");
    getSimulation()->getActiveEnvir()->alert(text1);
    
}


void EmergencyServiceRSU_EMS::indicate(const vanetza::btp::DataIndication& indication, std::unique_ptr<vanetza::UpPacket> packet)
{
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"Taga");
    getSimulation()->getActiveEnvir()->alert(text1);
    Asn1PacketVisitor<vanetza::asn1::Denm> visitor;
    const vanetza::asn1::Denm* denm = boost::apply_visitor(visitor, *packet);
    const auto egoStationID = this->getParentModule()->getId();
    //const auto egoStationID = getFacilities().get_const<VehicleDataProvider>().station_id();

    if (denm && (*denm)->header.stationID != egoStationID) {
        DenmObject obj = visitor.shared_wrapper;
        mMemory->received(obj);
        emit(denmReceivedSignal, &obj);

        for (auto use_case : mUseCases) {
            use_case->indicate(obj);
        }
    }
}

vanetza::btp::DataRequestB EmergencyServiceRSU_EMS::createRequest()
{
    namespace geonet = vanetza::geonet;
    using vanetza::units::si::seconds;
    using vanetza::units::si::meter;

    vanetza::btp::DataRequestB request;
    request.gn.traffic_class.tc_id(0);
    request.gn.maximum_hop_limit = 1;

    geonet::DataRequest::Repetition repetition;
    repetition.interval = 0.1 * seconds;
    repetition.maximum = 0.3 * seconds;
    request.gn.repetition = repetition;

    geonet::Area destination;
    geonet::Circle destination_shape;
    destination_shape.r = 100.0 * meter;
    destination.shape = destination_shape;
    destination.position.latitude = 0;
    destination.position.longitude = 0;
    //destination.position.latitude = mVdp->latitude();
    //destination.position.longitude = mVdp->longitude();
    request.gn.destination = destination;

    return request;
}

void EmergencyServiceRSU_EMS::trigger()
{
    mMemory->drop();

    for (auto use_case : mUseCases) {
        use_case->check();
    }
}

ActionID_t EmergencyServiceRSU_EMS::requestActionID()
{
    ActionID_t id;
    //id.originatingStationID = getFacilities().get_const<VehicleDataProvider>().station_id();
    id.sequenceNumber = ++mSequenceNumber;
    return id;
}

const Timer* EmergencyServiceRSU_EMS::getTimer() const
{
    return mTimer;
}
 
std::shared_ptr<const den::Memory> EmergencyServiceRSU_EMS::getMemory() const
{
    return mMemory;
}

void EmergencyServiceRSU_EMS::sendDenm(vanetza::asn1::Denm&& message, vanetza::btp::DataRequestB& request)
{
    fillRequest(request);
    DenmObject obj { std::move(message) };
    emit(denmSentSignal, &obj);

    using namespace vanetza;
    
    using DenmConvertible = vanetza::convertible::byte_buffer_impl<vanetza::asn1::Denm>;
    std::unique_ptr<geonet::DownPacket> payload { new geonet::DownPacket };
    std::unique_ptr<vanetza::convertible::byte_buffer> denm { new DenmConvertible { obj.shared_ptr() } };
    payload->layer(OsiLayer::Application) = vanetza::ByteBufferConvertible { std::move(denm) };
    this->request(request, std::move(payload));
}

void EmergencyServiceRSU_EMS::sendAmbulanceRequest()
{
    
    using namespace vanetza;
    btp::DataRequestB request;
	request.destination_port = btp::ports::CAM;
	request.gn.its_aid = aid::CA;
	request.gn.transport_type = geonet::TransportType::SHB;
	request.gn.maximum_lifetime = geonet::Lifetime { geonet::Lifetime::Base::One_Second, 1 };
	request.gn.traffic_class.tc_id(static_cast<unsigned>(dcc::Profile::DP2));
	request.gn.communication_profile = geonet::CommunicationProfile::ITS_G5;

    std::string obj = "Test";
    using ByteBuffer = convertible::byte_buffer_impl<std::string>;
    std::unique_ptr<geonet::DownPacket> payload { new geonet::DownPacket() };
    std::unique_ptr<convertible::byte_buffer> buffer { new ByteBuffer(obj) };
    payload->layer(OsiLayer::Application) = std::move(buffer);
    ssize_t test_size = payload->size();
    EV_INFO << "Packet size:" << test_size << "\n";
    //cPacket *Test = new cPacket("Test");
    this->request(request,std::move(payload));

    SOS_to_Ambulance++;

}

void EmergencyServiceRSU_EMS::sendAmbulanceRequest_Temp(std::vector<std::string> Route_Steps_Temp, std::string AmbulanceID)
{
    
    std::string Ambulance_Path_Prefix = "World.";
    std::string Ambulance_Full_Path = Ambulance_Path_Prefix.append(AmbulanceID); //A melhor maneira de enviar o ID de volta para questões de aceder aos métodos é através do getModule
    
    //auto varHolding = par("AmbulanceModule").setStringValue(Ambulance_Full_Path);
    cModule* middleware = this->getParentModule();
    cModule* RSU = middleware->getParentModule();
    cModule* World = RSU->getParentModule();
    cModule* Ambulance = World->getModuleByPath(Ambulance_Full_Path.c_str());
    if(Ambulance != nullptr){
        EV_INFO << "Got this id" << Ambulance->getId() << "\n";
        EV_INFO << "Attempt Search ->"<< Ambulance_Full_Path << "\n";
    }else{
        EV_INFO << "Got a nullptr" << "\n";
    }
    
    //Ambulance_To_Request = inet::getModuleFromPar<artery::AmbulanceSystem>(varHolding, this);

}

//Esta parte vai estar responsável por processar os dados da mensagem do Servidor para o EMS_RSU e de a introduzir em uma mensagem válida para os parâmetros
//Do Abstract Syntax Notation One (ASN.1). 
//Até ao momento a mensagem CAM para o caso uso de coordenação de veículos foi vista como a melhor maneira de adaptar um caso uso existent à situação do projeto.
//Falar com os professores se isto não entraria no mundo do CCAM visto que isto seria a maneira de as ambulâncias receberem informações acerca de um
//VRU não conectado aos serviços associados ao ITS-G5, o que faria com que a mensagem não fosse um CAM mas sim uma mensagem CPM (Cooperative Perception Message)
//Outra coisa que se poderia fazer era assumir que o estação de emergência é um POI e usar as informações definidas em ETSI TR 102 638 V2.1.1 no capítulo 5.10.
//No capítulo 5.10.6 está descrito que a POIM pode ser usada para a dissiminição de informações acerca dos serviços oferecidos pelos Pontos de interesse
//Apesar de o serviço DEN requirir, de um ponto de vista geral, a formulação da mensagem DENM e o envio da mesma aquando da ocorrência do evento, 
//é possível dizer que o evento, neste caso, é a necessidade do envio de uma ambulância ou a chegada do pedido.
//Decentralized Environmental Notification Message (DENM)  is a facilities layer message that is mainly used by ITS applications in order to alert road users of a detected event using ITS communication technologies
//Final draft ETSI EN 302 637-3 V1.2.1 (2014-09).
//Depois de falar com os professores foi sugerido usar a DENM
//Refazer um use case para ser usado, repor as coisas do DEN e por o DENM a ser lido na parte da ambulância.

void EmergencyServiceRSU_EMS::fillRequest(vanetza::btp::DataRequestB& request)
{
    using namespace vanetza;

    request.destination_port = btp::ports::DENM;
    request.gn.its_aid = aid::DEN;
    request.gn.transport_type = geonet::TransportType::GBC;
    request.gn.communication_profile = geonet::CommunicationProfile::ITS_G5;
}

int EmergencyServiceRSU_EMS::numInitStages() const
{
    return artery::InitStages::Total;
}

void EmergencyServiceRSU_EMS::ProcessSOS(MQTTPacket& SubscribePacket)
{
    //We need to extract the position of the bycicle and then create and send a emergency vehicle towards it
    
}

/*void EmergencyServiceRSU_EMS::processPacket(cPacket* pkt)
{
    auto ctrl = pkt->getControlInfo();
    const char* MQTT_Classname_Checker = pkt->getName();
    if (auto udp = dynamic_cast<inet::UDPDataIndication*>(ctrl)) {
        
        if (udp->getDestPort() == EmergPort){
            EV_WARN << "SOS occured, contacted by Server"  << "\n";
            emit(SharedSignal_MQTT_To_Den, pkt);  // Send data via signal
        } else {
            //throw cRuntimeError("Unknown UDP destination port %d", udp->getDestPort());
        }
    }
}*/

void EmergencyServiceRSU_EMS::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        if(msg == StartConfigTrigger){
            ConfigNode();
        }
        if(msg == StartCheckingSOS){
            CheckAndAnsSosRequest();
        }
    }
}

void EmergencyServiceRSU_EMS::TestAccessMQTT()
{
    char text1[128];
    text1[128] = '\0';
    strcat(text1,"CONNECTION SUCEFULL YAY");
    getSimulation()->getActiveEnvir()->alert(text1);
}

void EmergencyServiceRSU_EMS::ConfigNode()
{
    //AmbulancePort = par("AmbulanceSystemPort");
    //AmbulanceSocket.setOutputGate(gate("udpOut"));
    //AmbulanceSocket.bind(inet::L3Address(), AmbulancePort);
}

//void EmergencyServiceRSU::sendAmbulanceRequest(){
    //vanetza::btp::DataRequestB& request
//}

//When we receive a SOS request we check if there are any ambulances in the parking lot.
//If there aren't any ambulances we call AddAwaitingSOSRequest to add that SOS request to an awaiting list.
//We still need to decide how to this awaiting list should be handeled since we still don't have a way to comunicate with the ambulance to the EMS RSU.
std::string EmergencyServiceRSU_EMS::CheckAvailableAmbulance(MQTTPacket& SOS_Packet,bool add_to_wait_list)
{
    auto traci = inet::getModuleFromPar<traci::Core>(par("traciCoreModule"), this); //To do this I adapted the use of the BasicNodeManager and the StationaryPositionProvider codes as to learn and gain access to the TraCI Api controller
        traci::Listener::subscribeTraCI(traci);
    auto api = traci->getAPI();
    bool SOSRequestFulfilled = false;
    for(const std::string& id : api->vehicle.getIDList()){
        EV_INFO << "ID->" << id << "\n";
        if(id.find("f_1.") != std::string::npos){ //We check if the vehicle is an ambulance (all ambulances belong to the flow 1)
        //Then we check if the ambulance is in a parking area
            EV_INFO << "ID->" << api->vehicle.getTypeID(id) << "\n";
            //The type check is kind of redundant but for reliability questions we still check the vehicle type to confirm that it is indeed an ambulance
            if(strcmp(api->vehicle.getTypeID(id).c_str(),"Ambulance") == 0){
                
            //The vehicle is pretty much in a parking area for sure, now it's time to check if it is in the region
            //We do this since the ambulance can be stopped at the hospital for the drop off duration of the end of the sos call
                libsumo::TraCIPosition vehicle_position = api->vehicle.getPosition(id);
                if(api->vehicle.getStopState(id) > 128){
                    if(vehicle_position.x > 256.00 && vehicle_position.x < 268.00 ){
                        if(vehicle_position.y > 163.00 && vehicle_position.y < 172.00){
                            for (auto use_case : mUseCases) {
                                use_case->activateUseCase();
                            }
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

                            //It seems that the ambulance is available (still have to perfom some tests to check if the we really even need the list)
                            std::vector<char*> SOS_Info = Location_And_ID_Extractor(SOS_Packet.getPayload());

                            std::string Vehicle_Id_String = SOS_Info[0];

                            std::string Accident_Road = api->vehicle.getRoadID(Vehicle_Id_String);

                            std::vector<std::string> Route_Steps = api->simulation.findRoute("E22",Accident_Road,"Ambulance").edges;

                            for(std::string& route_edge : Route_Steps){
                                //EV_WARN << "Ambulance Route description ->" << route_edge << "\n";
                            }

                            sendAmbulanceRequest_Temp(Route_Steps,id);

                            api->vehicle.setRoute(id,Route_Steps);

                            api->vehicle.resume(id);
                            
                            //Now that the ambulance is going to the accident location we should probably make a service that only the ambulance has access to to receive this instead 
                            //Of doing it all in here (specially to when he gets to the bycicle).
                            //We should probably make the ambulance stop right in front of the bycicle so as to not impide the traffic.

                            double Accident_Bycicle_Position_Lane = api->vehicle.getLanePosition(Vehicle_Id_String);

                            //api->lane.getLength(api->vehicle.getLaneID(Vehicle_Id_String))-

                            double Stop_Position_Ambulance_Start = Accident_Bycicle_Position_Lane + 1 + api->vehicle.getLength(id);
                            double Stop_Position_Ambulance_End = Stop_Position_Ambulance_Start + api->vehicle.getLength(id);

                            //EV_INFO << "Accident_Bycicle_Position_Lane -> " << Accident_Bycicle_Position_Lane << " Stop_Position_Ambulance_Start -> " << Stop_Position_Ambulance_Start << " Stop_Position_Ambulance_End -> " << Stop_Position_Ambulance_End <<  "\n";

                            api->vehicle.setStop(id, Accident_Road, Stop_Position_Ambulance_End, api->vehicle.getLaneIndex(Vehicle_Id_String), 5 , 0, Stop_Position_Ambulance_Start);
                            
                            char text1[128];
                            text1[128] = '\0';
                            strcat(text1,"SOS Answered");
                            getSimulation()->getActiveEnvir()->alert(text1);

                            //We need to make sure the ambulance then goes to the hospital and then goes to the back to the EMS station.
                            //One way to do this is to make it possile for the ambulance to detect when it arrives to it's final destination and stops.
                            //When this happens we need to give them new routes to go to the places they need to be and remove the bycicle from the simulation.
                            return id; //We return the ambulance id
                        }
                    }
                }else{
                    //Later on we can try to discover if there is a way to contact a ambulance if it isn't in the inRoute Ambulances (Ambulances answering to a SOS message or transporting a cyclist to the hospital)

                }
            }
        }   
    }
    if(add_to_wait_list){
        AddAwaitingSOSRequest(SOS_Packet);
    }
    return "";

}

//This function is going to fetch the position of the vehicle which sent the SOS, then we are going to use the findRoute() function in the TraCIApi to get a route to the accident based on the best effort metric
//This can be changed by us designing our own function or just giving the ambulance the edge to where it needs to go and then using an algorithm on the ambulance itself.
//In the broad sense of location we can't turn the lat and long pos from the simulator into a location he can interpert, we can however use the vehicle ID to get the edge where it currently is
//And then use the setStop from the API to establish the place where the ambulance should try to stop to assist the fallen cyclist.
//In a real life scenario the lat and long would probably be more than enough for the emergency services to assist the injured cyclist.
std::vector<char*> EmergencyServiceRSU_EMS::Location_And_ID_Extractor(const char * Sos_Packet_content){
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

void EmergencyServiceRSU_EMS::AddAwaitingSOSRequest(MQTTPacket& AwaitingSosRequest)
{
    Enter_Method("AddAwaitingSOSRequest");
    //We probably will have very quick periodic checks for this or maybe we just put all the SOS requests as awaiting ones.
    //Maybe the best awaiting period would be 1 second if there are SOSRequests awaiting an answer
    //If the AwaitingSOSRequest list is empty we don't check for available ambulances until there are more SOS requests.
    //In the future adding a more complex but reliable way of knowing if the cyclist is still emitting the SOS should be implemented.
    AwaitingSOS.push_back(&AwaitingSosRequest);
    StartCheckingSOS = new cMessage("CheckSOS");
    CheckSOSTimer = par("CheckSOSTimer");
    scheduleAt(simTime() + uniform(0.0, CheckSOSTimer), StartCheckingSOS);
}

void EmergencyServiceRSU_EMS::CheckAndAnsSosRequest(){
    Enter_Method("CheckAndAnsSosRequest");
    for(MQTTPacket* AttemptSOS : AwaitingSOS){
        if(CheckAvailableAmbulance(*AttemptSOS,false) != ""){
            AwaitingSOS.remove(AttemptSOS);
        }
    }
    if(AwaitingSOS.empty()){
        //No more SOS are awaiting
    }else{
        //We still have more SOS requests that can't be answered due to a ambulance shortage
        scheduleAt(simTime() + uniform(0.0, CheckSOSTimer), StartCheckingSOS);
    }
}

} // namespace artery


