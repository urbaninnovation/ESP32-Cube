#include <WiFi.h>
const char* ssid     = "abc";
const char* password = "abc";
WiFiServer server(80);

IPAddress local_IP(192, 168, 1, 110);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

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

void setup()
{
    Serial.begin(115200);
    dht.begin();

    pinMode(2, OUTPUT);      // set the LED pin mode

    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("STA Failed to configure");
    }
    
    delay(10);

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();

    blink(5,250);
    
    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    display.setBrightness(0x0f);
    // All segments on
    display.setSegments(data);
}

void loop(){
  WiFiClient client = server.available();

  if ( (millis()>(time2+1000)) || (time2<1) )
  {
    time2 = millis();
    //Serial.println(time2);
    display.showNumberDec(t, false, 4);
    int t_new=read_dht22();
    if (t_new<10000) {t=t_new;}
  }

  
  if (client) {                         
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {

          if (currentLine.length() == 0) {
            Serial.println("TEMP:"+String(t));
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("Click <a href=\"/H\">here</a> to turn the LED on.<br>");
            client.print("Click <a href=\"/L\">here</a> to turn the LED off.<br>");
            client.print("<br>TEMP: "+String(t));
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

      
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(2, HIGH);
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(2, LOW);
        }

      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}


void blink(int z, int d) {
   for (int i=0; i < z; i++){
      digitalWrite(2, HIGH);
      delay(d);
      digitalWrite(2, LOW);
      delay(d);
   }
}

int read_dht22() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  float hif = dht.computeHeatIndex(f, h);
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.print(hif);
  Serial.println(" *F");

  //return String(t)+" Grad Celsius";
  return int(t*100);
}
