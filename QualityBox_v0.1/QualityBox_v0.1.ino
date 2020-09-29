#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Mapeamento de Hardware ----------------------------
//Nivel
#define trig 5  
#define echo 4  


// Constantes MQTT -----------------------------------
const char* mqtt_server = "ec2-18-207-3-216.compute-1.amazonaws.com"; //mqtt server
char publishTopic[] = "sensores";
char subscribeTopic[] = "atuadores";


// Contantes Server ----------------------------------
const byte        WEBSERVER_PORT          = 80;
const char*       WEBSERVER_HEADER_KEYS[] = {"User-Agent", "Cookie"};
const byte        DNSSERVER_PORT          = 53;
const   size_t    JSON_SIZE               = JSON_OBJECT_SIZE(13) + 340;


WiFiClient espClient;
PubSubClient client(espClient); //lib required for mqtt

WebServer  server(WEBSERVER_PORT);
DNSServer         dnsServer;

// Variáveis Globais ------------------------------------
char              id[30];       // Identificação do dispositivo
char              tipo[30]; 
char              ssid[30];     // Rede WiFi
char              pw[30];       // Senha da Rede WiFi
char              broker[60]; 
char              boxPw[20];
word              timeout;
word              bootCount;    // Número de inicializações
boolean           bkrOn;        
boolean           phOn;
boolean           vazaoOn;
boolean           nivelOn;
boolean           temperaturaOn;


// Funções Genéricas ------------------------------------
void log(String s) {
  // Gera log na Serial
  Serial.println(s);
}

String softwareStr() {
  // Retorna nome do software
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}

String hexStr(const unsigned long &h, const byte &l = 8) {
  // Retorna valor em formato hexadecimal
  String s;
  s= String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

// Funções de Configuração ------------------------------
void  configReset() {
  strlcpy(id, "1", sizeof(id)); 
  strlcpy(tipo, "Entrada", sizeof(tipo)); 
  strlcpy(ssid, "", sizeof(ssid)); 
  strlcpy(pw, "", sizeof(pw)); 
  strlcpy(broker, mqtt_server, sizeof(broker));
  strlcpy(boxPw, "12345", sizeof(boxPw)); 
  timeout = 10;
  bootCount = 0;
  bkrOn = false;
  phOn = false;
  nivelOn = false;
  vazaoOn = false;
  temperaturaOn = false;
}

boolean configRead() {
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "r");
  if (deserializeJson(jsonConfig, file)) {
    // Falha na leitura, assume valores padrão
    configReset();

    log(F("Falha lendo CONFIG, assumindo valores padrão."));
    return false;
  } else {
    strlcpy(id, jsonConfig["id"] | "", sizeof(id)); 
    strlcpy(tipo, jsonConfig["tipo"] | "", sizeof(tipo)); 
    strlcpy(ssid, jsonConfig["ssid"] | "", sizeof(ssid)); 
    strlcpy(pw, jsonConfig["pw"] | "", sizeof(pw)); 
    strlcpy(broker, jsonConfig["broker"] | "", sizeof(broker)); 
    strlcpy(boxPw, jsonConfig["boxPw"] | "", sizeof(boxPw)); 
    timeout = jsonConfig["timeout"] | 0;
    bootCount = jsonConfig["boot"]    | 0;
    bkrOn     = jsonConfig["bkr"]     | false;
    phOn     = jsonConfig["ph"]     | false;
    nivelOn     = jsonConfig["nivel"]     | false;
    vazaoOn     = jsonConfig["vazao"]     | false;
    temperaturaOn     = jsonConfig["temperatura"]     | false;

    file.close();

    log(F("\nLendo config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
}

boolean configSave() {
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "w+");
  if (file) {
    // Atribui valores ao JSON e grava
    jsonConfig["id"]        = id;
    jsonConfig["tipo"]        = tipo;
    jsonConfig["ssid"]      = ssid;
    jsonConfig["pw"]        = pw;
    jsonConfig["broker"]  = broker;
    jsonConfig["boxPw"]     = boxPw;
    jsonConfig["timeout"]   = timeout;
    jsonConfig["boot"]      = bootCount;
    jsonConfig["bkr"]       = bkrOn;
    jsonConfig["ph"]       = phOn;
    jsonConfig["nivel"]       = nivelOn;
    jsonConfig["vazao"]       = vazaoOn;
    jsonConfig["temperatura"]       = temperaturaOn;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  return false;
}


// MQTT --------------------------------------------
void callbackMQTT(char* topic, byte* payload, unsigned int length) {   
  Serial.print("Nova mensagem do broker: [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }  
  Serial.println();
}

void connectmqtt()
{  
  client.connect("Entrada");  // ESP will connect to mqtt broker with clientID - [Entrada/Saida]
  {
    if (!client.connected())
    {
      reconnect();
    }
    else{
      log("Broker Conectado");
    }
    
    client.subscribe("status", 1);
    client.subscribe("solenoide", 1);
    client.subscribe("bomba", 1);
    client.subscribe("motor", 1);
    client.publish("status", "Conectado");    
  }
}

void reconnect() {
  byte b = 0;
  while (!client.connected() && b < 5) {
    Serial.println("Tentando reconectar ao Broker...");
    if (client.connect(tipo)) {
      Serial.println(" Broker Conectado");
      client.publish("Status", "Conectado");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    b++;
  }
}


// Sensores ----------------------------------------
bool leSensores(){
  StaticJsonDocument<300> doc;
  doc["id"] = id;
  doc["tipo"] = tipo;
  JsonObject pH = doc.createNestedObject("ph");
  pH["conectado"] = phOn;
  pH["valor"] = leSensorPH();
  JsonObject temp = doc.createNestedObject("temperatura");
  temp["conectado"] = temperaturaOn;
  temp["valor"] = leSensorTemperatura();
  JsonObject vazao = doc.createNestedObject("vazao");
  vazao["conectado"] = vazaoOn;
  vazao["valor"] = leSensorVazao();
  JsonObject nivel = doc.createNestedObject("nivel");
  nivel["conectado"] = nivelOn;
  nivel["valor"] = leSensorNivel();
  

  // Generate the minified JSON and send it to the Serial port.
  serializeJson(doc, Serial);
  //Serial.println();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  return client.publish(publishTopic, buffer, n);
}

double leSensorPH(){
  return (double) random(500, 700) / 100;
}

float leSensorNivel(){
  return retornaNivel();
}

double leSensorVazao(){
  return (double) random(0, 1000 / 100);
}

double leSensorTemperatura(){
  return (double) random(1000, 3000) /100;
}

// Setup -------------------------------------------
void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin()) {
    log(F("SPIFFS ERRO"));
    while (true);
  }
  
  configRead();
  
  bootCount++;

  configSave();


  // WiFi Access Point Configura WiFi para ESP32
  WiFi.setHostname(deviceID().c_str());
  WiFi.softAP(deviceID().c_str(), deviceID().c_str());
  log("WiFi AP " + deviceID() + " - IP " + ipStr(WiFi.softAPIP()));

  // Habilita roteamento DNS
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNSSERVER_PORT, "*", WiFi.softAPIP());

  // WiFi Station
  WiFi.begin(ssid, pw);
  log("Conectando WiFi " + String(ssid));
  byte b = 0;
  while(WiFi.status() != WL_CONNECTED && b < 30) {
    b++;
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // WiFi Station conectado
    log("WiFi conectado (" + String(WiFi.RSSI()) + ") IP " + ipStr(WiFi.localIP()));
  } else {
    log(F("WiFi não conectado"));
  }  
  

  // Servindo as Paginas   
  server.on(F("/"), handleLogin);
  server.on(F("/home"), handleHome);
  server.on(F("/config"), handleConfiguration);
  server.on(F("/sensores"), handleSensores);
  server.on(F("/atuadores")  , handleAtuadores);
  server.on(F("/json")  , handleJsonPage);
  server.on(F("/reconfig")  , handleReconfig);

  // Recursos CSS e JS
  server.on(F("/bootstrap"), handleBootstrap);
  server.on(F("/signin"), handleSigninCss);
  server.on(F("/jquery"), handleJquery);
  server.on(F("/bootstrap-js"), handleBootstrapJs);

  // Requisições HTTP
  server.on(F("/postNetSave"), handleNetSave);    
  server.on(F("/reboot"), handleReboot);
  server.on(F("/postBoxSave"), handleBoxPw);
  server.on(F("/loginCheck"), HTTP_POST, handleLoginCheck);

  server.onNotFound(handleLogin);
  server.collectHeaders(WEBSERVER_HEADER_KEYS, 1);
  server.begin();

  
  //MQTT
  client.setServer(broker, 1883);//connecting to mqtt server
  client.setCallback(callbackMQTT);
  //delay(5000);

  if(WiFi.status() == WL_CONNECTED){
    connectmqtt();
  }  
  
  configuraUltrassonico();
  // Pronto
  if(!bkrOn){
    log("Notificações desabilitadas!");
  }
  log(F("Pronto"));
}

// Loop --------------------------------------------
void loop() {
  // WatchDog ----------------------------------------
  yield();

  // DNS ---------------------------------------------
  dnsServer.processNextRequest();

  // Web ---------------------------------------------
  server.handleClient();  

  if (!client.connected())
  {
    if(WiFi.status() == WL_CONNECTED){
      reconnect();      
    }
  } 
  else{
  // Negocio --------------------------
    if(bkrOn){
      if(leSensores()){
        log("\nPublicado com sucesso!");
      }
      else{
        log("\nFalhou a publicação");
      }
      //delay(2000);
    }
  }  
  client.loop();
}
