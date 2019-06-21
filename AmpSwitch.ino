#define VERSION "19.06"
#define NAME "AmpSwitch"

#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h> // MQTT - if you get rc=-4 you should maybe change MQTT_VERSION in library

const int connect_wait = 20; // WLAN

// Pin-Zuordnung
const uint8_t pin_ledr = 4;
const uint8_t pin_ledl = 2;
const uint8_t pin_switch = 5;
const uint8_t pin_amp1r = 14;
const uint8_t pin_amp1l = 13;
const uint8_t pin_amp2r = 16;
const uint8_t pin_amp2l = 12;

uint16_t mqttport = 1883;
const char* mqtttopic_cmd = "ampswitch/cmd";
const char* mqtttopic_state = "ampswitch/state";

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String scanned_wifis;
String html;
bool wifi_connected = false;
String eeprom_ssid;
String eeprom_pass;
String eeprom_mqttServer;
byte eeprom_configWritten; // 42 = yes




byte act_amp = 0;
byte new_amp = 1;

//////////////////////////
// Setup
//////////////////////////
void setup() {
  delay(1000);

  pinMode(pin_ledr,OUTPUT);
  pinMode(pin_ledl,OUTPUT);
  pinMode(pin_switch,INPUT);
  pinMode(pin_amp1r,OUTPUT);
  pinMode(pin_amp1l,OUTPUT);
  pinMode(pin_amp2r,OUTPUT);
  pinMode(pin_amp2l,OUTPUT);
  
  EEPROM.begin(512);
  delay(10);
  
  // EEPROM: Config Written?
  eeprom_configWritten = EEPROM.read(500);
  
  if(eeprom_configWritten == 42){ // Yes
    // EEPROM: SSID
    for(int i = 0; i < 32; i++){
      int val = EEPROM.read(i);
      if(val == 0) break;
      eeprom_ssid += char(val);
    }

    // EEPROM: Password
    for(int i = 32; i < 96; i++) {
      int val = EEPROM.read(i);
      if(val == 0) break;
      eeprom_pass += char(val);
    }

    // EEPROM: mqttServer
    for(int i = 96; i < 128; i++) {
      int val = EEPROM.read(i);
      if(val == 0) break;
      eeprom_mqttServer += char(val);
    }
  }else{
    eeprom_ssid = "";
    eeprom_pass = "";
    eeprom_mqttServer = "192.168.1.1";
  }
 
  // Connect to WiFi
  WiFi.hostname(NAME);
  if(eeprom_ssid.length() > 1){
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(eeprom_ssid.c_str(), eeprom_pass.c_str());

    for(int i = 0; i < 60; i++){
      if(WiFi.status() == WL_CONNECTED){
        wifi_connected = true;
        break;
      }else{
        delay(500);
      }
    }
  }

  if(wifi_connected){
    if(eeprom_mqttServer != ""){
      mqttClient.setServer(eeprom_mqttServer.c_str(), mqttport);
      mqttClient.setCallback(mqtt_callback);
    }
  }else{
    setupAP();
  }

  startWebServer();
}


//////////////////////////
// Loop
//////////////////////////
void loop() {
  // MQTT
  if(wifi_connected){
    if(!mqttClient.connected()) {
      mqtt_reconnect();
    }else {    
      mqttClient.loop(); // Do MQTT
    }
  }

  // Switch
  if(analogRead(pin_switch) < 100){
    if(act_amp == 1) new_amp = 2;
    else new_amp = 1;
  }

  // Change relais state
  if(act_amp != new_amp){
    if(new_amp == 1){
      digitalWrite(pin_ledr,0);
      digitalWrite(pin_amp2r,0);
      digitalWrite(pin_amp2l,0);
      delay(200);
      digitalWrite(pin_ledl,1);
      digitalWrite(pin_amp1r,1);
      digitalWrite(pin_amp1l,1);
      act_amp = 1;
    }else{
      digitalWrite(pin_ledl,0);
      digitalWrite(pin_amp1r,0);
      digitalWrite(pin_amp1l,0);
      delay(200);
      digitalWrite(pin_ledr,1);
      digitalWrite(pin_amp2r,1);
      digitalWrite(pin_amp2l,1);
      act_amp = 2;
    }
    mqttClient.publish(mqtttopic_state,String(act_amp).c_str());
  }

  // Webserver
  server.handleClient();
}

//////////////////////////
// MQTT
//////////////////////////
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char msg[length];
  for (int i = 0; i < length; i++) {
    msg[i] = payload[i];
  }

  if(String(topic) == String(mqtttopic_cmd)){
    String cmd = String(msg);
    if(cmd.startsWith("1")){
      new_amp = 1;
    }else if(cmd.startsWith("2")){
      new_amp = 2;
    }
  }
}

void mqtt_reconnect() {
  if (mqttClient.connect(NAME)) {
    mqttClient.subscribe(mqtttopic_cmd);
//  }else{
//    delay(5000); // Wait 5 seconds before retrying
  }
}



///////////////
// WiFi Code //
///////////////
void setupAP(void){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  delay(100);
  
  int n = WiFi.scanNetworks();

  scanned_wifis = "<ol>";
  for (int i = 0; i < n; i++){
    scanned_wifis += "<li><a href='#' onClick=\"document.getElementById('ssid').value = '";
    scanned_wifis += WiFi.SSID(i);
    scanned_wifis += "'\">";
    scanned_wifis += WiFi.SSID(i);
    scanned_wifis += "</a></li>";
  }
  scanned_wifis += "</ol>";
  
  delay(100);
  
  WiFi.softAP(NAME,"");
}

void startWebServer(){
  server.on("/", []() {
    IPAddress ip = WiFi.softAPIP();
    html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>";
    html += NAME;
    html += "</title></head><body>";
    html += "<h3>";
    html += NAME;
    html += " v";
    html += VERSION;
    html += "</h3>";
    html += "<p>";
    html += scanned_wifis;
    html += "</p><form method='post' action='save'><table>";
    html += "<tr><td>WLAN Name:</td><td><input name='ssid' id='ssid' value='";
    html += eeprom_ssid;
    html += "'></td></tr>";
    html += "<tr><td>WLAN Passwort:</td><td><input name='pass' type='password' value='";
    if(eeprom_pass.length() > 0) html += "***";
    html += "'></td></tr>";
    html += "<tr><td>MQTT Server:</td><td><input name='mqttServer' value='";
    html += eeprom_mqttServer;
    html += "'></td></tr>";
    html += "<tr><td><input type='submit' value='speichern'></form></td>";
    html += "<td><form method='post' action='clear' style='display:inline'><input type='submit' value='zur&uuml;cksetzen'></form></td></tr>";
    html += "</table></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/save", []() {
    String new_ssid = server.arg("ssid");
    String new_pass = server.arg("pass");
    String new_mqttServer = server.arg("mqttServer");
    
    for (int i = 0; i < 501; ++i) { EEPROM.write(i, 0); } // Clear EEPROM

    // EEPROM: SSID
    for (int i = 0; i < new_ssid.length(); i++){
      EEPROM.write(i, new_ssid[i]);
    }

    // EEPROM: Password
    if(new_pass == "***") new_pass = eeprom_pass;   
    for (int i = 0; i < new_pass.length(); i++){
      EEPROM.write(32+i, new_pass[i]);
    }    

    // EEPROM: mqttServer
    for (int i = 0; i < new_mqttServer.length(); i++){
      EEPROM.write(96+i, new_mqttServer[i]);
    }

    EEPROM.write(500,42); // Config Written = yes
    EEPROM.commit();

    html = "<html><head><title>";
    html += NAME;
    html += "</title><meta http-equiv='refresh' content='10; url=/'></head><body>";
    html += "<h3>";
    html += NAME;
    html += "</h3>";
    html += "Einstellungen gespeichert - starte neu..<br>";
    html += "(wenn es nicht funktioniert, bitte hart restarten (Stecker ziehen))";
    html += "</body></html>";
    server.send(200, "text/html", html);
    delay(100);
    ESP.restart();
  });

  server.on("/clear", []() {
    for (int i = 0; i < 501; i++) { 
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    
    html = "<html><head><title>";
    html += NAME;
    html += "</title><meta http-equiv='refresh' content='10; url=/'></head><body>";
    html += "<h3>";
    html += NAME;
    html += "</h3>";
    html += "Einstellungen zur&uuml;ckgesetzt - starte neu..<br>";
    html += "(wenn es nicht funktioniert, bitte hart restarten (Stecker ziehen))";
    html += "</body></html>";
    server.send(200, "text/html", html);
    delay(100);
    ESP.restart();
  });

  server.begin();
}
