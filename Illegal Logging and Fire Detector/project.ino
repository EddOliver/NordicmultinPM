#include <Adafruit_SGP30.h>
#include <Arduino.h>
#include <XIAO_inferencing.h>
#include <Wire.h>

// Classes Sensor TVOC
Adafruit_SGP30 sgp;

// LoRaWAN Define
#define SEC 30
// LoRaWAN Vars
unsigned long last_lora_interval_millis = 0;
static char recv_buf[512];
static bool is_exist = true;
static bool is_join = true;
static int led = 0;
int INTERVAL_LORA_MILLIS = 0;
// LoRaWAN Functions
static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...);
static void recv_prase(char *p_msg);
// Edge Impulse Define
#define FREQUENCY_HZ 4000
#define NUM 3
#define INTERVAL_MICROS (1000000 / (FREQUENCY_HZ))
// Edge Impulse Vars
static unsigned long last_interval_micros = 0;
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {};
int indexes = 0;
int counter = 0;
// Edge Impulse Functions
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr);
int get_inference_result(ei_impulse_result_t result);
// Sensor Vars
int counterSensor = 0;

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // LoRaWAN
  Serial.begin(115200);
  delay(100);
  Serial1.begin(9600);
  at_send_check_response("+MODE: LWOTAA", 1000, "AT+MODE=LWOTAA\r\n");
  at_send_check_response("+DR: US915", 1000, "AT+DR=US915\r\n");
  at_send_check_response("+DR: DR3", 1000, "AT+DR=DR3\r\n");
  at_send_check_response("+ADR: OFF", 1000, "AT+ADR=OFF\r\n");
  at_send_check_response("+KEY: APPKEY", 1000, "AT+KEY=APPKEY,\"XXXXXXXXXXXXXXXXXXXXXXXXXXXX\"\r\n");
  at_send_check_response("+PORT: 8", 1000, "AT+PORT=8\r\n");
  at_send_check_response("+POWER: 14", 1000, "AT+POWER=14\r\n");
  Serial.println("Lora OK");
  // TVOC Sensor
  if (!sgp.begin()) {
    while (1)
      ;
  }
  Serial.println("Sensor OK");
}

void loop(void) {
  // LoRaWAN
  if (millis() > last_lora_interval_millis + INTERVAL_LORA_MILLIS) {
    INTERVAL_LORA_MILLIS = 30000;
    last_lora_interval_millis = millis();
    int ret1 = 0;
    int ret2 = 0;
    if (is_join) {
      while (is_join) {
        ret1 = at_send_check_response("+JOIN: Network joined", 12000, "AT+JOIN\r\n");
        ret2 = at_send_check_response("+JOIN: Joined already", 12000, "AT+JOIN\r\n");
        if (ret1 || ret2) {
          is_join = false;
          digitalWrite(LED_BUILTIN, LOW);
        } else {
          delay(5000);
        }
      }
    } else {
      // Sensor Read
      Serial.println("LORA");
      if (!sgp.IAQmeasure()) {
        return;
      }
      uint16_t TVOC = sgp.TVOC;
      if (!sgp.IAQmeasureRaw()) {
        return;
      }
      counterSensor++;
      if (counterSensor == 30) {
        counterSensor = 0;
        uint16_t TVOC_base, eCO2_base;
        if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
          return;
        }
      }
      // Append Everything on Message
      char cmd[128];
      sprintf(cmd, "AT+CMSG=\"{'result':'%d','fire':'%u'}\"", indexes, (unsigned int)TVOC);
      Serial.println(cmd);
      at_send_check_response("Done", 10000, cmd);
    }
  }
  // Run Classifier
  if (micros() > last_interval_micros + INTERVAL_MICROS) {
    last_interval_micros = micros();
    features[counter] = analogRead(A1);
    counter++;
    if (counter == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1) {
      if (sizeof(features) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        ei_printf("The size of your 'features' array is not correct. Expected %lu items, but had %lu\n",
                  EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(features) / sizeof(float));
        delay(1000);
        return;
      }
      ei_impulse_result_t result = { 0 };
      // the features are stored into flash, and we don't want to load everything into RAM
      signal_t features_signal;
      features_signal.total_length = sizeof(features) / sizeof(features[0]);
      features_signal.get_data = &raw_feature_get_data;
      // invoke the impulse
      EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
      if (res != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", res);
        return;
      }
      indexes = get_inference_result(result);
      ei_printf("%s\r\n", ei_classifier_inferencing_categories[indexes]);
      counter = 0;
    }
  }
}

// Functions

// LoRaWAN

static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...) {
  int ch;
  int num = 0;
  int index = 0;
  int startMillis = 0;
  memset(recv_buf, 0, sizeof(recv_buf));
  Serial1.print(p_cmd);
  Serial.print(p_cmd);
  delay(200);
  startMillis = millis();

  if (p_ack == NULL) {
    return 0;
  }

  do {
    while (Serial1.available() > 0) {
      ch = Serial1.read();
      recv_buf[index++] = ch;
      Serial.print((char)ch);
      delay(2);
    }

    if (strstr(recv_buf, p_ack) != NULL) {
      return 1;
    }

  } while (millis() - startMillis < timeout_ms);
  return 0;
}

static void recv_prase(char *p_msg) {
  if (p_msg == NULL) {
    return;
  }
  char *p_start = NULL;
  int data = 0;
  int rssi = 0;
  int snr = 0;

  p_start = strstr(p_msg, "RX");
  if (p_start && (1 == sscanf(p_start, "RX: \"%d\"\r\n", &data))) {
    Serial.println(data);
    led = !!data;
    if (led) {
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

// Edge Impulse

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

int get_inference_result(ei_impulse_result_t result) {
  int check = 0;
  float temp = result.classification[0].value;
  for (uint16_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > temp) {
      temp = result.classification[i].value;
      check = i;
    }
  }
  return check;
}