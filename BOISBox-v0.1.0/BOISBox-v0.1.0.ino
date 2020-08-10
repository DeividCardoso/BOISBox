#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

const byte        WEBSERVER_PORT          = 80;
const char*       WEBSERVER_HEADER_KEYS[] = {"User-Agent"};
const byte        DNSSERVER_PORT          = 53;
const byte        LED_PIN                 = 2;
const byte        LED_ON                  = HIGH;
const byte        LED_OFF                 = LOW;
const   size_t    JSON_SIZE               = JSON_OBJECT_SIZE(8) + 250;


WebServer  server(WEBSERVER_PORT);
DNSServer         dnsServer;

// Variáveis Globais ------------------------------------
char              id[30];       // Identificação do dispositivo
char              ssid[30];     // Rede WiFi
char              pw[30];       // Senha da Rede WiFi
char              endpoint[60]; 
char              boxPw[20];
word              timeout;
word              bootCount;    // Número de inicializações
boolean           ledOn;        // Estado do LED


// Funções Genéricas ------------------------------------
void log(String s) {
  // Gera log na Serial
  Serial.println(s);
}

String softwareStr() {
  // Retorna nome do software
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}

String longTimeStr(const time_t &t){
  // Retorna segundos como "d:hh:mm:ss"
  String s = String(t / SECS_PER_DAY) + ':';
  if (hour(t) < 10) {
    s += '0';
  }
  s += String(hour(t)) + ':';
  if (minute(t) < 10) {
    s += '0';
  }
  s += String(minute(t)) + ':';
  if (second(t) < 10) {
    s += '0';
  }
  s += String(second(t));
  return s;
}

String hexStr(const unsigned long &h, const byte &l = 8) {
  // Retorna valor em formato hexadecimal
  String s;
  s= String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

String deviceID() {
  // Retorna ID padrão ESP32 utiliza função getEfuseMac()
  return "IeC-" + hexStr(ESP.getEfuseMac());
}

String ipStr(const IPAddress &ip) {
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++) {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}

// Funções de Configuração ------------------------------
void  configReset() {
  strlcpy(id, "BOIS Box", sizeof(id)); 
  strlcpy(ssid, "", sizeof(ssid)); 
  strlcpy(pw, "", sizeof(pw)); 
  strlcpy(endpoint, "", sizeof(endpoint));
  strlcpy(boxPw, "BOIS Box", sizeof(boxPw)); 
  timeout = 10;
  bootCount = 0;
  ledOn = false;
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
    strlcpy(ssid, jsonConfig["ssid"] | "", sizeof(ssid)); 
    strlcpy(pw, jsonConfig["pw"] | "", sizeof(pw)); 
    strlcpy(endpoint, jsonConfig["endpoint"] | "", sizeof(endpoint)); 
    strlcpy(boxPw, jsonConfig["boxPw"] | "", sizeof(boxPw)); 
    timeout = jsonConfig["timeout"] | 0;
    bootCount = jsonConfig["boot"]    | 0;
    ledOn     = jsonConfig["led"]     | false;


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
    jsonConfig["ssid"]      = ssid;
    jsonConfig["pw"]        = pw;
    jsonConfig["endpoint"]  = endpoint;
    jsonConfig["boxPw"]     = boxPw;
    jsonConfig["timeout"]   = timeout;
    jsonConfig["boot"]      = bootCount;
    jsonConfig["led"]       = ledOn;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  Serial.println("Não abri");
  return false;
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

  pinMode(LED_PIN, OUTPUT);

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
  while(WiFi.status() != WL_CONNECTED && b < 60) {
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
  server.on(F("/home"), handleHome);
  server.on(F("/config"), handleConfiguration);
  server.on(F("/sensores"), handleSensores);
  server.on(F("/atuadores")  , handleAtuadores);
  server.on(F("/json")  , handleJsonPage);

  // Recursos CSS e JS
  server.on(F("/bootstrap"), handleBootstrap);
  server.on(F("/signin"), handleSigninCss);
  server.on(F("/jquery"), handleJquery);
  server.on(F("/bootstrap-js"), handleBootstrapJs);

  // Requisições HTTP
  server.on(F("/postNetSave"), handleNetSave);    
  server.on(F("/reboot"), handleReboot);
  server.on(F("/postBoxSave"), handleBoxPw);

  server.onNotFound(handleHome);
  server.collectHeaders(WEBSERVER_HEADER_KEYS, 1);
  server.begin();

  // Pronto
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
}
