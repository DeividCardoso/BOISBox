#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// Mapeamento de Hardware ----------------------------
//Nivel
#define trig 5  
#define echo 4  
#define oneWireBus 19
#define sensorPH 35
const int portaVazao = GPIO_NUM_21;

// Constantes MQTT -----------------------------------
const char* mqtt_server = "ec2-18-207-3-216.compute-1.amazonaws.com"; //mqtt server
char publishTopic[] = "sensores";
char subscribeTopic[] = "atuadores";


// Contantes Server ----------------------------------
const byte        WEBSERVER_PORT          = 80;
const char*       WEBSERVER_HEADER_KEYS[] = {"User-Agent", "Cookie"};
const byte        DNSSERVER_PORT          = 53;
const   size_t    JSON_SIZE               = JSON_OBJECT_SIZE(13) + 340;
const int         LEITURAS_SENSOR         = 3;
const float       m                       = -7.32;


WiFiClient espClient;
PubSubClient client(espClient); //lib required for mqtt

WebServer  server(WEBSERVER_PORT);
DNSServer         dnsServer;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

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
int               contador = 0;
float             mediaPH[LEITURAS_SENSOR];
float             mediaNivel[LEITURAS_SENSOR];
float             mediaVazao[LEITURAS_SENSOR];
float             mediaTemperatura[LEITURAS_SENSOR];
double            vazao = 0;
double            acumuladoVazao = 0;
double            fatorConversao = 7.5;
float             calibraPH = 0;
unsigned long int avgValue;

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
  if(contador == LEITURAS_SENSOR){
    StaticJsonDocument<1024> doc;
    doc["id"] = id;
    doc["tipo"] = tipo;
    JsonObject pH = doc.createNestedObject("ph");
    pH["conectado"] = phOn;
    pH["valor"] = mediaLeitura(mediaPH, LEITURAS_SENSOR, false);
    JsonObject temp = doc.createNestedObject("temperatura");
    temp["conectado"] = temperaturaOn;
    temp["valor"] = mediaLeitura(mediaTemperatura, LEITURAS_SENSOR, false);
    JsonObject cons = doc.createNestedObject("consumo");
    cons["conectado"] = vazaoOn;    
    cons["valor"] = acumuladoVazao;
    JsonObject nivel = doc.createNestedObject("nivel");
    nivel["conectado"] = nivelOn;
    nivel["valor"] = mediaLeitura(mediaNivel, LEITURAS_SENSOR, false);


    serializeJson(doc, Serial);
    contador = 0;
    
    char buffer[256];
    size_t n = serializeJson(doc, buffer);   
    
    return client.publish(publishTopic, buffer, n);
  }
  else{
    phOn ? mediaPH[contador] = leSensorPH() : mediaPH[contador] = 0;
    nivelOn ? mediaNivel[contador] = leSensorNivel() : mediaNivel[contador] = 0;
    vazaoOn ? mediaVazao[contador] = leSensorVazao() : mediaVazao[contador] = 0;
    temperaturaOn ? mediaTemperatura[contador] = leSensorTemperatura() : mediaTemperatura[contador] = 0;
    contador++;
    return false;
  }
}

float mediaLeitura(float sensor[], int arraySize, bool contaZerados){
  float resultado = 0;
  int valoresZero = 0;
  
  for(int i = 0; i < arraySize; i++){
    if(contaZerados && sensor[i] == 0)
    {
      valoresZero++;
    }
      
    resultado += sensor[i];
//    Serial.print("sensor: ");
//    Serial.println(resultado);
  }

  if(contaZerados){
    return resultado/(arraySize - valoresZero);
  }
  else{
    return resultado/arraySize;
  }  
}

float leSensorPH(){
  int buf[10],temp;
  
  for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(sensorPH);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  
  avgValue=0;
  for(int i=2;i<8;i++)                      //take the average value of 6 center sample
    avgValue+=buf[i];    
  
  avgValue = avgValue/6;

  Serial.print("Bits lidos no sensor 0 - min | 4095 - max: ");
  Serial.println(avgValue);
  
  float voltage=((float)avgValue*3.3)/4095; //convert the analog into millivolt
  Serial.print("Conversão de bits para tensão: ");
  Serial.println(voltage);
  
  float phValue = 7 - (1.91 - voltage) * m;                      //convert the millivolt into pH value
  Serial.print("Conversão para pH: ");  
  Serial.println(phValue,2);

  phValue = phValue + calibraPH;
  
  Serial.print("pH calibrado: ");
  Serial.println(phValue, 2);
  
  return phValue;
}

float leSensorNivel(){
  float nivel = retornaNivel();
//  Serial.print("Nivel: ");
//  Serial.println(nivel);
  return nivel;
}

double leSensorVazao(){
  return vazao;
}

float leSensorTemperatura(){
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  return temperatureC;
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
  server.on(F("/onOffNivel")  , handleToggleNivel);
  server.on(F("/onOffTemperatura")  , handleToggleTemperatura);
  server.on(F("/onOffPH")  , handleTogglePH);
  server.on(F("/onOffVazao")  , handleToggleVazao);
  server.on(F("/calibrarNivel")  , handleCalibrarNivel);
  server.on(F("/calibrarVazao"), handleCalibraVazao);
  server.on(F("/atualizarMedicao")  , handleAtualizarMedicao);
  server.on(F("/medicaoMinimo")  , handleMedicaoMinimo);
  server.on(F("/medicaoMaximo")  , handleMedicaoMaximo);
  server.on(F("/resetarNivel")  , handleResetNivel);
  server.on(F("/zeraConsumo")  , handleZeraConsumo);

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
  server.on(F("/postCalibraVazao"), HTTP_POST, handleSaveCalibVazao);  
  
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

  // Sensores
  configuraUltrassonico();    //Ultrassonico
  sensors.begin();            //Temperatura
  iniciaVazao((gpio_num_t) portaVazao); //vazao
  startTimer();

  
  // Pronto
  if(!bkrOn){
    log("Notificações desabilitadas!");
  }

  pinMode(sensorPH, INPUT);
  
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
      else if(contador == 0){
        log("\nFalha na publicação!");
      }
    }
  }  
  
  client.loop();
}
