Caso Uso - Dados Gerados

Este caso uso têm por objetivo estudar se a recolha, envio e distribuição das posições de uma x quantidade de bicicletas é possivel.
Para este fim foi realizada uma simulação no simulador de mobilidade SUMO da qual foram extraídas as posições de 10 bicicletas, sendo a extração realizada através da aplicação do argumento -fcd-output na execução da simulação através da linha de comandos.
De modo a transformar o ficheiro criado num ficheiro CSV foi utilizada a ferramenta xml2csv.

Quanto à escolha de um MQTT Broker existem duas opções em consideração de momento:
- HiveMQ;
- Mosquitto;

Ambos desempenham as suas funções com uma qualidade razoável sendo a escolha do Mosquitto a mais provável já que este é totalmente open-source (HiveMQ na sua versão de comunidade também o é) e pelo facto
de ser mais leve e mais configurável que o HiveMQ. Contudo é necessário apontar que o HiveMQ oferece uma solução gratuíta (mas limitada) em cloud.
Até agora só foram realizados testes com o broker a correr na mesma rede.

Neste caso uso foi utilizada uma placa ESP32 Devkit c4 de modo a realizar as operações que iriam ser realizadas pelos componentes integrados na bicicleta (neste caso uso apenas foi utilizado o protocolo Wi-Fi).
Até ao momento as operaçoes realizadas foram as seguintes:

- Conexão ao MQTT Broker;
- Criação de um tópico e atualização do conteúdo do mesmo.
- Envio de 50 posições, constituídas pelas latitude e longitude, de uma das bicicletas.

Através deste caso uso foi possível determinar, até agora, que seria uma boa ideia ter um tópico por bicicleta.
