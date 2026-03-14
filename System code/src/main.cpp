#define TINY_GSM_MODEM_SIM7600
#include <ESP_SSLClient.h>
#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>
#include <TinyGPSPlus.h>
#include <Adafruit_INA219.h>

const long Baudrate = 115200;
const long Baudrate2 = 9600;
const char RX_Pin = 16;
const char TX_Pin = 17;
const char firebase[] = "tracking-system-database-default-rtdb.europe-west1.firebasedatabase.app";
String path = "/test.json?auth=mBxdxNNC3idfnxM7vmPzJ9cqHKF0s5bvcLxt8L8g";
const char apn[] = "everywhere";
unsigned long lastmsg = 0;

TinyGPSPlus GPS;
Adafruit_INA219 ina;

HardwareSerial sim(1);
TinyGsm modem(sim);
TinyGsmClient baseClient(modem, 0);
ESP_SSLClient sslClient;
HttpClient client(sslClient, firebase, 443);



void modemSetup() {
  Serial.println("initalising modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem");
  } else {
    Serial.println("Modem restarted ");
  }

  String name = modem.getModemName();
  Serial.println("Modem: " + name);

  String imei = modem.getIMEI();
  Serial.println("IMEI: " + imei);

  Serial.println("waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println("network timeout");
    return;
  }

  Serial.println("connecting to APN: " + String(apn));
  if (!modem.gprsConnect(apn, "","")) {
    Serial.println("GPRS connection failed");
    return;
  }
  Serial.print("moden connected to network");

  sslClient.setClient(&baseClient);
  sslClient.setInsecure();
  sslClient.setBufferSizes(1024, 512);

  Serial.println("Setup complete");
}


void upload() {
  String Latitude = String(GPS.location.lat(),6);
  String Longitude = String(GPS.location.lng(),6);
  float busVoltage = ina.getBusVoltage_V();
  float current_mA = ina.getCurrent_mA();
  float power_mW = ina.getPower_mW();


  //Serial output
  Serial.println("=======================");
  Serial.printf("Timestamp: %lu ms | GPS Time: %02d:%02d:%02d UTC\r\n", millis(), GPS.time.hour(), GPS.time.minute(), GPS.time.second());
  Serial.printf("Satellites: %i\r\n", GPS.satellites.value());
  Serial.printf("Longitude: %s\r\n", Longitude.c_str());
  Serial.printf("Latitude: %s\r\n", Latitude.c_str());
  Serial.printf("Voltage: %.2f V\r\n", busVoltage);
  Serial.printf("Current: %.2f mA\r\n", current_mA);
  Serial.printf("Power: %.2f mW\r\n", power_mW);

  
  //send to DB
  String payload = "{";
  payload += "\"Tm\":" + String(millis());

  if (GPS.location.isValid()) {
    payload += ",\"Lat\":" + String(GPS.location.lat(), 6);
    payload += ",\"Lng\":" + String(GPS.location.lng(), 6);
  } else {
    payload += ",\"Lat\":null,\"Lng\":null";
  }
  payload += "}";

  //DB UPLOAD
  Serial.println("Uploading to Firebase...");
  Serial.println("Payload" + payload);

  client.beginRequest();
  client.post(path);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", payload.length());
  client.beginBody();
  client.print(payload);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("response ");
  Serial.println(response);

}



void setup() {
  Serial.begin(115200);
  ina.begin();
  sim.begin(Baudrate, SERIAL_8N1, RX_Pin, TX_Pin);
  Serial2.begin(Baudrate2, SERIAL_8N1, 26, 25);
  modemSetup();
  delay(1000);
  sslClient.setClient(&baseClient);
}

void loop() {
  static unsigned long lastUpload = 0;
  static unsigned long lastMsg = 0;


  while (Serial2.available() > 0) {
      char incomingByte = Serial2.read();
      GPS.encode(incomingByte);
  }

  if (sim.available()) {
    Serial.write(sim.read());
  }
    if (Serial.available()) {
    sim.write(Serial.read());
  }
  
  if (GPS.location.isUpdated() && GPS.location.isValid()) {
    if (millis() - lastUpload > 5000) { // every 5s
        upload();
        lastUpload = millis();
    }
  } else {
    if (millis() - lastMsg > 5000) { // every 5s
        Serial.println("waiting for satellite lock...");
        lastMsg = millis();
    }
  }
}
