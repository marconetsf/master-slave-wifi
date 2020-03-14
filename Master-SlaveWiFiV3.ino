/* 
 * Programação do interruptor "Slave" 
 * Marcone Tenório da Silva Filho
 * Última atualização: 12/02/2020
*/

/*
 * Algorítimo do programa:
 *    Ao ser conectado pela primeira vez na energia, o mesmo irá ficar preso em um loop proveniente
 * a função wifiManager.autoConnect() que aguarda que o usuário se conecte na rede da esp que atual-
 * mente se encontram em modo AP.
 *    Para se conectar basta aguardar o celular redirecionar para a interface de configuração onde
 * será possível colocar a senha do WiFi local, após isso a esp irá reiniciar trocando do modo AP 
 * para o modo STA.
 *    Após condigurada e conectada no WiFi local, a esp irá procurar por um endereço DNS específico
 * na rede, sendo ele: "disp0.local".
 *    Caso não encontre, o que quer dizer que este é o primeiro dispositivo (interruptor) a ser co-  
 * nectado à rede, a esp automáticamente configura o seu próprio DNS para "disp0.local" o que imbui
 * à mesma da responsabilidade de fazer a ponte de comunicação entre as seguintes esps "slaves" e o
 * App, utilizando as funções novoDispositivo() e mapDispositivosApp() para receber as infor-
 * mações de outra esp e enviá-las ao app respectivamente.
 *    Caso encontre, o que quer dizer que já há uma ponte de comunicação entre dispositivos e app, a
 * esp enviará as suas informações, sendeo elas: Endereço IP, Endereço MAC e disponibilidade de car-
 * gas, para o "master" (disp0.local) utilizando a função sendMyIP() para isso. Após receber o feed-
 * back do envio das informações, a esp aguardará que o usuário aperte no botão mapear, onde irá re-
 * ceber as informações do slave através do master (disp0.local) e, tendo posse do ip do novo slave,
 * o app, através da função configDispositivo() irá enviar ao slave o próximo DNS disponível na rede,
 * assim, a esp irá executar a função saveOnEEPROM(), salvará essa informação em sua memória EEPROM,
 * que por sua vez reseta a esp e faz a configuração do DNS já utilizando a informação salva na me-
 * mória EEPROM.
 * 
 * OBS1: É interessante que assim que um dispositivo seja instalado (energizado), com excessão do 1°,
 * ele seja configurado no celular para que não ocorra a possibilidade do DHCP trocar o seu ip antes
 * que a configuração no app seja feita. 
 * 
 * Tabela de equivalência entre funções:
 * acionaDispositivo1() - dns.local/on1
 * acionaDispositivo2() - dns.local/on2
 * acionaDispositivo3() - dns.local/on3
 * statusDispositivo()  - dns.local/status
 * configDispositivo()  - dns.local/config/?dns_configuration={"dns":"****"}
 * resetDispositivo()   - dns.local/reset
 * novoDispositivo()    - dns.local/data/?ip_newdevice={"ip":"******","macAdress":"*******","tipoDisp":"*"}
 * mapDispositivosApp() - dns.local/map
 * restartDispositivo() - dns.local/restart
 * piscaDisp()          - dns.local/blink
 */

#include <FS.h>               // Biblioteca responsável por criar um sistema de arquivos dentro da memória 
#define arquivo "/devices.txt"
#include <WiFiManager.h>      // Biblioteca responsável por criar uma interface de configuração Wifi
#include <ESP8266WiFi.h>      // Biblioteca responsável por fazer a conexão da esp com a internet
#include <ArduinoJson.h>      // Biblioteca responsável por fazer a leitura de API's Json
#include <ArduinoOTA.h>       // Biblioteca responsável por fazer o upload via rede
#include <ESP8266WebServer.h> // Biblioteca responsável pela criação de um webserver
#include <ESP8266mDNS.h>      // Biblioteca responsável por definir um link de acesso
#include <EEPROM.h>;          // Biblioteca responsável por acessar a EEPROM da ESP
#include <ESP8266Ping.h>      // Biblioteca responsável por efetuar o ping com outros dispositivos

ESP8266WebServer server(80);  // Instancia um objeto do tipo servidor na porta 80
WiFiManager wifiManager;      // Instancia um objeto do tipo Wifi Manager

// Portas digitais que vão controlar o acionamento dos relés.
int pinRele1 = D5;
int pinRele2 = D6;
int pinRele3 = D7;

// Protótipo das funções que são utilizadas como resposta às requisições
void sendMyIP();            // Função responsável por enviar IP MAC e tipo do dispositivo para o disp0
void acionaDispositivo1();  // Função responsável por acionar a carga 1
void acionaDispositivo2();  // Função responsável por acionar a carga 2
void acionaDispositivo3();  // Função responsável por acionar a carga 3
void statusDispositivo();   // Função que retorna o estado das 3 cargas
void configDispositivo();   // Função que recebe o dns em formato json ?dns_condiguration={"dns":"****"}
bool idDispositivo();       // Função que verifica se há DNS cadastrado na EEPROM 
void saveOnEEPROM();        // Função que salva o DNS na EEPROM(100) assim como o estado da gravação(120) true = gravado, false = não gravado 
void resetDispositivo();    // Função que acessa a EEPROM e muda o estado da gravação dns(120) para false
bool masterVerification();  // Função responsável por descobrir se já há um dns "disp0.local" na rede, ou seja, se já há um master configurado
void mapDispositivosApp();  // Função responsável por repassar os ips cadastrados na esp (mestre) ao App
void novoDispositivo();     // Função responsável por recebia via Json a API de informações de um novo dispositivo
void restartDispositivo();  // Função responsável por reiniciar a esp
void pisca();               // Função responsável por fazer o LED_BUILTIN piscar
void piscaDisp();           // Função responsável por fazer o ledBuiltin piscar para identificar o equipamento físico

// Parametros para se comunicar com o mestre e enviar o ip
const char* host = "disp0.local"; // Define o disp0 como host para enviar as suas informações para pareamento
const int httpPort = 80;          // Define a porta de conexão

IPAddress staticIP(0, 0, 0, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0); 

int ipLastDigit = 111;
String dnsString = "";      // Variável responsável por armazenar o dns recebido pelo app e grava-lo na EEPROM
String dnsEEPROM = "";      // Variável responsável por armazenar o dns resgatado da EEPROM
String tipoDisp = "1";      // Quantidade de atuadores no interruptor
bool ipCadastrado = false;  // Variável que define se a esp continuará enviando seu ip
bool dnsCadastrado = false; // Variável que define se a esp deve iniciar o processo de solicitar um dns

String listaIP[] = {"", "", "", "", "", "", "", "", "", "", ""};  // Lista que armazena os endereços ip cadastrados de cada dispositivo
String listaMAC[] = {"", "", "", "", "", "", "", "", "", "", ""}; // Lista que armazena os endereços mac cadastrados de cada dispositivo
String tipoDISP[] = {"", "", "", "", "", "", "", "", "", "", ""}; // Lista que armazena a quantidade de cargas de cada dispositivo cadastrado
int qtdDISP = 0; // Contador que registra quantos dispositivos a esp tem atualmente em sua memória


void setup(){
  pinMode(pinRele1, OUTPUT);
  pinMode(pinRele2, OUTPUT);
  pinMode(pinRele3, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
   
  Serial.begin(115200); // Inicia a comunicação serial dos pinos RX(0) e TX(1) da esp

  if (idDispositivo()){
    staticIP = IPAddress(192, 168, 1, ipLastDigit);
    wifiManager.setSTAStaticIPConfig(staticIP, gateway, subnet);
    Serial.println("");
    Serial.println("###Setando configurações de rede###");
  }

  wifiManager.autoConnect("DispHouse4.0"); // Define o nome da esp em modo AP

  Serial.println("");
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(" (EEPROM)");
  Serial.print("Gateway: ");
  Serial.println(gateway);
  Serial.print("Subnet: ");
  Serial.println(subnet);
  Serial.println("");
  
  if (!masterVerification() && !idDispositivo()){
    staticIP = IPAddress(192, 168, 1, 97);
    dnsString = "disp0";
    Serial.println("");
    Serial.println("Mestre não encontrado na rede");
    Serial.println("Reinicializando dispositivo em modo Mestre");
    Serial.println("");
    saveOnEEPROM();
    ESP.restart();
  } else if (ipLastDigit == 97){ 
    Serial.println("");
    Serial.println("EU SOU O MESTRE");
    Serial.println("");
  } else {
    Serial.println("");
    Serial.println("Mestre encontrado na rede");
    Serial.println("Inicializando como novo dispositivo");
    Serial.println("");
  }
  
  if (idDispositivo()){ 
    Serial.println("");
    Serial.println("###Setando configurações do DNS###");
    Serial.print("Estado: ");
    if (MDNS.begin(dnsEEPROM)) {
     Serial.println("DNS configurado corretamente.");
     Serial.print("Endereço DNS: ");
     Serial.print(dnsEEPROM);
     Serial.println(".local");
    } else {
      Serial.println("Erro em configurar DNS");
    } 
  } else {
    Serial.println("DNS não cadastrado na EEPROM");
  }

  // Funções Slave
  server.on("/on1", acionaDispositivo1);              // Aciona e desaciona o interruptor 1
  server.on("/on2", acionaDispositivo2);              // Aciona e desaciona o interruptor 2
  server.on("/on3", acionaDispositivo3);              // Aciona e desaciona o interruptor 3
  server.on("/status", statusDispositivo);            // Leitura do status do interruptor para atualizar o estado dos icones assim que o app abre
  server.on("/config/", HTTP_GET, configDispositivo); // Configurao o DNS do dispositivo
  server.on("/reset", resetDispositivo);              // Função responsável por "resetar" a EEPROM da ESP
  server.on("/restart", restartDispositivo);          // Função responsável por reiniciar a esp
  server.on("/blink", piscaDisp);
  
  //Funções Master
  server.on("/data/", HTTP_GET, novoDispositivo);     // Quando o servidor recebe uma requisição com /data/ no corpo da string ele roda a função handleSentVar
  server.on("/map", mapDispositivosApp);              // Função que repassa os ips cadastrados ao celular
  server.on("/teste", saveOnFlash);
  server.begin();                                     // Inicia o webServer
   
  beginOTA();
}

void loop(){
  sendMyIP();
  ArduinoOTA.handle();  // Função responsável por verificar se há alguma tentativa de gravação acontecendo
  server.handleClient(); // Função responsável por verificar há algum acesso acontecendo
} 

void sendMyIP(){
  if(!ipCadastrado && !dnsCadastrado){ // Condicional para verificar se o ip já foi cadastrado
    WiFiClient client;
      if (!client.connect(host, httpPort)){ // Faz a conexão com o mestre 
      Serial.println("");  
      Serial.println("Falha na conexão com o Mestre");
      Serial.println("");
      return;
    } else {
      Serial.println("");
      Serial.println("Conexão com o Mestre estabelecida");
      Serial.println("Enviando informações para cadastramento do dispositivo");
      Serial.println("");
    }

    IPAddress staticIP = WiFi.localIP(); // Variável que contemplará as 6 partes do endereço MAC do dispositivo
    String ipAtual = String(staticIP[0]);
           ipAtual += ".";
           ipAtual += String(staticIP[1]);
           ipAtual += ".";
           ipAtual += String(staticIP[2]);
           ipAtual += ".";
           ipAtual += String(staticIP[3]);  

    Serial.println("Endereço IP: " + ipAtual);

    byte mac[6]; // Variável que contemplará as 6 partes do endereço MAC do dispositivo
    WiFi.macAddress(mac);
    String macAtual = String(mac[5], HEX);
           macAtual += ":";
           macAtual += String(mac[4], HEX);
           macAtual += ":";
           macAtual += String(mac[3], HEX);
           macAtual += ":";
           macAtual += String(mac[2], HEX);
           macAtual += ":";
           macAtual += String(mac[1], HEX);
           macAtual += ":";
           macAtual += String(mac[0], HEX);

    Serial.println("Endereço MAC: " + macAtual);
    Serial.println("Cargas: " + tipoDisp);
    
    String url = "/data/";
           url+= "?ip_newdevice=";
           url+= "{\"ip\":\"ip0_value\",\"macAdress\":\"macAdress_value\",\"tipoDisp\":\"tipoDisp_value\"}";
           
    url.replace("ip0_value", ipAtual);
    url.replace("macAdress_value", macAtual);
    url.replace("tipoDisp_value", tipoDisp);
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + // Envia os dados, sim a sintaxe é tudo isso
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("");
    Serial.println("Json enviado: " + url);
    Serial.println("");
            
    unsigned long timeout= millis();
    while (client.available() || client.connected()){
      if (millis() - timeout > 5000){ // Interrompe automaticamente a conexão caso ela dure mais de 5sec 
        Serial.println(">>> Client Timeout !");
        client.stop();
        return ;
      }

      // Após o envio dos parâmetros o host retorna apenas o ip para confirmar que o valor foi recebido corretamente
      if (client.available()){  // Verifica se há uma conexão aberta
        String line = client.readStringUntil('\n'); // Recebe resposta da esp mestre
  
        if(line.indexOf(ipAtual) != -1){ // Verifica se o ip enviado foi retornado
          ipCadastrado = true;
          client.stop();
          Serial.print("DISPOSITIVO CADASTRADO");
          pisca(10, 100);
          return ;
        }
      }
    }
  }
}

void acionaDispositivo1() { // Função de acionamento da carga 1
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele1) == HIGH) {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(pinRele1, LOW);
    statusDispositivo();
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(pinRele1 , HIGH);
    statusDispositivo();
  }
}

void acionaDispositivo2() { // Função de acionamento da carga 2
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele2) == HIGH) {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(pinRele2, LOW);
    statusDispositivo();
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(pinRele2, HIGH);
    statusDispositivo();
  }
}

void acionaDispositivo3() { // Função de acionamento da carga 3
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele3) == HIGH) {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(pinRele3, LOW);
    statusDispositivo();
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(pinRele3, HIGH);
    statusDispositivo();
  }
}

void statusDispositivo() { // Função que envia o status das 3 cargas em Json
  String statusDispositivos = "{\"dispositivo1\":\"status1\",\"dispositivo2\":\"status2\",\"dispositivo3\":\"status3\"}";;
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele1) == HIGH) {
    statusDispositivos.replace("status1", "LIGADO");
  } else {
    statusDispositivos.replace("status1", "DESLIGADO");
  }
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele2) == HIGH) {
    statusDispositivos.replace("status2", "LIGADO");
  } else {
    statusDispositivos.replace("status2", "DESLIGADO");
  }
  if (digitalRead(LED_BUILTIN) == HIGH && digitalRead(pinRele3) == HIGH) {
    statusDispositivos.replace("status3", "LIGADO");
  } else {
    statusDispositivos.replace("status3", "DESLIGADO");
  }
  server.send(200, "text/html", statusDispositivos);
}

void configDispositivo(){ // Função que recebe o DNS do app
  if (!dnsCadastrado){
    if (server.hasArg("dns_configuration")){  
      const size_t capacity1 = JSON_OBJECT_SIZE(1) + 33;
      DynamicJsonDocument doc1(capacity1);
      
      String line = server.arg("dns_configuration"); // Recebe resposta do app
      Serial.println(line);
  
      deserializeJson(doc1, line);    
      const char* dns = doc1["dns"];
      dnsString = dns;

      staticIP = WiFi.localIP();
      saveOnEEPROM();
      delay(100);
      
      ESP.restart(); // Função repsonsável por reiniciar a ESP via Watch Dog
    }
  }
}

bool idDispositivo(){ // Função que verifica se o DNS já está cadastrado
  EEPROM.begin(150);
  EEPROM.get(90, ipLastDigit);
  EEPROM.get(100, dnsEEPROM);
  EEPROM.get(120, dnsCadastrado);
  EEPROM.end();
  if (dnsCadastrado == true && dnsCadastrado != 255){
    return dnsCadastrado;
  }
}

void saveOnEEPROM(){ // Função que salva o DNS na EEPROM
  EEPROM.begin(150);
  ipLastDigit = staticIP[3];
  EEPROM.put(90, ipLastDigit);
  EEPROM.put(100, dnsString);
  dnsCadastrado = true;
  EEPROM.put(120, dnsCadastrado);
  EEPROM.end();
  Serial.println("");
  Serial.println("INFORMAÇÕES SALVAS NO DISPOSITIVO");
  Serial.println("");
}

void resetDispositivo(){ // Função que "reseta" a EEPROM do dispositivo
  EEPROM.begin(150);
  dnsCadastrado = false;
  EEPROM.put(120, dnsCadastrado);
  EEPROM.end();
  Serial.println("");
  Serial.println("DISPOSITIVO RESETADO");
  Serial.println("");
}

void restartDispositivo(){
    ESP.restart(); // Função repsonsável por reiniciar a ESP via Watch Dog
}

bool masterVerification(){
  bool disp0 = Ping.ping("disp0.local", 10);
  return disp0;
}

void novoDispositivo() {
  if (server.hasArg("ip_newdevice")) {
    String ip_values = server.arg("ip_newdevice");
    Serial.println(ip_values);
  
    const size_t capacity = JSON_OBJECT_SIZE(3) + 110;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, ip_values);
  
    const char* ip = doc["ip"];
    const char* macAdress = doc["macAdress"];
    const char* tipoDisp = doc["tipoDisp"];
  
    Serial.println("");
    Serial.println("NOVO DISPOSITIVO RECONHECIDO");
    Serial.println("");
  
    server.send(200, "text/html", ip);
  
    listaIP[qtdDISP] = ip;
    listaMAC[qtdDISP] = macAdress;
    tipoDISP[qtdDISP] = tipoDisp;
    qtdDISP++;
    if (qtdDISP >= 10){
      qtdDISP = 0;
    }
  }
}

void mapDispositivosApp() {
  String dispositivos = "";
  dispositivos += "{\n\r\"ip\": [";
  for (int i = 0; i < qtdDISP; i++) {
    dispositivos += "\"";
    dispositivos += listaIP[i];
    dispositivos += "\"";
    if (i != qtdDISP - 1) {
      dispositivos += ",";
    } else {
      dispositivos += "],\n";
    }
  }
  if (qtdDISP == 0) {
    dispositivos += "],\n";
  }

  dispositivos += "\r\"macAdress\": [";
  for (int i = 0; i < qtdDISP; i++) {
    dispositivos += "\"";
    dispositivos += listaMAC[i];
    dispositivos += "\"";
    if (i != qtdDISP - 1) {
      dispositivos += ",";
    } else {
      dispositivos += "],\n";
    }
  }
  if (qtdDISP == 0) {
    dispositivos += "],\n";
  }

  dispositivos += "\r\"tipoDISPO\": [";
  for (int i = 0; i < qtdDISP; i++) {
    dispositivos += "\"";
    dispositivos += tipoDISP[i];
    dispositivos += "\"";
    if (i != qtdDISP - 1) {
      dispositivos += ",";
    } else {
      dispositivos += "],\n";
    }
  }
  if (qtdDISP == 0) {
    dispositivos += "],\n";
  }

  dispositivos += "\r\"qtdDISP\": ";
  dispositivos += "\"";
  dispositivos += qtdDISP;
  dispositivos += "\"\n";
  dispositivos += "}";

  Serial.println(dispositivos);
  server.send(200, "text/html", dispositivos);
}

void pisca(int vezes, int tempo) {
  for (int d = 0; d <= vezes; d++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(tempo);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(tempo);
  }
}

void piscaDisp(){
  for (int d = 0; d <= 3; d++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
  }
}

void saveOnFlash(){
  bool spiffsActive = false;
  if (SPIFFS.begin()){
    Serial.println("SPIFFS ativo");
    spiffsActive = true;
  } else {
    Serial.println("Impossivel ativar SPIFFS");
  }
  delay(400);

  if (spiffsActive){
    if (SPIFFS.exists(arquivo)){
      File f = SPIFFS.open(arquivo, "a");
      if (f){
        f.print("teste");
        f.close();
      }

      f = SPIFFS.open(arquivo, "r");
      if (f){
        String s;
        while (f.position()<f.size()){
          s = f.readStringUntil('\n');
          s.trim();
          server.send(200, "text/html", s);
        }
        f.close();
      }
    } else {
      server.send(200, "text/html", "Arquivo não encontrado");
    }
  }
}

// Funções OTA
void startOTA(){
  String type;

  //caso a atualização esteja sendo gravada na memória flash externa, então informa "flash"
  if (ArduinoOTA.getCommand() == U_FLASH) type = "flash";
  else type = "filesystem"; // U_SPIFFS //caso a atualização seja feita pela memória interna (file system), então informa "filesystem"

  //exibe mensagem junto ao tipo de gravação
  Serial.println("Start updating " + type);
}

void endOTA(){
  Serial.println("\nEnd");
}

//exibe o progresso em porcentagem
void progressOTA(unsigned int progress, unsigned int total){
  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
}

void errorOTA(ota_error_t error){
  Serial.printf("Error[%u]: ", error);

  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  else if (error ==  OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  else if (error == OTA_END_ERROR) Serial.println("End Failed");
}

void beginOTA(){
  //define o que será executado quando o ArduinoOTA iniciar
  ArduinoOTA.onStart(startOTA);

  //define o que será executado quando o ArduinoOTA terminar
  ArduinoOTA.onEnd(endOTA);

  //define o que será executado quando o ArduinoOTA estiver gravado
  ArduinoOTA.onProgress(progressOTA);

  //define o que será executado quando o ARduinoOTA encontrar um erro
  ArduinoOTA.onError(errorOTA);

  //inicializa ArduinoOTA
  ArduinoOTA.begin();
}
