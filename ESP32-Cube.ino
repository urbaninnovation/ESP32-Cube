#include <WiFi.h>
#include "config.h"
const char* ssid     = SSID;
const char* password = PWD;
WiFiServer server(80);
byte myIP[] = IP;
IPAddress local_IP(myIP[0],myIP[1],myIP[2],myIP[3]);
byte myGW[] = GATEWAY;
IPAddress gateway(myGW[0],myGW[1],myGW[2],myGW[3]);
byte mySN[] = SUBNET;
IPAddress subnet(mySN[0],mySN[1],mySN[2],mySN[3]);

#include <DHT.h>
#define DHTPIN 23
#define DHTTYPE DHT22 // DHT 22 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

#include <TM1637Display.h>
#define CLK 19
#define DIO 21
TM1637Display display(CLK, DIO);

unsigned long timeflag = 0; // millis at last update
int counter = 0; // current counter
int temp = 0; // current temperature
int number=0; // current number on display
int stepms=5000; // ms to wait between updates
int art_z=0;
int art_d=0;
unsigned long art_timestamp=0;
byte art_data[] = { 0b00000001, 0b00000010, 0b00000100, 0b00001000 };

void setup()
{
	Serial.begin(115200);
	pinMode(2, OUTPUT);
	blink(2,50);
	display.setBrightness(0x00, true);
	uint8_t data[] = { 0b01001001, 0b01001001, 0b01001001, 0b01001001 };
	display.setSegments(data);
	if (!WiFi.config(local_IP, gateway, subnet)) {Serial.println("STA Failed to configure");}
	Serial.print("Connecting to ");
	Serial.print(ssid);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {delay(500); Serial.print(".");}
	Serial.print("WiFi connected. IP address: ");
	Serial.println(WiFi.localIP());
	display.showNumberDecEx(myIP[3], 0b00000000, false, 4, 0);
	blink(1,800);
	server.begin();
	dht.begin();
}

void loop(){
	art(0,0);
	if (abs(millis()-timeflag)>stepms) {timeflag = millis(); temp=read_dht22(); if (temp>0&&temp<10000) {updateDisplay(temp);};}

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
					else if (currentLine.equals("GET /ART ")) {res=assambleRES(); art(480,80);}
				}
			}
		}
		client.stop();
	}
}

String assambleRES() {
	++counter;
	art(12,40);
	return "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body style=font-size:2em><a href=\"/\">ESP32-Cube</a> (<a href=https://github.com/urbaninnovation/ESP32-Cube>GitHub</a>)<br>LED <a href=\"/H\">ON</a> | <a href=\"/L\">OFF</a><br>DISPLAY <a href=\"/ON\">ON</a> | <a href=\"/OFF\">OFF</a><br><a href=\"/ART\">START DISPLAY ART</a><br>TEMP: "
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
	return int(t*100);
}

void updateDisplay(int n) {
	display.showNumberDec(n, false, 4);
}