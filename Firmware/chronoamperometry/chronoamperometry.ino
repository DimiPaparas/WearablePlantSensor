#include <LMP91000.h>
#include <Wire.h>
#include <WiFiNINA.h>  // WiFi library for the Arduino Nano 33 IoT
#include <ArduinoJson.h>
 
LMP91000 pStat[4];
const int MENB[4] = {2, 3, 4, 5};
// const int MENB[4] = {5, 4, 3, 2};
const int ANALOG[4] = {A3, A2, A1, A0};

const int16_t opVolt = 3300; //milliVolts if working with a 3.3V device
const uint8_t resolution = 10; //10-bits

char ssid[] = "Leaf_Sensor_AP";
char pass[] = "Leaf_Sensor_AP_password";

IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(80);

int sensorValue[4];
const int biasValue = -7;

int status = WL_IDLE_STATUS;

// 1V = 1000 and 24% of 3.3V is 792
// anything higher will make the chip
// output zero

uint16_t sample_interval = 10;
uint8_t range = 6;
int16_t gain[4] = {2, 2, 2, 2};
int16_t pre_stepV[4] = {0, 0, 0, 0};
int16_t v1[4] = {0, 0, 0, 0};
int16_t v2[4] = {0, 0, 0, 0};
uint32_t quietTime = 1000;
uint32_t t1 = 1000;
uint32_t t2 = 1000;

void setup() {
  Wire.begin();
  Serial.begin(9600);

  // Initialize all MENB pins and potentiostats in bulk
  for (int i = 0; i < 4; i++) {
    pStat[i].setMENB(MENB[i]);
    pStat[i].enable();
    initPotentiostat(pStat[i], gain[i]);
    pStat[i].disable();
  }

  startAccessPoint();
  delay(1000);
  server.begin();
  Serial.println("TCP server started.");
}

void loop() {
  if (status != WiFi.status()) {
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      Serial.println("Device connected to AP.");
    } else {
      Serial.println("Device disconnected from AP.");
    }
  }


  WiFiClient client = server.available();

  if (client) {
    Serial.println("Client connected.");
    while (client.connected()) {
      // TODO: Somehow add the runAmp function here...
      // readAndSendData(client);

      String jsonString = client.readStringUntil('\n');
      Serial.println("Received: " + jsonString);

      if (parseJson(jsonString)) {
        // Run the function with updated values
        runAmp(sample_interval, range, pre_stepV, v1, v2, quietTime, t1, t2, client);
        client.print("OK\n");
      } else {
        Serial.println("Error parsing JSON.");
        client.print("STOP\n");
      }
      client.flush();
      // delay(500);
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
  // WiFi.config(local_IP, gateway, subnet);
  WiFi.config(local_IP);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("AP setup failed.");
    while (true);
  }
  Serial.print("AP IP Address: "); Serial.println(WiFi.localIP());
}

void readAndSendData(WiFiClient &client) {
  // Read sensor values and timestamps in bulk
  String data = "";
  for (int i = 0; i < 4; i++) {
    sensorValue[i] = analogRead(A3 - i);
    unsigned long timestamp = millis();
    data += "P" + String(i + 1) + "," + String(biasValue) + "," + String(sensorValue[i]) + "," + String(timestamp) + "\n";
  }

  client.print(data);
  Serial.print(data);
}

void initPotentiostat(LMP91000 &pStat, uint8_t user_gain) {
  pStat.standby();
  delay(50);

  pStat.disableFET();
  pStat.setGain(user_gain);
  pStat.setRLoad(0);
  pStat.setIntRefSource();
  pStat.setIntZ(1);
  pStat.setThreeLead();
  pStat.setBias(0);
}

//range = 12 is picoamperes
//range = 9 is nanoamperes
//range = 6 is microamperes
//range = 3 is milliamperes
void runAmp(
  uint16_t sample_interval, uint8_t range,
  int16_t* pre_stepV, int16_t* v1, int16_t* v2,
  uint32_t quietTime, uint32_t t1, uint32_t t2,
  WiFiClient &client
)
{
  //Print column headers
  String current = "";
  if(range == 12) current = "Current(pA)";
  else if(range == 9) current = "Current(nA)";
  else if(range == 6) current = "Current(uA)";
  else if(range == 3) current = "Current(mA)";
  else current = "SOME ERROR";
  
  Serial.println("Voltage(mV),Time(ms)," + current);

  uint32_t timeArray[3] = {quietTime, t1, t2};
  int16_t voltageArray[4][3];

    // Populate the 2D array
  for (uint8_t j = 0; j < 4; j++) {
    voltageArray[j][0] = determineLMP91000Bias(pre_stepV[j]);
    voltageArray[j][1] = determineLMP91000Bias(v1[j]);
    voltageArray[j][2] = determineLMP91000Bias(v2[j]);
    pStat[j].enable();
    pStat[j].setGain(gain[j]);
    pStat[j].disable();
  }

  //i = 0 is pre-step voltage
  //i = 1 is first step potential
  //i = 2 is second step potential
  unsigned long startTime;
  unsigned long refTime = millis();
  uint32_t fs = sample_interval;
  for(uint8_t i = 0; i < 3; i++){
    for(uint8_t j = 0; j < 4; j++){  
      pStat[j].enable();
      // if(j==3) pStat[j].disable();
      if(voltageArray[j][i] < 0) pStat[j].setNegBias();
      else pStat[j].setPosBias();
    
      pStat[j].setBias(abs(voltageArray[j][i]));
      pStat[j].disable();
    }

    if(i==0){
      startTime = millis();
    }
    unsigned long sampleTime = startTime;
    while(millis() - startTime < timeArray[i])
    {
      String data = "";
      for(uint8_t j = 0; j < 4; j++){
        pStat[j].enable();
        data +=
          String((int16_t)(opVolt * TIA_BIAS[abs(voltageArray[j][i])] * (voltageArray[j][i] / abs(voltageArray[j][i])))) + ","
          + String(millis()-refTime) + ","
          + String(pow(10, range) * pStat[j].getCurrent(pStat[j].getOutput(ANALOG[j]), opVolt / 1000.0, resolution)) + ",";
        pStat[j].disable();
      }
      data += "\n";
      client.print(data);

      if(sampleTime + fs > startTime + timeArray[i])
        break;
      
      while(sampleTime + fs > millis());
      sampleTime += fs;
    }
    startTime += timeArray[i];
  }

  //End at 0V
  for(uint8_t j = 0; j < 4; j++){
    pStat[j].enable();
    pStat[j].setBias(0);
    pStat[j].disable();
  }
}


signed char determineLMP91000Bias(int16_t voltage)
{
  signed char polarity = 0;
  if(voltage < 0) polarity = -1;
  else polarity = 1;
  
  int16_t v1 = 0;
  int16_t v2 = 0;
  
  voltage = abs(voltage);

  if(voltage == 0) return 0;
  
  for(int i = 0; i < NUM_TIA_BIAS-1; i++)
  {
    v1 = opVolt*TIA_BIAS[i];
    v2 = opVolt*TIA_BIAS[i+1];

    if(voltage == v1) return polarity*i;
    else if(voltage > v1 && voltage < v2)
    {
      if(abs(voltage-v1) < abs(voltage-v2)) return polarity*i;
      else return polarity*(i+1);
    }
  }
  return 0;
}

bool parseJson(String jsonString) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.println("JSON parsing error: " + String(error.c_str()));
    return false;
  }

  // Update parameters only if provided
  if (doc.containsKey("sample_interval")) sample_interval = doc["sample_interval"];
  if (doc.containsKey("range")) range = doc["range"];
  if (doc.containsKey("pre_stepV")) {
    JsonArray arr = doc["pre_stepV"];
    for (size_t i = 0; i < 4; i++) pre_stepV[i] = arr[i];
  }
  if (doc.containsKey("v1")) {
    JsonArray arr = doc["v1"];
    for (size_t i = 0; i < 4; i++) v1[i] = arr[i];
  }
  if (doc.containsKey("v2")) {
    JsonArray arr = doc["v2"];
    for (size_t i = 0; i < 4; i++) v2[i] = arr[i];
  }
  if (doc.containsKey("gain")) {
    JsonArray arr = doc["gain"];
    for (size_t i = 0; i < 4; i++) gain[i] = arr[i];
  }
  if (doc.containsKey("quietTime")) quietTime = doc["quietTime"];
  if (doc.containsKey("t1")) t1 = doc["t1"];
  if (doc.containsKey("t2")) t2 = doc["t2"];

  return true;
}
