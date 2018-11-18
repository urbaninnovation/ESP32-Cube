#include <WiFi.h>
#include "config.h"
char* ssid[]     = SSID;
char* password[] = PWD;
byte numberofssids = NUMBER_OF_SSIDS;
WiFiServer server(80);
byte myIP[] = IP; // { 0, 0, 0, 0 }
IPAddress local_IP(myIP[0],myIP[1],myIP[2],myIP[3]);
byte myGW[] = GATEWAY; // { 192, 168, 1, 1 }
IPAddress gateway(myGW[0],myGW[1],myGW[2],myGW[3]);
byte mySN[] = SUBNET; // { 255, 255, 255, 0 }
IPAddress subnet(mySN[0],mySN[1],mySN[2],mySN[3]);

#define ESP32
#include <SocketIOClient.h>
SocketIOClient sIOclient;
char host[] = SOCKETIOHOST; // "192.168.1.221"
int port = SOCKETIOPORT; // 3000
extern String RID;
extern String Rname;
extern String Rcontent;

#include <DHT.h>
#define DHTPIN 23
#define DHTTYPE DHT22 // DHT 22 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

#include <TM1637Display.h>
#define CLK 19
#define DIO 21
TM1637Display display(CLK, DIO);

//DEEP SLEEP while PIN 33 is connected to GND
//#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex
RTC_DATA_ATTR int bootCount = 0;

unsigned long timeflag = 0; // millis at last update
int counter = 0; // current counter
int temp = 0; // current temperature
int stepms=5000; // ms to wait between updates
int art_z=0;
int art_d=0;
unsigned long art_timestamp=0;
byte art_data[] = { 0b00000001, 0b00000010, 0b00000100, 0b00001000 };
bool sIOshouldBeConnected=false;

void setup()
{
	Serial.begin(115200);
	pinMode(2, OUTPUT);
	blink(2,50);
  
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low

	display.setBrightness(0x00, true);
	uint8_t data[] = { 0b01001001, 0b01001001, 0b01001001, 0b01001001 };
	display.setSegments(data);
	dht.begin();

	byte tries=0;
	while (tries<3 && !(WiFi.status()==WL_CONNECTED)) {
    byte i=0;
    while (i<numberofssids && !connectWiFi(ssid[i],password[i])) {
      Serial.println("Failed to connect to WiFi "+String(ssid[i]));
      i++;
    }
    tries++;  
	}
}

bool connectWiFi(char* ssid,char* password) {
	if (myIP[3]>0) {if (!WiFi.config(local_IP, gateway, subnet)) {Serial.println("STA Failed to configure");}}
	Serial.print("Connecting to WiFi "+String(ssid));
	WiFi.begin(ssid, password);
	int wait=10;
	while (WiFi.status() != WL_CONNECTED && wait>0) {wait--; delay(500); Serial.print(".");}

	if (WiFi.status()==WL_CONNECTED) {
		Serial.print("WiFi connected. IP address: ");
		Serial.println(WiFi.localIP());
		String ip=WiFi.localIP().toString(); ip=ip.substring(ip.lastIndexOf('.')+1,ip.length());
		display.showNumberDecEx(ip.toInt(), 0b00000000, false, 4, 0);
		server.begin();
		connectSocketIO();
		blink(1,800);
		return true;	
	} else {
		blink(2,100);
		return false;
	}
}

void connectSocketIO() {
	if (sIOshouldBeConnected) {sIOclient.disconnect();}
	if (!sIOclient.connect(host, port)) {
		Serial.println("Failed to connect to SocketIO-server "+String(host));
	}
	if (sIOclient.connected()) {
		Serial.println("Connected to SocketIO-server "+String(host));
		sIOshouldBeConnected=true;
	}	
}

void loop(){
	art(0,0);
	if (abs(millis()-timeflag)>stepms) {
		Serial.print('.');
		timeflag = millis();
		if (art_z<1) {temp=read_dht22(); if (temp!=0&&temp<10000) {Serial.println();Serial.println("TEMP: "+String(temp)); updateDisplay(temp);};}
		
		if (sIOshouldBeConnected) {
			if (!sIOclient.connected()) {sIOclient.disconnect(); sIOshouldBeConnected=false;} 
			else {sIOclient.heartbeat(1);}
		}
		if (!sIOshouldBeConnected) {blink(5,50); Serial.println();Serial.println("Not connected to SocketIO-server "+String(host)); connectSocketIO();}
	}

	if (sIOshouldBeConnected && sIOclient.monitor())
	{
		blink(1,50);
		Serial.print(RID+", ");
		Serial.print(Rname+", ");
		Serial.println(Rcontent);
		if (Rname=="time") {art_z=0; display.showNumberDecEx(Rcontent.toInt(), 0b01000000, true, 4, 0);}
	}

	WiFiClient client = server.available();
	if (client) {                         
		String currentLine = "";
		String res = "";
		while (client.connected()) {
			if (client.available()) {
				char c = client.read();
				if (c == '\n') {
					
					if (currentLine.length() == 0) {
						if (res.length()==0) {client.println("HTTP/1.1 404 NOT FOUND");}
						else {
							client.println("HTTP/1.1 200 OK");
							client.println("Content-type:text/html");
							client.println();
							client.print(res);
							Serial.print('*');
							blink(1,50);
						}
						break;
					}
					else {currentLine = "";}

				} else if (c != '\r') {
					currentLine += c;
					if (currentLine.equals("GET / ")) {res=assambleRES();}
					else if (currentLine.equals("GET /H ")) {digitalWrite(2, HIGH); res=assambleRES();}
					else if (currentLine.equals("GET /L ")) {digitalWrite(2, LOW); res=assambleRES();}
					else if (currentLine.equals("GET /ON ")) {display.setBrightness(0x00, true); res=assambleRES();}
					else if (currentLine.equals("GET /OFF ")) {display.setBrightness(0x00, false); res=assambleRES();}
					else if (currentLine.equals("GET /ART ")) {res=assambleRES(); art(8480,80);}
					else if (currentLine.equals("GET /TIME ")) {res=assambleRES(); sIOclient.send("broadcast","get","time");}
          else if (currentLine.equals("GET /CONNECT ")) {connectSocketIO(); res=assambleRES();}
          else if (currentLine.equals("GET /SLEEP")) {display.setBrightness(0x00, false); updateDisplay(0); digitalWrite(2, true); client.stop(); goToDeepSleep();}
				}
			}
		}
		client.stop();
	}
}

String assambleRES() {
	++counter;
	art(12,40);
	String sid="not connected";
	if (sIOshouldBeConnected) {sid=sIOclient.sid;}
	return "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body style=font-size:2em><a href=\"/\">ESP32-Cube</a> (<a href=https://github.com/urbaninnovation/ESP32-Cube>GitHub</a>)<br>LED <a href=\"/H\">ON</a> | <a href=\"/L\">OFF</a><br>DISPLAY <a href=\"/ON\">ON</a> | <a href=\"/OFF\">OFF</a><br><a href=\"/ART\">START DISPLAY ART</a><br><a href=\"/SLEEP\">DEEP SLEEP (till PIN 33 not GND)</a><br><a href=\"/CONNECT\">CONNECT TO SERVER</a><br><a href=\"/TIME\">REQUEST TIME</a><br>SID: "
	+String(sid)
	+"<br>TEMP: "
	+String(temp)
	+"<br>COUNTER: "
	+String(counter)
	+"</body></html>";
}

void art(int z, int d) {
	if (z>0||d>0) {art_z=z; art_d=d;} 
	else if (art_z>0) {
		if (abs(millis()-art_timestamp)>art_d) {
			art_z--;
			art_timestamp=millis();
			for (int a=0; a < 4; a++) {
				art_data[a]=art_data[a]<<1;
				if (art_data[a]==0b01000000) {art_data[a]=0b00000001;}
			}
			display.setSegments(art_data);
			if (art_z<=0) {updateDisplay(counter);}
		}
	}
}

void blink(int z, int d) {
	for (int i=0; i < z; i++){
		digitalWrite(2, !digitalRead(2));
		delay(d);
		digitalWrite(2, !digitalRead(2));
		delay(d);
	}
}

int read_dht22() {
	float t = dht.readTemperature();
	//float h = dht.readHumidity();
	return int(t*10);
}

void updateDisplay(int n) {
	display.showNumberDec(n, false, 4);
}

void goToDeepSleep() {
  //DEEP SLEEP while PIN 33 is connected to GND
  Serial.println("Going to sleep now");
  delay(1000);
  esp_deep_sleep_start();
}
