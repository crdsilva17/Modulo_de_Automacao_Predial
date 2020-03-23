/*///////////////////////////////////////////////////////////////////////////////////////////////////////////
//Empresa:    CRS Automação
//Arquivo:    Módulo 2 Relés
//Tipo:       standalone 
//Autor:      Cristiano da Rocha Silva
//Descricao:  Rele para automação residencial.
//Data:       22/01/2020
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////*/

//Libs
#include <Arduino.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <PubSubClientTools.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//default custom static IP
char static_ip[16] = "192.168.1.80";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";
//Protocol MQTT
char mqtt_server[60] = "example.com";
char mqtt_port[6] = "1883";
char mqtt_user[30] = "root";
char mqtt_pass[20] = "senha123";
char mqtt_prefix[30] = "AUM/";
char mqtt_topic[60] = "topic/";
char mqtt_dev1[20] = "device 1";
char mqtt_dev2[20] = "device 2";
//Host ID
char hostname[20] = "MAP";
const byte dnsPort = 53;
//pin mode
short input1 = 14;
short input2 = 13;
short output1 = 4;
short output2 = 5;
short btnReset = 0;
short led = 2;
bool sts1 = false;
bool sts2 = false;
//page html
String page = "";

//flag for saving data
bool shouldSaveConfig = false;


//DNS Server
DNSServer dnsServer;

//WebServer
ESP8266WebServer server(80);

//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

// Client WiFi MQTT
WiFiClient espClient;
PubSubClient client(espClient);
PubSubClientTools mqtt(client);

//Methods
void handleRoot();
void handleAction();
void handleJson();
void handleReset();
void handleNotFound();
void topic1_sub(String topic,String message);
void topic2_sub(String topic,String message);

//callback notifying us of the need to save config
void saveConfigCallback () {
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  if(SPIFFS.begin()){ //iniciando gerenciador de arquivos
    if(SPIFFS.exists("/config.json")){ // verificando a existencia do arquivo de configuração
      Serial.println("File Exist");
      File configFile = SPIFFS.open("/config.json","r");
      Serial.println("lendo arquivo");
      if(configFile){
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(),size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc,buf.get());
        if(!error){
          Serial.println("sem error");
          strcpy(hostname,doc["myhost"]);
          strcpy(mqtt_dev1,doc["dev1"]);
          strcpy(mqtt_dev2,doc["dev2"]);
          strcpy(mqtt_pass,doc["pass"]);
          strcpy(mqtt_port,doc["port"]);
          strcpy(mqtt_server,doc["serv"]);
          strcpy(mqtt_topic,doc["topic"]);
          strcpy(mqtt_user,doc["user"]);
          input1 = atoi(doc["in1"]);
          input2 = atoi(doc["in2"]);
          output1 = atoi(doc["out1"]);
          output2 = atoi(doc["out2"]);
          Serial.println("Copiou dados");
          if(doc["ip"]){
           Serial.println("IP exist");
            //static_ip = json["ip"];
            strcpy(static_ip, doc["ip"]);
            strcpy(static_gw, doc["gateway"]);
            strcpy(static_sn, doc["subnet"]);
          }
        }
      }
      Serial.println("fechando arquivo");
    configFile.close();
    }
  }//end SPIFFS begin

  //Configuração dos Pinos de Entrada e Saída
  pinMode(input1,INPUT);
  pinMode(input2,INPUT);
  pinMode(output1,OUTPUT);
  pinMode(output2,OUTPUT);
  pinMode(btnReset,INPUT);
  pinMode(led, OUTPUT);
  digitalWrite(led,HIGH);
  digitalWrite(output1,LOW);
  digitalWrite(output2,LOW);

// Criando parametros para adicionar a tela principal
WiFiManagerParameter custom_hostname("hostname", "host",hostname,20);
WiFiManagerParameter custom_dev1("dev1","device 1",mqtt_dev1,20);
WiFiManagerParameter custom_dev2("dev2","device 2",mqtt_dev2,20);
WiFiManagerParameter custom_pass("pass","password",mqtt_pass,20);
WiFiManagerParameter custom_port("port","port",mqtt_port,6);
WiFiManagerParameter custom_server("serv","server",mqtt_server,60);
WiFiManagerParameter custom_topic("topic","topic",mqtt_topic,60);
WiFiManagerParameter custom_user("user","userName",mqtt_user,30);
String input = (String)input1;
WiFiManagerParameter custom_in1("in1", "Input 1", (char*)input.c_str(), 2);
input = (String)input2;
WiFiManagerParameter custom_in2("in2", "Input 2", (char*)input.c_str(), 2);
input = (String)output1;
WiFiManagerParameter custom_out1("out1", "Output 1", (char*)input.c_str(), 2);
input = (String)output2;
WiFiManagerParameter custom_out2("out2", "Output 2", (char*)input.c_str(), 2);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);

  //Recebendo valores do usuario

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  wifiManager.addParameter(&custom_hostname);
  wifiManager.addParameter(&custom_server);
  wifiManager.addParameter(&custom_port);
  wifiManager.addParameter(&custom_user);
  wifiManager.addParameter(&custom_pass);
  wifiManager.addParameter(&custom_topic);
  wifiManager.addParameter(&custom_dev1);
  wifiManager.addParameter(&custom_dev2);
  wifiManager.addParameter(&custom_in1);
  wifiManager.addParameter(&custom_in2);
  wifiManager.addParameter(&custom_out1);
  wifiManager.addParameter(&custom_out2);
  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  char autConnect[40];
  String name = "MAP"+(String)WiFi.macAddress(); 
  name.toCharArray(autConnect,40);
  if (!wifiManager.autoConnect(autConnect, "senha123")) {
    Serial.println("3 seg");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    Serial.println("RESET ESP");
    ESP.reset();
    delay(5000);
  }

  strcpy(hostname, custom_hostname.getValue());
  strcpy(mqtt_dev1,custom_dev1.getValue());
  strcpy(mqtt_dev2,custom_dev2.getValue());
  strcpy(mqtt_pass,custom_pass.getValue());
  strcpy(mqtt_port,custom_port.getValue());
  strcpy(mqtt_server,custom_server.getValue());
  strcpy(mqtt_topic,custom_topic.getValue());
  strcpy(mqtt_user,custom_user.getValue());

  input1 = atoi(custom_in1.getValue());
  input2 = atoi(custom_in2.getValue());
  output1 = atoi(custom_out1.getValue());
  output2 = atoi(custom_out2.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DynamicJsonDocument doc(1024);

    doc["myhost"] = hostname;
    doc["dev1"] = mqtt_dev1;
    doc["dev2"] = mqtt_dev2;
    doc["pass"] = mqtt_pass;
    doc["port"] = mqtt_port;
    doc["serv"] = mqtt_server;
    doc["topic"] = mqtt_topic;
    doc["user"] = mqtt_user;
    doc["in1"] = String(input1);
    doc["in2"] = String(input2);
    doc["out1"] = String(output1);
    doc["out2"] = String(output2);

    doc["ip"] = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"] = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("ERROR confgFile");
    }

    serializeJson(doc, configFile);
    configFile.close();
    //end save
  }
dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
dnsServer.start(dnsPort,"*",_ip);

if(MDNS.begin(String(hostname))){
  MDNS.addService("http", "tcp", 80);
}

//PubSubClient client(mqtt_server,(int)mqtt_port,espClient);
//PubSubClientTools mqtt(client);
client.setServer(mqtt_server,atoi(mqtt_port));


if(client.connect(hostname) or client.connect(hostname,mqtt_user,mqtt_pass))
{
  Serial.println("Connected to MQTT");
  String topic = mqtt_topic;
  String config = mqtt_prefix;
  String msg('{');
  topic += mqtt_dev1;
  config += topic;
  config += "/config";
  topic += "/set";
  mqtt.subscribe(topic,topic1_sub);
  msg += "\"name\" : " + String(hostname) + "_1" + ",\"command_topic\" : " + String(topic);
  topic.replace("set","state");
  msg += ",\"state_topic\" : " + String(topic);
  msg += "\"}\"";
  mqtt.publish(config,msg);
  delay(200);
  topic = mqtt_topic;
  topic += mqtt_dev2;
  topic += "/set";
  mqtt.subscribe(topic,topic2_sub);
  msg = "{";
  msg += "\"name\" : " + String(hostname) + "_2" + ",\"command_topic\" : " + String(topic);
  topic.replace("set","state");
  msg += ",\"state_topic\" : " + String(topic);
  msg += "\"}\"";
  config.replace(mqtt_dev1, mqtt_dev2);
  mqtt.publish(config,msg);
}

server.on("/",handleRoot);
server.on("/action",handleAction);
server.on("/reset",handleReset);
server.onNotFound(handleNotFound);

server.on("/all",HTTP_POST,[](){
  if(SPIFFS.begin()){
    if(SPIFFS.exists("/config.json")){
      Serial.println("File Exist");
      File configFile = SPIFFS.open("/config.json","r");
      Serial.println("lendo arquivo");
      if(configFile){
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(),size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc,buf.get());
        if(!error){
          Serial.println("sem error");
          strcpy(hostname,doc["myhost"]);
          strcpy(mqtt_dev1,doc["dev1"]);
          strcpy(mqtt_dev2,doc["dev2"]);
          strcpy(mqtt_pass,doc["pass"]);
          strcpy(mqtt_port,doc["port"]);
          strcpy(mqtt_server,doc["serv"]);
          strcpy(mqtt_topic,doc["topic"]);
          strcpy(mqtt_user,doc["user"]);
          Serial.println("Copiou dados");
        }
      }
      Serial.println("fechando arquivo");
    configFile.close();
    }
  }//end SPIFFS begin
  String json('{');
  json += "\"mqtt_server\":\"" + (String)mqtt_server;
  json += "\", \"mqtt_port\":\"" + String(mqtt_port);
  json += "\", \"mqtt_user\":\"" + String(mqtt_user);
  json += "\", \"mqtt_pass\":\"" + String(mqtt_pass);
  json += "\", \"mqtt_topic\":\"" + String(mqtt_topic);
  json += "\", \"mqtt_dev1\":\"" + String(mqtt_dev1);
  json += "\", \"mqtt_dev2\":\"" + String(mqtt_dev2);
  json += "\", \"pin1\":" + String(input1);
  json += ", \"pin2\":" + String(input2);
  json += ", \"pin3\":" + String(output1);
  json += ", \"pin4\":" + String(output2);
  json += "}";
  server.send(200, "text/json", json);
  Serial.println(json);
  json.clear();
});

server.on("/btn",handleJson);

ArduinoOTA.setHostname(hostname);
ArduinoOTA.setPassword("senha123");

ArduinoOTA.onStart([](){
   SPIFFS.end();
   delay(500);
   digitalWrite(led,LOW);
});

ArduinoOTA.onEnd([](){
  delay(500);
  ESP.restart();
  delay(3000);
});

  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });



server.begin();
ArduinoOTA.begin();

}//end setup

void loop() {

  ArduinoOTA.handle();

  // put your main code here, to run repeatedly:
  MDNS.update();
  // Do work:
  //DNS
  dnsServer.processNextRequest();

  client.loop();
 
  server.handleClient();

if(!digitalRead(btnReset)){
  delay(450);
  if (!digitalRead(btnReset))
  {
    delay(700);
    if (!digitalRead(btnReset))
    {
      digitalWrite(led,LOW);
      delay(1000);
      digitalWrite(led,HIGH);
      delay(2000);

      if (!digitalRead(btnReset))
      {
      digitalWrite(led,LOW);
      delay(1000);
      digitalWrite(led,HIGH);
      delay(2000);

        for(int i = 0; i < 10; i++){
          digitalWrite(led,LOW);
          delay(300);
          digitalWrite(led,HIGH);
          delay(300);
        }
        delay(500);
      wifiManager.resetSettings();
      delay(1000);
      if(SPIFFS.begin()){
        if(SPIFFS.exists("/config.json")){
          SPIFFS.remove("/config.json");
        }
      }
      delay(1000);
      ESP.reset();
      delay(3000);
      }else{
        wifiManager.resetSettings();
        delay(500);
        ESP.reset();
        delay(3000);
      }
      
    }
    
  }
  
}

if(digitalRead(input1) != sts1){
  delay(350);
  sts1 = digitalRead(input1);
  digitalWrite(output1,digitalRead(output1)?LOW:HIGH);
  if(client.connected())
  {
  String topic = mqtt_topic;
  topic += mqtt_dev1;
  topic += "/state";
  mqtt.publish(topic,digitalRead(output1)?"ON":"OFF");
  }  
}
if(digitalRead(input2)!= sts2){
  delay(350);
  sts2 = digitalRead(input2);
  digitalWrite(output2,digitalRead(output2)?LOW:HIGH);
  if(client.connected())
  {
  String topic = mqtt_topic;
  topic += mqtt_dev2;
  topic += "/state";
  mqtt.publish(topic,digitalRead(output2)?"ON":"OFF");
  }
}

}

void topic1_sub(String topic, String message)
{
  
  if(message.equals("ON"))
  {
    digitalWrite(output1,HIGH);
    topic.replace("set","state");
    mqtt.publish(topic,"ON");
  }else if(message.equals("OFF"))
  {
    digitalWrite(output1,LOW);
    topic.replace("set","state");
    mqtt.publish(topic,"OFF");
  }
  
}

void topic2_sub(String topic, String message)
{
  if(message.equals("ON"))
  {
    digitalWrite(output2,HIGH);
    topic.replace("set","state");
    mqtt.publish(topic,"ON");
  }else if(message.equals("OFF"))
  {
    digitalWrite(output2,LOW);
    topic.replace("set","state");
    mqtt.publish(topic,"OFF");
  }
}

void handleRoot(){
  
  if(SPIFFS.begin()){
    if(SPIFFS.exists("/index.htm"))
    {
      File file = SPIFFS.open("/index.htm","r");
      server.streamFile(file, "text/html");
      file.close();
    }
  }
  
}

void handleAction(){
  if(server.args()){
    PubSubClient client(mqtt_server,(int)mqtt_port,espClient);
    PubSubClientTools mqtt(client);
    Serial.print("Comando: ");
    Serial.println(server.arg(0));
    if(server.argName(0).equals("out1")){
      if (server.arg(0).equals("On"))
      {
        digitalWrite(output1, HIGH);
        if(client.connected())
        {
        String topic = mqtt_topic;
        topic += mqtt_dev1;
        topic += "/state";
        mqtt.publish(topic,"ON");
        }
      }else if(server.arg(0).equals("Off")){
        digitalWrite(output1,LOW);
        if(client.connected())
        {
        String topic = mqtt_topic;
        topic += mqtt_dev1;
        topic += "/state";
        mqtt.publish(topic,"OFF");
        }
      }
      handleJson();
    }else if(server.argName(0).equals("out2")){
      if(server.arg(0).equals("On")){
        digitalWrite(output2,HIGH);
        if(client.connected())
        {
        String topic = mqtt_topic;
        topic += mqtt_dev2;
        topic += "/state";
        mqtt.publish(topic,"ON");
        }
      }else if(server.arg(0).equals("Off")){
        digitalWrite(output2,LOW);
        if(client.connected())
        {
        String topic = mqtt_topic;
        topic += mqtt_dev2;
        topic += "/state";
        mqtt.publish(topic,"OFF");
        }
      }
      handleJson();
    }else if(server.argName(0).equals("reset")){
      if(server.arg(0).equals("WiFi")){
      server.send(200,"text/plain","Reset WIFI!!!! Please reconnect to Access Point.");
      delay(500);
      wifiManager.resetSettings();
      delay(1000);
      ESP.reset();
      delay(5000);
    }else if(server.arg(0).equals("All")){
      server.send(200,"text/plain","Restart device!!!!");
      delay(500);
      wifiManager.resetSettings();
      delay(1000);
      if(SPIFFS.begin()){
        if(SPIFFS.exists("/config.json")){
          SPIFFS.remove("/config.json");
        }
      }
      delay(1000);
      ESP.reset();
      delay(5000);
    }
    }else if(server.argName(0).equals("checkbox")){
      if (server.arg(0).equals("on"))
      {
         
        DynamicJsonDocument doc(1024);

        doc["myhost"] = hostname;
        doc["dev1"] = server.arg(6);
        doc["dev2"] = server.arg(7);
        doc["pass"] = server.arg(4);
        doc["port"] = server.arg(2);
        doc["serv"] = server.arg(1);
        doc["topic"] = server.arg(5);
        doc["user"] = server.arg(3);
        doc["in1"] = server.arg(8);
        doc["in2"] = server.arg(9);
        doc["out1"] = server.arg(10);
        doc["out2"] = server.arg(11);

        doc["ip"] = WiFi.localIP().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["subnet"] = WiFi.subnetMask().toString();

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
          Serial.println("ERROR confgFile");
        }

        serializeJson(doc, configFile);
        configFile.close();
        //end save
       
        delay(500);
        ESP.reset();
        delay(3000);
      }
      
    }else if(server.argName(0).equals("server"))
    {
      if(SPIFFS.begin())
      {
        if(SPIFFS.exists("/config.json"))
        {
          File file = SPIFFS.open("/config.json","r");
        if(file){
        size_t size = file.size();
        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(),size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc,buf.get());
        if(!error){
          Serial.println("sem error");
          strcpy(hostname,doc["myhost"]);
          strcpy(mqtt_dev1,doc["dev1"]);
          strcpy(mqtt_dev2,doc["dev2"]);
          strcpy(mqtt_pass,doc["pass"]);
          strcpy(mqtt_port,doc["port"]);
          strcpy(mqtt_server,doc["serv"]);
          strcpy(mqtt_topic,doc["topic"]);
          strcpy(mqtt_user,doc["user"]);
          Serial.println("Copiou dados");
          if(doc["ip"]){
           Serial.println("IP exist");
            //static_ip = json["ip"];
            strcpy(static_ip, doc["ip"]);
            strcpy(static_gw, doc["gateway"]);
            strcpy(static_sn, doc["subnet"]);
          }
        }
        file.close();
      }
        }
      
      if(SPIFFS.begin()){
      

      DynamicJsonDocument doc(1024);

        doc["myhost"] = hostname;
        doc["dev1"] = mqtt_dev1;
        doc["dev2"] = mqtt_dev2;
        doc["pass"] = mqtt_pass;
        doc["port"] = mqtt_port;
        doc["serv"] = mqtt_server;
        doc["topic"] = mqtt_topic;
        doc["user"] = mqtt_user;
        doc["in1"] = server.arg(7);
        doc["in2"] = server.arg(8);
        doc["out1"] = server.arg(9);
        doc["out2"] = server.arg(10);

        doc["ip"] = static_ip;
        doc["gateway"] = static_gw;
        doc["subnet"] = static_sn;

        File file = SPIFFS.open("/config.json", "w");
        if (file) {
          Serial.println("ERROR confgFile");
        }

        serializeJson(doc, file);
        file.close();
        //end save
        handleRoot();
        delay(500);
        ESP.reset();
        delay(3000);
        }
      }
    }
  }
  
}

void handleJson(){
  String json('{');
  json += "\"p1\":\"" + String(digitalRead(output1)?"On":"Off") + "\"";
  json += ", \"p2\":\"" + String(digitalRead(output2)?"On":"Off") + "\"";
  json += "}";
  server.send(200, "text/json", json);
  Serial.println(json);
  json.clear();
}

void handleReset(){

  if(SPIFFS.begin()){
    if(SPIFFS.exists("/config.htm"))
    {
      File file = SPIFFS.open("/config.htm","r");
      server.streamFile(file, "text/html");
      file.close();
    }
  }
  

}

void handleNotFound(){
  server.send(404,"text/plain", "Page NotFound!!!!");
}