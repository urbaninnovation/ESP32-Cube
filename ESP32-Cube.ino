#include "config.h"
#include <WiFi.h>
WiFiServer server(80);
#include <Preferences.h>
Preferences preferences;
String wifiSSID, wifiPassword;

#define ESP32
#include <SocketIOClient.h>
SocketIOClient sIOclient;
//char host[] = SOCKETIOHOST; // "192.168.1.221"
//int port = SOCKETIOPORT; // 3000
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
RTC_DATA_ATTR int counter = 0; // current counter
int temp = 0; // current temperature
int stepms=10000; // ms to wait between updates
int art_z=0;
int art_d=0;
unsigned long art_timestamp=0;
byte art_data[] = { 0b00000001, 0b00000010, 0b00000100, 0b00001000 };
static volatile bool wifi_connected = false;
static volatile bool sIOshouldBeConnected=false;
static volatile bool deepsleep=false;
static volatile bool restart=false;

void setup()
{
	Serial.begin(115200);
	pinMode(2, OUTPUT);
	blink(2,50);
	
	pinMode(18, OUTPUT); digitalWrite(18,true); //PIN to serve 3.3v for TM1637Display
	pinMode(22, OUTPUT); digitalWrite(22,true); //PIN to serve 3.3v for DHT
		
	++bootCount;
	Serial.println("Boot number: " + String(bootCount));
	//DEEP SLEEP while PIN 33 is connected to GND
	//esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low
	//or while touchsensor on PIN15 isn't touched
	touchAttachInterrupt(T3, {}, 40); // T3=PIN15, Threshold=40
	esp_sleep_enable_touchpad_wakeup();

	display.setBrightness(0x03, true);
	uint8_t data[] = { 0b01001001, 0b01001001, 0b01001001, 0b01001001 };
	display.setSegments(data);
	dht.begin();

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_AP);

  loadPreferences();
	setupAP();
	connectWiFi();
	server.begin();
}

bool clearPreferences() {
	// Remove all preferences under opened namespace
	preferences.clear();
}

bool savePreferences(String qsid, String qpass) {
	preferences.begin("wifi", false);
	preferences.putString("ssid", qsid);
	preferences.putString("password", qpass);
	delay(300);
	preferences.end();
  wifiSSID = qsid;
  wifiPassword = qpass;
  loadPreferences();
}

bool loadPreferences() {
	preferences.begin("wifi", false);
  wifiSSID =  preferences.getString("ssid", "none");
  wifiPassword =  preferences.getString("password", "none");
  preferences.end();
  Serial.print("Stored SSID: ");
  Serial.println(wifiSSID);
}

bool setupAP() {
	IPAddress AP_local_IP(8,8,8,8);
	IPAddress AP_gateway(8,8,8,8);
	IPAddress AP_subnet(255,255,255,0);
	WiFi.softAPConfig(AP_local_IP, AP_gateway, AP_subnet);
	delay(100);
	WiFi.softAP(AP_SSID);
	Serial.print("Soft-AP SSID = ");
	Serial.println(AP_SSID);
	Serial.print("Soft-AP IP = ");
	Serial.println(WiFi.softAPIP());
	display.showNumberDecEx(8888, 0b00000000, false, 4, 0);
}

bool connectWiFi() {
  WiFi.mode(WIFI_MODE_APSTA);
	WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
	Serial.println("Trying to connect to WiFi "+wifiSSID+".");
	int wait=12;
	while (WiFi.status() != WL_CONNECTED && wait>0) {wait--; delay(500); Serial.print("~");}
	if (WiFi.status()==WL_CONNECTED) {
		Serial.print("WiFi connected. IP address: ");
		Serial.println(WiFi.localIP());
		String ip=WiFi.localIP().toString(); ip=ip.substring(ip.lastIndexOf('.')+1,ip.length());
		display.showNumberDecEx(ip.toInt(), 0b00000000, false, 4, 0);
		connectSocketIO();
		blink(1,800);
		return true;  
	} else {
	  WiFi.mode(WIFI_MODE_AP);
		blink(2,100);
		return false;
	}
}

void connectSocketIO() {
	if (sIOshouldBeConnected) {sIOclient.disconnect();}
	if (!sIOclient.connect(SOCKETIOHOST, SOCKETIOPORT)) {
		//Serial.println("Failed to connect to SocketIO-server "+String(host));
	}
	if (sIOclient.connected()) {
		Serial.println("Connected to SocketIO-server "+String(SOCKETIOHOST));
		sIOshouldBeConnected=true;
	} 
}

void doScanNetworks() {
  Serial.println("Scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  int i=0;
  while (i<n) {
    Serial.println(WiFi.SSID(i));
    i++;
  }
}

void loop(){
	art(0,0);
	if (abs(millis()-timeflag)>stepms) {
		Serial.print('.');
		timeflag = millis();
		if (art_z<1) {temp=read_dht22(); if (temp!=0&&temp<10000) {/*Serial.println();Serial.println("TEMP: "+String(temp));*/ updateDisplay(temp);};}
		
		if (sIOshouldBeConnected) {
			if (!sIOclient.connected()) {sIOclient.disconnect(); sIOshouldBeConnected=false;} 
			else {sIOclient.heartbeat(1);}
		}
		if (wifi_connected && !sIOshouldBeConnected) {
			blink(5,50); 
			//Serial.println();
			//Serial.println("Not connected to SocketIO-server "+String(host));
			connectSocketIO();
		}
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

	        else if (currentLine.startsWith("GET /a?ssid=") ) {
	          //GET /a?ssid=123&pass=321
	          // Remove all preferences under opened namespace
						clearPreferences();
	          String qsid;
	          qsid = urlDecode(currentLine.substring(12, currentLine.indexOf('&'))); //parse ssid
	          String qpass;
	          qpass = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); //parse password
	          //savePreferences(urlDecode(currentLine.substring(12, currentLine.indexOf('&'))),urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))));
	          savePreferences(qsid,qpass);

	          client.println("HTTP/1.1 200 OK");
	          client.println("Content-type:text/html");
	          client.println();

						client.print(assambleRES());
						// the content of the HTTP response follows the header:
	          //client.print("<h1>OK!</h1>");
	          //client.println(qsid);
	          //Serial.println("OK! "+qsid);
	          //delay(5000);
	          //ESP.restart();
	          client.stop();
	        }
						

					else {currentLine = "";}

				} else if (c != '\r') {
					currentLine += c;
					if (currentLine.equals("GET / ")) {res=assambleRES();}
					else if (currentLine.equals("GET /H ")) {digitalWrite(2, HIGH); res=assambleRES();}
					else if (currentLine.equals("GET /L ")) {digitalWrite(2, LOW); res=assambleRES();}
					else if (currentLine.equals("GET /ON ")) {display.setBrightness(0x03, true); res=assambleRES();}
					else if (currentLine.equals("GET /OFF ")) {display.setBrightness(0x00, false); res=assambleRES();}
					else if (currentLine.equals("GET /ART ")) {res=assambleRES(); art(8480,80);}
					else if (currentLine.equals("GET /TIME ")) {res=assambleRES(); sIOclient.send("broadcast","get","time");}
					else if (currentLine.equals("GET /SCANNETWORKS ")) {doScanNetworks(); res=assambleRES();}
					else if (currentLine.equals("GET /CONNECTWIFI ")) {connectWiFi(); res=assambleRES();}
					else if (currentLine.equals("GET /RESTART ")) {res=assambleRES(); restart=true;}
					else if (currentLine.equals("GET /CONNECTSOCKET ")) {connectSocketIO(); res=assambleRES();}
					else if (currentLine.equals("GET /SLEEP ")) {res=assambleRES(); deepsleep=true;}
				}

			}
		}
		client.stop();
		if (deepsleep) {goToDeepSleep();}
		if (restart) {ESP.restart();}
	}
}

String assambleRES() {
	++counter;
	art(12,40);
	String sid="not connected";
	if (sIOshouldBeConnected) {sid=sIOclient.sid;}
	return "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body style=font-size:1.5em><a href=\"/\">ESP32-Cube</a> (<a href=https://github.com/urbaninnovation/ESP32-Cube>GitHub</a>)<br>LED <a href=\"/H\">ON</a> | <a href=\"/L\">OFF</a><br>DISPLAY <a href=\"/ON\">ON</a> | <a href=\"/OFF\">OFF</a><br><a href=\"/ART\">START DISPLAY ART</a><br><a href=\"/SLEEP\">DEEP SLEEP</a> ("
	+String(bootCount)
	+")<br><a href=\"/RESTART\">RESTART</a><br><a href=\"/SCANNETWORKS\">SCAN NETWORKS</a><br><a href=\"/CONNECTWIFI\">CONNECT TO WIFI</a><br><a href=\"/CONNECTSOCKET\">CONNECT TO SERVER</a><br><a href=\"/TIME\">REQUEST TIME</a><br>SID: "
	+String(sid)
	+"<br>AP SSID: "
	+AP_SSID
	+"<br>WiFi: "
	+wifiSSID
	+"<br>IP: "
	+WiFi.localIP().toString()
	+"<br>TEMP: "
	+String(temp)
	+"<br>COUNTER: "
	+String(counter)
	+"<form method='get' action='a'><label></label><input name='ssid' style=width:20%><input name='pass' style=width:20%><input type='submit'></form>"
	+"</body></html>";
}

void art(int z, int d) {
	if (z>0||d>0) {art_z=z; art_d=d;} 
	else if (art_z>0) {
		if (abs(millis()-art_timestamp)>art_d) {
			art_timestamp=millis();
			if (--art_z<=0) {updateDisplay(counter);}
			else {
				for (int a=0; a < 4; a++) {
					art_data[a]=art_data[a]<<1;
					if (art_data[a]==0b01000000) {art_data[a]=0b00000001;}
				}
				display.setSegments(art_data);
			}
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
	//DEEP SLEEP while PIN 33 is connected to GND //or while touchsensor on PIN15 isn't touched
	display.setBrightness(0x00, false); updateDisplay(0); digitalWrite(2, true); 
	Serial.println("Going to sleep now");
	delay(1000);
	esp_deep_sleep_start();
}

String urlDecode(const String& text)
{
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)
	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))
		{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);

			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
			{
				decodedChar = ' ';
			}
			else {
				decodedChar = encodedChar;  // normal ascii char
			}
		}
		decoded += decodedChar;
	}
	return decoded;
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_AP_START:
      WiFi.softAPsetHostname(AP_SSID);
      break;
    case SYSTEM_EVENT_STA_START:
      WiFi.setHostname(AP_SSID);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      wifi_connected = true;
		  WiFi.mode(WIFI_MODE_APSTA);
		  Serial.println("STA Connected");
		  //Serial.print("STA SSID: ");
		  //Serial.println(WiFi.SSID());
		  //Serial.print("STA IPv4: ");
		  //Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      wifi_connected = false;
		  Serial.println("STA Disconnected");
		  delay(1000);
		  connectWiFi();
      break;
    default:
      break;
  }
}
