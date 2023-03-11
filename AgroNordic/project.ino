#include <ArduinoBLE.h>
#include <SensirionI2CSht4x.h>
#include <Wire.h>

// BLE Services and Chars Class
BLEService EnvironmentalService("181A");
BLEIntCharacteristic moiCharacteristic("0540", BLERead | BLENotify);     // Moisture
BLEFloatCharacteristic tempCharacteristic("0543", BLERead | BLENotify);  // Temperature
BLEFloatCharacteristic humCharacteristic("0544", BLERead | BLENotify);   // Humidity

// Sensor Class
SensirionI2CSht4x sht4x;

// Sensor Vars
float temperature;
float humidity;

void updateSHT(void);
void updateMOISTURE(void);

// General
long prevMillis;

void setup() {
  // Serial Setup
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Serial OK");

  // LED
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("LED OK");

  // BLE Setup
  if (!BLE.begin()) {
    Serial.println("failed to initialize BLE!");
    while (1)
      ;
  }

  BLE.setLocalName("AgroNordic");
  BLE.setAdvertisedService(EnvironmentalService);
  EnvironmentalService.addCharacteristic(moiCharacteristic);
  EnvironmentalService.addCharacteristic(tempCharacteristic);
  EnvironmentalService.addCharacteristic(humCharacteristic);
  BLE.addService(EnvironmentalService);
  Serial.println("BLE OK");

  // Setup Sensors

  Wire.begin();
  sht4x.begin(Wire);
  Serial.println("Sensor OK");

  // Set First Value

  updateSHT();
  updateMOISTURE();
  prevMillis = millis();

  // Start Advertise

  BLE.advertise();
  Serial.println("BLE Adv OK");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    digitalWrite(LED_BUILTIN, LOW);

    while (central.connected()) {
      if (millis() - prevMillis >= 1 * 30 * 1000) {  // 30 Seconds Update
        prevMillis = millis();
        updateSHT();
        updateMOISTURE();
      }
    }

    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void updateSHT(void) {
  sht4x.measureHighPrecision(temperature, humidity);
  humCharacteristic.setValue(humidity);
  tempCharacteristic.setValue(temperature);
  Serial.println("SHT OK");
}

void updateMOISTURE(void) {
  static int value = analogRead(A0);
  moiCharacteristic.setValue(value);
  Serial.println("Moisture OK");
}