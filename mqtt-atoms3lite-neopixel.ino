#include <M5Unified.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h"

WiFiClient wifi_client;
void mqtt_sub_callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqtt_client(mqtt_host, mqtt_port, mqtt_sub_callback, wifi_client);

#include <Adafruit_NeoPixel.h>

#define PIN 1 // G1 pin is the outer pin of AtomS3Lite's Grove connector.
#define LED_NUM 8

Adafruit_NeoPixel button_led(1, 35, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led_strip(LED_NUM, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 19200;
  M5.begin(cfg);

  // Wifi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setSleep(false);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    switch (count % 4) {
      case 0:
        Serial.println("|");
        button_led.setPixelColor(0, button_led.Color(10, 10, 0));
        button_led.show();
        break;
      case 1:
        Serial.println("/");
        break;
      case 2:
        button_led.setPixelColor(0, button_led.Color(0, 0, 0));
        button_led.show();
        Serial.println("-");
        break;
      case 3:
        Serial.println("\\");
        break;
    }
    count++;
    if (count >= 240) reboot();  // 240 / 4 = 60sec
  }
  button_led.setPixelColor(0, button_led.Color(10, 10, 0));
  button_led.show();
  Serial.println("WiFi connected!");
  delay(1000);

  // MQTT
  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  } else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    Serial.println("mqtt connecting failed...");
    reboot();
  }
  Serial.println("MQTT connected!");

  button_led.setPixelColor(0, button_led.Color(0, 0, 10));
  button_led.show();

  Serial.print("Subscribe : topic=");
  Serial.println(mqtt_subscribe_topic);
  mqtt_client.subscribe(mqtt_subscribe_topic);

  delay(1000);

  // configTime
  configTime(9 * 3600, 0, "ntp.nict.jp");
  struct tm t;
  if (!getLocalTime(&t)) {
    Serial.println("getLocalTime() failed...");
    delay(1000);
    reboot();
  }
  Serial.println("configTime() success!");
  button_led.setPixelColor(0, button_led.Color(0, 10, 0));
  button_led.show();

  delay(1000);
}

void reboot() {
  Serial.println("REBOOT!!!!!");
  for (int i = 0; i < 30; ++i) {
    button_led.setPixelColor(0, button_led.Color(255, 0, 255));
    delay(100);
    button_led.setPixelColor(0, button_led.Color(0, 0, 0));
    delay(100);
  }

  ESP.restart();
}

void loop() {
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected...");
    reboot();
  }

  M5.update();
}

#define BUF_LEN 256
char buf[BUF_LEN];

//
// format:
//     #RRGGBB, #RRGGBB, #RRGGBB, ....
//
// exampple
//     #ff0000, #00ff00, #0000ff, ....
//
#define COMMAND_BUF_LEN 16
void mqtt_sub_callback(char* topic, byte* payload, unsigned int length) {
  char buf[COMMAND_BUF_LEN];
  String color_str;

  int idx = 0;
  char *tp;
  tp = strtok((char*)payload, ",");
  while (tp != NULL) {
    memset(buf, 0, COMMAND_BUF_LEN);

    int len = strlen(tp) < COMMAND_BUF_LEN - 1 ? strlen(tp) : COMMAND_BUF_LEN - 1;
    strncpy(buf, tp, len);

    // trim space & semicolon
    color_str = String(buf);
    color_str.trim();
    if (color_str.substring(color_str.length() - 1, color_str.length()) == ",") {
      color_str = color_str.substring(0, color_str.length() - 1);
    }

    Serial.print(idx);
    Serial.print("=");
    Serial.println(color_str);
    set_led_color(idx, color_str);

    idx ++;
    if (LED_NUM <= idx) break;

    tp = strtok(NULL, ",");
  }
  led_strip.show();
}

void set_led_color(int idx, String &color_str) {
  if (color_str.length() != 7) return;

  // check header
  if (color_str.substring(0, 1) != "#") return;

  String str_r = color_str.substring(1, 3);
  String str_g = color_str.substring(3, 5);
  String str_b = color_str.substring(5, 7);

  char *pos;
  uint8_t r = (uint8_t)strtol(str_r.c_str(), &pos, 16);
  uint8_t g = (uint8_t)strtol(str_g.c_str(), &pos, 16);
  uint8_t b = (uint8_t)strtol(str_b.c_str(), &pos, 16);

  led_strip.setPixelColor(idx, led_strip.Color(r,  g,  b));
}
