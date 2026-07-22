/*********************************************************************
 * SERVER ESP32
 * - Reads DHT11 humidity and publishes it to MQTT.
 * - Receives LED blink commands from client:
 *      "r" -> start/stop red blinking
 *      "y" -> start/stop yellow blinking
 *      "g" -> start/stop green blinking
 *      "b" -> start/stop blue blinking
 * - Receives blink speed from client POT:
 *      "0".."4" -> 5 blink speeds
 *********************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ================== WiFi CONFIG ==================
const char* ssid     = "Rany";
const char* password = "12345678";

// ================== MQTT BROKER CONFIG ==================
IPAddress mqtt_server(10, 29, 218, 236);
const int mqtt_port = 1883;

// ================== MQTT TOPICS ==================
const char* TOPIC_HUMIDITY    = "proj/humidity";
const char* TOPIC_LED_CONTROL = "proj/led/control";
const char* TOPIC_LED_SPEED   = "proj/led/speed";

// ================== DHT11 ==================
#define DHTPIN   4
#define DHTTYPE  DHT11

DHT dht(DHTPIN, DHTTYPE);

// ================== LED PINS ==================
// Change these if your board uses different GPIOs
#define LED_RED     25
#define LED_YELLOW  14
#define LED_GREEN   26
#define LED_BLUE    27

// If LEDs behave opposite, change this to false
const bool LED_ACTIVE_HIGH = true;

#define LED_ON   (LED_ACTIVE_HIGH ? HIGH : LOW)
#define LED_OFF  (LED_ACTIVE_HIGH ? LOW  : HIGH)

// ================== OBJECTS ==================
WiFiClient espClient;
PubSubClient client(espClient);

// ================== HUMIDITY TIMING ==================
unsigned long lastHumidityTime = 0;
const unsigned long humidityInterval = 3000;

// ================== BLINK STATES ==================
bool redBlinkEnabled    = false;
bool yellowBlinkEnabled = false;
bool greenBlinkEnabled  = false;
bool blueBlinkEnabled   = false;

bool redPhase    = false;
bool yellowPhase = false;
bool greenPhase  = false;
bool bluePhase   = false;

// 5 speeds: slow -> fast
const unsigned long speedTable[5] = {1000, 700, 500, 300, 120};
int speedIndex = 2;

unsigned long lastBlinkToggle = 0;

// ================== FUNCTION DECLARATIONS ==================
void startWifi();
void connectBroker();
void callback(char* topic, byte* payload, unsigned int length);
void publishHumidity();

void allLedsOff();
void writeLed(int pin, bool on);
void handleLedCommand(char led);
void handleBlink();

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  allLedsOff();

  dht.begin();

  startWifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  connectBroker();
}

// ================== LOOP ==================
void loop() {
  if (!client.connected()) {
    connectBroker();
  }

  client.loop();
  handleBlink();

  unsigned long now = millis();
  if (now - lastHumidityTime >= humidityInterval) {
    lastHumidityTime = now;
    publishHumidity();
  }
}

// ================== WIFI ==================
void startWifi() {
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nWiFi connected!");
  Serial.print("Server ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// ================== MQTT CONNECT ==================
void connectBroker() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker... ");

    if (client.connect("esp32-server")) {
      Serial.println("connected!");

      client.subscribe(TOPIC_LED_CONTROL);
      client.subscribe(TOPIC_LED_SPEED);

      Serial.println("Subscribed to LED control topic.");
      Serial.println("Subscribed to LED speed topic.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retry in 3 seconds");
      delay(3000);
    }
  }
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  if (length == 0) return;

  String t = String(topic);
  char command = (char)payload[0];

  Serial.print("MSG [");
  Serial.print(topic);
  Serial.print("] = ");
  Serial.println(command);

  if (t == TOPIC_LED_CONTROL) {
    handleLedCommand(command);
  }
  else if (t == TOPIC_LED_SPEED) {
    int idx = command - '0';

    if (idx >= 0 && idx <= 4) {
      speedIndex = idx;

      Serial.print("Blink speed level = ");
      Serial.print(speedIndex);
      Serial.print(" / blink delay = ");
      Serial.print(speedTable[speedIndex]);
      Serial.println(" ms");
    }
  }
}

// ================== LED COMMAND ==================
void handleLedCommand(char led) {
  if (led == 'r') {
    redBlinkEnabled = !redBlinkEnabled;

    if (!redBlinkEnabled) {
      redPhase = false;
      writeLed(LED_RED, false);
    }

    Serial.print("Red blinking = ");
    Serial.println(redBlinkEnabled ? "ON" : "OFF");
  }

  else if (led == 'y') {
    yellowBlinkEnabled = !yellowBlinkEnabled;

    if (!yellowBlinkEnabled) {
      yellowPhase = false;
      writeLed(LED_YELLOW, false);
    }

    Serial.print("Yellow blinking = ");
    Serial.println(yellowBlinkEnabled ? "ON" : "OFF");
  }

  else if (led == 'g') {
    greenBlinkEnabled = !greenBlinkEnabled;

    if (!greenBlinkEnabled) {
      greenPhase = false;
      writeLed(LED_GREEN, false);
    }

    Serial.print("Green blinking = ");
    Serial.println(greenBlinkEnabled ? "ON" : "OFF");
  }

  else if (led == 'b') {
    blueBlinkEnabled = !blueBlinkEnabled;

    if (!blueBlinkEnabled) {
      bluePhase = false;
      writeLed(LED_BLUE, false);
    }

    Serial.print("Blue blinking = ");
    Serial.println(blueBlinkEnabled ? "ON" : "OFF");
  }
}

// ================== HANDLE BLINK ==================
void handleBlink() {
  unsigned long now = millis();

  if (now - lastBlinkToggle >= speedTable[speedIndex]) {
    lastBlinkToggle = now;

    if (redBlinkEnabled) {
      redPhase = !redPhase;
      writeLed(LED_RED, redPhase);
    }

    if (yellowBlinkEnabled) {
      yellowPhase = !yellowPhase;
      writeLed(LED_YELLOW, yellowPhase);
    }

    if (greenBlinkEnabled) {
      greenPhase = !greenPhase;
      writeLed(LED_GREEN, greenPhase);
    }

    if (blueBlinkEnabled) {
      bluePhase = !bluePhase;
      writeLed(LED_BLUE, bluePhase);
    }
  }
}

// ================== LED HELPERS ==================
void allLedsOff() {
  writeLed(LED_RED, false);
  writeLed(LED_YELLOW, false);
  writeLed(LED_GREEN, false);
  writeLed(LED_BLUE, false);
}

void writeLed(int pin, bool on) {
  digitalWrite(pin, on ? LED_ON : LED_OFF);
}

// ================== READ + PUBLISH HUMIDITY ==================
void publishHumidity() {
  float h = dht.readHumidity();

  if (isnan(h)) {
    Serial.println("DHT read failed (nan)");
    return;
  }

  char msg[12];
  dtostrf(h, 1, 1, msg);

  Serial.print("Humidity = ");
  Serial.print(msg);
  Serial.println(" %");

  client.publish(TOPIC_HUMIDITY, msg);
}