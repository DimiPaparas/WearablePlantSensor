#include <LMP91000.h>
#include <Wire.h>
#include <WiFiNINA.h>  // WiFi library for the Arduino Nano 33 IoT
 
LMP91000 pStat[4];
const int MENB[4] = {2, 3, 4, 5};

const int16_t opVolt = 3300; //milliVolts if working with a 3.3V device
const uint8_t resolution = 10; //10-bits

char ssid[] = "Leaf_Sensor_AP";
char pass[] = "password123";

IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer server(123);

int sensorValue[4];
const int biasValue = -7;

int status = WL_IDLE_STATUS;

void setup() {
  Wire.begin();
  Serial.begin(9600);

  // Initialize all MENB pins and potentiostats in bulk
  for (int i = 0; i < 4; i++) {
    pinMode(MENB[i], OUTPUT);
    digitalWrite(MENB[i], LOW);
    initPotentiostat(pStat[i], 2);
  }
  
  delay(2000);

  startAccessPoint();
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
      client.print("400");
      int16_t pre_stepV[] = {0,0,0,0};
      // 1V = 1000 and 24% of 3.3V is 792
      // anything higher will make the chip
      // output zero
      int16_t v1[] = {100,200,400,700};
      int16_t v2[] = {0,0,0,0};
      runAmp(
        10, 6,
        pre_stepV, v1, v2,
        1000, 1000, 1000,
        client
      );
      client.flush();
      delay(500);
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
  uint16_t samples, uint8_t range,
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
    voltageArray[j][0] = pre_stepV[j];
    voltageArray[j][1] = v1[j];
    voltageArray[j][2] = v2[j];
  }

  //i = 0 is pre-step voltage
  //i = 1 is first step potential
  //i = 2 is second step potential
  for(uint8_t i = 0; i < 3; i++)
  {
    uint32_t fs = timeArray[i]/samples;

    for(uint8_t j = 0; j < 4; j++){  
      if(voltageArray[j][i] < 0) pStat[j].setNegBias();
      else pStat[j].setPosBias();
    
      pStat[j].setBias(abs(voltageArray[j][i]));
    }

    unsigned long startTime = millis();
    while(millis() - startTime < timeArray[i])
    {
      String data = "";
      for(uint8_t j = 0; j < 4; j++){
        // Serial.print((uint16_t)(opVolt*TIA_BIAS[abs(voltageArray[j][i])]*(voltageArray[j][i]/abs(voltageArray[j][i]))));
        // Serial.print(",");
        // Serial.print(millis());
        // Serial.print(",");
        // Serial.println(pow(10,range)*pStat[j].getCurrent(pStat[j].getOutput(A0), opVolt/1000.0, resolution));

        data +=
          String((uint16_t)(opVolt * TIA_BIAS[abs(voltageArray[j][i])] * (voltageArray[j][i] / abs(voltageArray[j][i])))) + ","
          + String(millis()) + ","
          + String(pow(10, range) * pStat[j].getCurrent(pStat[j].getOutput(A0), opVolt / 1000.0, resolution)) + ",";

        // Send the data over serial and WiFi
        // Serial.print(data);
      }
      data += "\n";
      client.print(data);
      // Serial.print(data);
      delay(fs);
    }
  }

  //End at 0V
  for(uint8_t j = 0; j < 4; j++){
    pStat[j].setBias(0);
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
