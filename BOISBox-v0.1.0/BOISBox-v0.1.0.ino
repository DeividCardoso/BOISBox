#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

const byte        WEBSERVER_PORT          = 80;
const char*       WEBSERVER_HEADER_KEYS[] = {"User-Agent"};
const byte        DNSSERVER_PORT          = 53;
const byte      LED_PIN                 = 2;
const byte      LED_ON                  = HIGH;
const byte      LED_OFF                 = LOW;
const   size_t    JSON_SIZE               = JSON_OBJECT_SIZE(5) + 130;


WebServer  server(WEBSERVER_PORT);
DNSServer         dnsServer;

// Variáveis Globais ------------------------------------
char              id[30];       // Identificação do dispositivo
boolean           ledOn;        // Estado do LED
word              bootCount;    // Número de inicializações
char              ssid[30];     // Rede WiFi
char              pw[30];       // Senha da Rede WiFi

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

void ledSet() {
  // Define estado do LED
  digitalWrite(LED_PIN, ledOn ? LED_ON : LED_OFF);
}

// Funções de Configuração ------------------------------
void  configReset() {
  // Define configuração padrão
  strlcpy(id, "IeC Device", sizeof(id)); 
  strlcpy(ssid, "", sizeof(ssid)); 
  strlcpy(pw, "", sizeof(pw)); 
  ledOn     = false;
  bootCount = 0;
}

boolean configRead() {
  // Lê configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "r");
  if (deserializeJson(jsonConfig, file)) {
    // Falha na leitura, assume valores padrão
    configReset();

    log(F("Falha lendo CONFIG, assumindo valores padrão."));
    return false;
  } else {
    // Sucesso na leitura
    strlcpy(id, jsonConfig["id"]      | "", sizeof(id)); 
    strlcpy(ssid, jsonConfig["ssid"]  | "", sizeof(ssid)); 
    strlcpy(pw, jsonConfig["pw"]      | "", sizeof(pw)); 
    ledOn     = jsonConfig["led"]     | false;
    bootCount = jsonConfig["boot"]    | 0;

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
    jsonConfig["id"]    = id;
    jsonConfig["led"]   = ledOn;
    jsonConfig["boot"]  = bootCount;
    jsonConfig["ssid"]  = ssid;
    jsonConfig["pw"]    = pw;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  return false;
}

// Requisições Web --------------------------------------
void handleLogin() {
  File file = SPIFFS.open(F("/login.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    server.send(200, F("text/html"), s);
    log("Login - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Login - ERROR 500"));
    log(F("Login - ERRO lendo arquivo"));
  }
}


void handleHome() {
  // Home
  File file = SPIFFS.open(F("/home.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    s.replace(F("#id#")       , id);
    s.replace(F("#led#")      , ledOn ? F("Ligado") : F("Desligado"));
    s.replace(F("#bootCount#"), String(bootCount));
    s.replace(F("#serial#") , hexStr(ESP.getEfuseMac()));
    s.replace(F("#software#") , softwareStr());
    s.replace(F("#sysIP#")    , ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
    s.replace(F("#clientIP#") , ipStr(server.client().remoteIP()));
    s.replace(F("#active#")   , longTimeStr(millis() / 1000));
    s.replace(F("#userAgent#"), server.header(F("User-Agent")));

    server.send(200, F("text/html"), s);
    log("Home - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    log(F("Home - ERRO lendo arquivo"));
  }
}

void handleConfiguration() {
  File file = SPIFFS.open(F("/configuration.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
    s.replace(F("#ssid#"), ssid);

    if (WiFi.status() != WL_CONNECTED) {
      s.replace(F("#displayConnected#"), "d-none");     
    } 
   
    server.send(200, F("text/html"), s);
    log("Configuration - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Configuration - ERROR 500"));
    log(F("Configuration - ERRO lendo arquivo"));
  }
}

void handleSensores() {
  File file = SPIFFS.open(F("/sensores.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    server.send(200, F("text/html"), s);
    log("Sensores - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Sensores - ERROR 500"));
    log(F("Sensores - ERRO lendo arquivo"));
  }
}

void handleAtuadores() {
  File file = SPIFFS.open(F("/atuadores.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    server.send(200, F("text/html"), s);
    log("Atuadores - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Atuadores - ERROR 500"));
    log(F("Atuadores - ERRO lendo arquivo"));
  }
}

void handleJsonPage() {
  File file = SPIFFS.open(F("/json.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    server.send(200, F("text/html"), s);
    log("Json - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Json - ERROR 500"));
    log(F("Json - ERRO lendo arquivo"));
  }
}

void handleReconfig() {
  // Reinicia Config
  configReset();

  // Atualiza LED
  ledSet();

  // Grava configuração
  if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
    log("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
    log(F("Reconfig - ERRO reiniciando Config"));
  }
}

void handleReboot() {
  // Reboot
  File file = SPIFFS.open(F("/Reboot.html"), "r");
  if (file) {
    server.streamFile(file, F("text/html"));
    file.close();
    log("Reboot - Cliente: " + ipStr(server.client().remoteIP()));
    delay(100);
    ESP.restart();
  } else {
    server.send(500, F("text/plain"), F("Reboot - ERROR 500"));
    log(F("Reboot - ERRO lendo arquivo"));
  }
}

void handleCSS() {
  // Arquivo CSS
  File file = SPIFFS.open(F("/Style.css"), "r");
  if (file) {
    // Define cache para 3 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("CSS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("CSS - ERROR 500"));
    log(F("CSS - ERRO lendo arquivo"));
  }
}

void handleBootstrap() {
  File file = SPIFFS.open(F("/css/bootstrap.min.css"), "r");
  if (file) {
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();

    log("Bootstrap - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Bootstrap - ERROR 500"));
    log(F("Bootstrap - ERRO lendo arquivo"));
  }
}

void handleSigninCss() {
  File file = SPIFFS.open(F("/css/signin.css"), "r");
  if (file) {
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("Signin CSS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Signin CSS - ERROR 500"));
    log(F("Signin CSS - ERRO lendo arquivo"));
  }
}

void handleJquery() {
  File file = SPIFFS.open(F("/js/jquery.slim.min.js"), "r");
  if (file) {
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("application/javascript"));
    file.close();
    log("Jquery - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Jquery - ERROR 500"));
    log(F("Jquery - ERRO lendo arquivo"));
  }
}

void handleBootstrapJs() {
  File file = SPIFFS.open(F("/js/bootstrap.bundle.min.js"), "r");
  if (file) {
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("application/javascript"));
    file.close();
    log("Bootstrap JS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Bootstrap JS - ERROR 500"));
    log(F("Bootstrap JS - ERRO lendo arquivo"));
  }
}

void handleConnectWifi(){
    int i = 0;
    String s;
    
    if(server.hasArg("ssid")){
      s = server.arg("ssid");
      s.trim();
      strlcpy(ssid, s.c_str(), sizeof(ssid));
    }

     if(server.hasArg("pw")){
      s = server.arg("pw");
      s.trim();
      if (s != "") {
        strlcpy(pw, s.c_str(), sizeof(pw));
      }
    }    
    
    if (configSave()) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
      log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    } else {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
      log(F("ConfigSave - ERRO salvando Config"));
    }
}


// Setup -------------------------------------------
void setup() {
  // Velocidade para ESP32
  Serial.begin(115200);

  // SPIFFS
  if (!SPIFFS.begin()) {
    log(F("SPIFFS ERRO"));
    while (true);
  }

  // Lê configuração
  configRead();

  // Incrementa contador de inicializações
  bootCount++;

  // Salva configuração
  configSave();

  // LED
  pinMode(LED_PIN, OUTPUT);
  ledSet();

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
  server.on(F("/postConectarWifi"), handleConnectWifi);    

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
