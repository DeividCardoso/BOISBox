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

void handleLoginCheck() {    
    if(server.hasArg("user") && server.hasArg("pw")){     
      String user = server.arg("user");
      String pw = server.arg("pw");
      user.trim();
      pw.trim();
      
      if(user != "Admin" || pw != boxPw){
        log("Usuário ou senha inválidos!");
        log(user);
        log(pw);
        server.sendHeader("Location", "/",true); //Redirect to our html web page 
        server.send(302, "text/plane",""); 
      }  
      else{            
        server.sendHeader("Location", "/home",true); //Redirect to our html web page 
        server.send(302, "text/plane",""); 
      }      
    }
    else{
      server.sendHeader("Location", "/",true);
      server.send(302, "text/plane",""); 
    }
}

void handleHome() {
  File file = SPIFFS.open(F("/home.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
    
    File navMenu = SPIFFS.open(F("/navMenu.html"), "r");
    if (navMenu) {
      navMenu.setTimeout(100);
      String snavMenu = navMenu.readString();
      navMenu.close(); 
      s.replace(F("<navMenu></navMenu>"), snavMenu);
    }  
    
    s.replace(F("#id#"), id);
    s.replace(F("#tipo#"), tipo);
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

    File navMenu = SPIFFS.open(F("/navMenu.html"), "r");
    if (navMenu) {
      navMenu.setTimeout(100);
      String snavMenu = navMenu.readString();
      navMenu.close(); 
      s.replace(F("<navMenu></navMenu>"), snavMenu);
      s.replace(F("#configuracoesActive#"), "active");
    }  
    
    s.replace(F("#id#"), id);
    s.replace(F("#tipo#"), tipo);
    s.replace(F("#ssid#"), ssid);
    s.replace(F("#broker#"), broker);
    s.replace(F("#timeout#"), String(timeout));
    
    if(bkrOn){
      s.replace(F("#brokerchecked#"), "checked");
    }

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

void handleNetSave(){
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
    
    if(server.hasArg("broker")){
      s = server.arg("broker");
      s.trim();
      strlcpy(broker, s.c_str(), sizeof(broker));
      log("Broker: ");
      log(s);
    }
    bkrOn = false;
    
    if(server.hasArg("brokerCheck")){
      s = server.arg("brokerCheck");
      s.trim();
      if(s == "on")
      {
        bkrOn = true;
      }
    }

    if(server.hasArg("timeout")){
      s = server.arg("timeout");
      timeout = s.toInt();
    }
 
        
    if (configSave()) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
      log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    } else {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
      log(F("ConfigSave - ERRO salvando Config"));
    }
}

void handleReboot(){      
    ESP.restart();  
}

void handleBoxPw(){
String s, pw, rpw;
    if(server.hasArg("id")){
      s = server.arg("id");
      s.trim();
      strlcpy(id, s.c_str(), sizeof(id));
    }  
    if(server.hasArg("tipo")){
      s = server.arg("tipo");
      s.trim();
      strlcpy(tipo, s.c_str(), sizeof(tipo));
    }  
  
    if(server.hasArg("oldPassword")){      
      pw = server.arg("oldPassword");
      pw.trim();
      if (pw != boxPw) {
        server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Senha atual inválida.');history.back()</script></html>"));
        log("Senha atual inválida");
        return ;
      }
    }

    if(server.hasArg("newPassword")){
      pw = server.arg("newPassword");
      pw.trim();
    }    
    
    if(server.hasArg("reNewPassword")){
      rpw = server.arg("reNewPassword");
      rpw.trim();
    }

    if (pw != rpw) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Senhas são diferentes.');history.back()</script></html>"));
      log("Senhas são diferentes");
    } else {
      
      pw.toCharArray(boxPw, 30);
      
      if (configSave()) {
        server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Senha do Box atualizada.');history.back()</script></html>"));
        log("ConfigSave - Senha do Box IpCliente: " + ipStr(server.client().remoteIP()));
      } else {
          server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
          log(F("ConfigSave - ERRO salvando Config"));
        }
    }
}

void handleSensores() {
  
  
  File file = SPIFFS.open(F("/sensores.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    File navMenu = SPIFFS.open(F("/navMenu.html"), "r");
    if (navMenu) {
      navMenu.setTimeout(100);
      String snavMenu = navMenu.readString();
      navMenu.close(); 
      s.replace(F("<navMenu></navMenu>"), snavMenu);
      s.replace(F("#sensoresActive#"), "active");
    }  
  
    if(nivelOn){
       double valorNivel = leSensorNivel();
       String svalorNivel = String(valorNivel);      
       s.replace(F("#btn-nivel-cor#"), "btn-danger");
       s.replace(F("#text-nivel#"), "Desconectar");
       s.replace(F("#valorNivel#"), svalorNivel);
    }
    else {
       s.replace(F("#btn-nivel-cor#"), "btn-primary");
       s.replace(F("#text-nivel#"), "Conectar");
       s.replace(F("#valorNivel#"), " - - ");
    }

    if(phOn){
       double valorPH = leSensorPH();
       String svalorPH = String(valorPH);
       
       s.replace(F("#btn-ph-cor#"), "btn-danger");
       s.replace(F("#text-ph#"), "Desconectar");
       s.replace(F("#valorPH#"), svalorPH);
    }
    else {
       s.replace(F("#btn-ph-cor#"), "btn-primary");
       s.replace(F("#text-ph#"), "Conectar");
       s.replace(F("#valorPH#"), " - - ");
    }
     if(temperaturaOn){
       double valorTemperatura = leSensorTemperatura();
       String svalorTemperatura = String(valorTemperatura);
       
       s.replace(F("#btn-temperatura-cor#"), "btn-danger");
       s.replace(F("#text-temperatura#"), "Desconectar");
       s.replace(F("#valorTemperatura#"), svalorTemperatura);
    }
    else {
       s.replace(F("#btn-temperatura-cor#"), "btn-primary");
       s.replace(F("#text-temperatura#"), "Conectar");
       s.replace(F("#valorTemperatura#"), " - - ");
    }
     if(vazaoOn){
       double valorVazao = leSensorVazao();
       String svalorVazao = String(valorVazao);
       
       s.replace(F("#btn-vazao-cor#"), "btn-danger");
       s.replace(F("#text-vazao#"), "Desconectar");
       s.replace(F("#valorVazao#"), svalorVazao);
    }
    else {
       s.replace(F("#btn-vazao-cor#"), "btn-primary");
       s.replace(F("#text-vazao#"), "Conectar");
       s.replace(F("#valorVazao#"), " - - ");
    }

    server.send(200, F("text/html"), s);
    log("Sensores - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Sensores - ERROR 500"));
    log(F("Sensores - ERRO lendo arquivo"));
  }
}

void handleCalibrarNivel(){  
  File file = SPIFFS.open(F("/sensores-calib-nivel.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    File navMenu = SPIFFS.open(F("/navMenu.html"), "r");
    if (navMenu) {
      navMenu.setTimeout(100);
      String snavMenu = navMenu.readString();
      navMenu.close(); 
      s.replace(F("<navMenu></navMenu>"), snavMenu);
      s.replace(F("#sensoresActive#"), "active");
    }  
  
    if(nivelOn){
       double valorNivel = leSensorNivel();
       String svalorNivel = String(valorNivel);      
       s.replace(F("#btn-nivel-cor#"), "btn-danger");
       s.replace(F("#text-nivel#"), "Desconectar");
       s.replace(F("#valorNivel#"), svalorNivel);
    }
    else {
       s.replace(F("#btn-nivel-cor#"), "btn-primary");
       s.replace(F("#text-nivel#"), "Conectar");
       s.replace(F("#valorNivel#"), " - - ");
    }

    if(phOn){
       double valorPH = leSensorPH();
       String svalorPH = String(valorPH);
       
       s.replace(F("#btn-ph-cor#"), "btn-danger");
       s.replace(F("#text-ph#"), "Desconectar");
       s.replace(F("#valorPH#"), svalorPH);
    }
    else {
       s.replace(F("#btn-ph-cor#"), "btn-primary");
       s.replace(F("#text-ph#"), "Conectar");
       s.replace(F("#valorPH#"), " - - ");
    }
     if(temperaturaOn){
       double valorTemperatura = leSensorTemperatura();
       String svalorTemperatura = String(valorTemperatura);
       
       s.replace(F("#btn-temperatura-cor#"), "btn-danger");
       s.replace(F("#text-temperatura#"), "Desconectar");
       s.replace(F("#valorTemperatura#"), svalorTemperatura);
    }
    else {
       s.replace(F("#btn-temperatura-cor#"), "btn-primary");
       s.replace(F("#text-temperatura#"), "Conectar");
       s.replace(F("#valorTemperatura#"), " - - ");
    }
     if(vazaoOn){
       double valorVazao = leSensorVazao();
       String svalorVazao = String(valorVazao);
       
       s.replace(F("#btn-vazao-cor#"), "btn-danger");
       s.replace(F("#text-vazao#"), "Desconectar");
       s.replace(F("#valorVazao#"), svalorVazao);
    }
    else {
       s.replace(F("#btn-vazao-cor#"), "btn-primary");
       s.replace(F("#text-vazao#"), "Conectar");
       s.replace(F("#valorVazao#"), " - - ");
    }

    server.send(200, F("text/html"), s);
    log("Sensores - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Sensores - ERROR 500"));
    log(F("Sensores - ERRO lendo arquivo"));
  }
}

void handleAtualizarMedicao(){
  server.sendHeader("Location", "/calibrarNivel",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleMedicaoMinimo(){
  setNivelMinimo();
  server.sendHeader("Location", "/calibrarNivel",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleMedicaoMaximo(){
  setNivelMaximo();
  server.sendHeader("Location", "/calibrarNivel",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}


void handleAtuadores() {
  File file = SPIFFS.open(F("/atuadores.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    File navMenu = SPIFFS.open(F("/navMenu.html"), "r");
    if (navMenu) {
      navMenu.setTimeout(100);
      String snavMenu = navMenu.readString();
      navMenu.close(); 
      s.replace(F("<navMenu></navMenu>"), snavMenu);
      s.replace(F("#atuadoresActive#"), "active");
    }  

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

void handleToggleNivel(){
  nivelOn = !nivelOn;
  server.sendHeader("Location", "/sensores",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleToggleTemperatura(){
  temperaturaOn = !temperaturaOn;
  server.sendHeader("Location", "/sensores",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleTogglePH(){
  phOn = !phOn;
  server.sendHeader("Location", "/sensores",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleToggleVazao(){
  vazaoOn = !vazaoOn;
  server.sendHeader("Location", "/sensores",true); //Redirect to our html web page 
  server.send(302, "text/plane",""); 
}

void handleReconfig() {
  // Reinicia Config
  configReset();

  // Grava configuração
  if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
    log("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
    log(F("Reconfig - ERRO reiniciando Config"));
  }
}

//---------------------- CSS e JS Files ---------------- 

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
