#include <LMP91000.h>
#include <Wire.h>
#include <WiFiNINA.h>  // WiFi library for the Arduino Nano 33 IoT
 
LMP91000 pstat1, pstat2, pstat3, pstat4;
 
const int menb1 = 2, menb2 = 3, menb3 = 4, menb4 = 5;
char ssid[] = "Nano33IoT_AccessPoint";
char pass[] = "password123";

IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(123);

int sensorValue1, sensorValue2, sensorValue3, sensorValue4;
const int biasValue = -7;

int status = WL_IDLE_STATUS;

void setup() {
  Wire.begin();
  Serial.begin(9600);

  pinMode(menb1, OUTPUT); pinMode(menb2, OUTPUT);
  pinMode(menb3, OUTPUT); pinMode(menb4, OUTPUT);
  digitalWrite(menb1, LOW); digitalWrite(menb2, LOW);
  digitalWrite(menb3, LOW); digitalWrite(menb4, LOW);

  configurePotentiostat(pstat1, menb1);
  configurePotentiostat(pstat2, menb2);
  configurePotentiostat(pstat3, menb3);
  configurePotentiostat(pstat4, menb4);
  delay(2000);

  startAccessPoint();
  server.begin();
  Serial.println("TCP server started.");
}

void loop() {
  // compare the previous status to the current status
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      // a device has connected to the AP
      Serial.println("Device connected to AP.");
    } else {
      // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP.");
    }
  }

  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected.");
    while (client.connected()) {
      readAndSendData(client);
      client.flush();
      delay(500);  // Throttle data sending rate
      if (client.status() != ESTABLISHED) break;
    }
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void startAccessPoint() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module failed.");
    while (true);
  }
  WiFi.config(local_IP, gateway, subnet);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("AP setup failed.");
    while (true);
  }
  Serial.print("AP IP Address: "); Serial.println(WiFi.localIP());
}

void readAndSendData(WiFiClient &client) {
  // Record timestamps for each sensor measurement
  int sensorValue1 = analogRead(A3);
  unsigned long timestamp1 = millis();
  int sensorValue2 = analogRead(A2);
  unsigned long timestamp2 = millis();
  int sensorValue3 = analogRead(A1);
  unsigned long timestamp3 = millis();
  int sensorValue4 = analogRead(A0);
  unsigned long timestamp4 = millis();

  // Create CSV formatted data with headers
  String data = "";
  data += "P1," + String(biasValue) + "," + String(sensorValue1) + "," + String(timestamp1) + "\n";
  data += "P2," + String(biasValue) + "," + String(sensorValue2) + "," + String(timestamp2) + "\n";
  data += "P3," + String(biasValue) + "," + String(sensorValue3) + "," + String(timestamp3) + "\n";
  data += "P4," + String(biasValue) + "," + String(sensorValue4) + "," + String(timestamp4) + "\n";

  // Send the data to the client
  client.print(data);
  Serial.print(data);
}


void configurePotentiostat(LMP91000 &pstat, int menb_pin) {
  pstat.setMENB(menb_pin);
  delay(50);
  pstat.standby();
  delay(50);
  pstat.disableFET();
  pstat.setGain(5);
  pstat.setRLoad(2);
  pstat.setExtRefSource();
  pstat.setIntZ(1);
  pstat.setThreeLead();
}