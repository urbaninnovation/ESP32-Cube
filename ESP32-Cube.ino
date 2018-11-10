#include <WiFi.h>
#include "secret.h"
const char* ssid     = SECRET_SSID;
const char* password = SECRET_WLANPWD;
WiFiServer server(80);
IPAddress local_IP(192, 168, 1, 110);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

#include "DHT.h"
#define DHTPIN 23     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

#include <TM1637Display.h>
#define CLK 19 // GRUEN
#define DIO 21 // GELB
TM1637Display display(CLK, DIO);

int t = 0;
unsigned long time2;
int counter = 0;

String default_res="<a href=\"/\">ESP32</a><br>Click <a href=\"/H\">here</a> to turn the LED on.<br>Click <a href=\"/L\">here</a> to turn the LED off.<br><br>TEMP: ";
int stepms=5000;

void blink(); 

void setup()
{
    Serial.begin(115200);
    pinMode(2, OUTPUT);
    display.setBrightness(0x00, true);
    display.showNumberDecEx(local_IP[3], 0b00000000, false, 4, 0);
    //uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    //display.setSegments(data);
    blink(5,50);
    if (!WiFi.config(local_IP, gateway, subnet)) {Serial.println("STA Failed to configure");}
    Serial.print("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {delay(500); Serial.print(".");}
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    blink(1,800);

    server.begin();
    dht.begin();
}

void loop(){
  WiFiClient client = server.available();

  if (abs(millis()-time2)>stepms)
  {
    int number=counter;
    time2 = millis();
    int t_new=read_dht22();
    if (t_new<10000) {t=t_new;}
    if (t>0) {number=t;}
    display.showNumberDec(number, false, 4);
  }

  if (client) {                         
	//Serial.println("HTTP-request.");
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
		  if (currentLine.equals("GET / ")) {display.showNumberDec(++counter, false, 4); res=default_res+String(t);}
		  else if (currentLine.equals("GET /H ")) {digitalWrite(2, HIGH); display.showNumberDec(++counter, false, 4); res=default_res+String(t);}
		  else if (currentLine.equals("GET /L ")) {digitalWrite(2, LOW); display.showNumberDec(++counter, false, 4); res=default_res+String(t);}
		  else if (currentLine.equals("GET /ON ")) {display.setBrightness(0x00, true); res=default_res+String(t);}
		  else if (currentLine.equals("GET /OFF ")) {display.setBrightness(0x00, false); res=default_res+String(t);}
        }
      }
    }
    client.stop();
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
  //float h = dht.readHumidity();
  float t = dht.readTemperature();
  //float f = dht.readTemperature(true);
  return int(t*100);
}
