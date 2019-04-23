#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <Wire.h>
#include <INA219.h>
#include <ArduinoJson.h>

#define INA219_DEBUG 1
#define SPIFFS_ALIGNED_OBJECT_INDEX_TABLES 1

#define SHUNT_MAX_V 0.04  /* Rated max for our shunt is 75mv for 50 A current: 
                             we will mesure only up to 20A so max is about 75mV*20/50 lets put some more*/
#define BUS_MAX_V   16.0  /* with 12v lead acid battery this should be enough*/
#define MAX_CURRENT 20    /* In our case this is enaugh even shunt is capable to 50 A*/
#define SHUNT_R   0.00220506535057615275946847955956   /* Shunt resistor in ohm */

INA219 monitor;


// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      client->text("all data received");
      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());
      checkcmd(msg);

  // Extract values
      

      if(info->opcode == WS_TEXT)
        client->text("I got your text message"+msg);

      else
        client->binary("I got your binary message"+msg);
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        client->text("partial message");
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}


#include<credentials.h>
const char * hostName = "esp-async";
const char* http_username = "admin";
const char* http_password = "admin";

void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  WiFi.hostname(hostName);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(hostName);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssid, password);
  }

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();

  MDNS.addService("http","tcp",80);

  SPIFFS.begin();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(http_username,http_password));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  server.begin();
//    Wire.setClock(800000L);

  monitor.begin();
  // setting up our configuration
  // default values are RANGE_32V, GAIN_8_320MV, ADC_12BIT, ADC_12BIT, CONT_SH_BUS
//  monitor.configure(INA219::RANGE_16V, INA219::GAIN_2_80MV, INA219::ADC_64SAMP, INA219::ADC_64SAMP, INA219::CONT_SH_BUS);
  monitor.configure(INA219::RANGE_16V, INA219::GAIN_1_40MV, INA219::ADC_64SAMP, INA219::ADC_64SAMP, INA219::CONT_SH_BUS);
  
  // calibrate with our values
  monitor.calibrate(SHUNT_R, SHUNT_MAX_V, BUS_MAX_V, MAX_CURRENT);

}

void loop(){
  String jsonMsg="{\"";
  ArduinoOTA.handle();
//    Serial.print("raw shunt voltage: ");
//  Serial.print(monitor.shuntVoltageRaw());

  
//  Serial.print(", raw bus voltage:   ");
//  Serial.print(monitor.busVoltageRaw());
  
//  Serial.println("--");
  
//  Serial.print(",shunt voltage: ");
//  Serial.print(monitor.shuntVoltage() * 1000, 4);
//  Serial.print(" mV,");
  jsonMsg +="Vshunt\":";
  jsonMsg +=monitor.shuntVoltage()*1000;
  jsonMsg +=",\"";
  
//  Serial.print("shunt current: ");
//  Serial.print(monitor.shuntCurrent() * 1000, 4);
//  Serial.print(" mA,");

  jsonMsg +="I\":";
  jsonMsg += monitor.shuntCurrent();
  jsonMsg += ",";
  
//  Serial.print("bus voltage:   ");
//  Serial.print(monitor.busVoltage(), 4);
//  Serial.print(" V,");

  jsonMsg +="\"Vbus\":";
  jsonMsg += monitor.busVoltage();
  jsonMsg += ",";
  
//  Serial.print("bus power:     ");
//  Serial.print(monitor.busPower() * 1000, 4);
//  Serial.print(" mW,");

  jsonMsg +="\"P\":";
  jsonMsg += monitor.busPower();
  jsonMsg += "}";
  
//  Serial.println(" ");
//  Serial.println(jsonMsg);
  char buf[500];
  jsonMsg.toCharArray(buf, jsonMsg.length()+2);
  buf[jsonMsg.length()]='{';
  buf[jsonMsg.length()]='\0';
 
//  Serial.println(buf);
  ws.binaryAll((char*)buf);



  delay(1000);
}

void checkcmd(String msg){
  //      const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 77;
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
  }else{
  JsonObject obj = doc.as<JsonObject>();
  Serial.println(F("Response:"));
  Serial.println(obj[String("type")].as<String>());
  Serial.println(obj[String("name")].as<String>());
  Serial.println(obj[String("range")].as<int>());
  Serial.println(obj[String("gain")].as<int>());
  Serial.println(obj[String("samples")].as<int>());
  Serial.println(obj[String("pga")].as<int>());


  String msgType=obj[String("type")].as<String>();
  String cmdName=obj[String("name")].as<String>();
  if(msgType=="command"){
    if(cmdName=="inasettings"){

      

  int range=obj[String("range")].as<int>();
  int gain=obj[String("gain")].as<int>();
  int samples=obj[String("samples")].as<int>();
  int pga=obj[String("pga")].as<int>();
  float shunt_r=obj[String("shuntresistance")].as<float>();
    
  
  INA219::t_range rrange=static_cast<INA219::t_range>(range);
  INA219::t_gain ggain=static_cast<INA219::t_gain>(gain);
  INA219::t_adc ssamples=static_cast<INA219::t_adc>(samples);
  Serial.println("typecast ok");
 // monitor.reset();
  monitor.begin();
  //yield();
  monitor.configure(rrange, ggain, ssamples, ssamples, INA219::CONT_SH_BUS);
  monitor.calibrate(shunt_r, SHUNT_MAX_V, BUS_MAX_V, MAX_CURRENT);
  Serial.println("reinit success");

    }}
    if(cmdName=="shuntsettings")    {
   /* 
        int range=obj[String("range")].as<int>();
        int gain=obj[String("gain")].as<int>();
        int samples=obj[String("samples")].as<int>();
   */
        float shunt_r=obj[String("shuntresistance")].as<float>();
    
 //       monitor.calibrate(shunt_r, SHUNT_MAX_V, BUS_MAX_V, MAX_CURRENT);

    
    }
}
}
