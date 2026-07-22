/*********************************************************************
 * CLIENT ESP32
 * - Receives humidity from MQTT and shows it on LCD.
 * - SW1 sends red blink toggle.
 * - SW2 sends yellow blink toggle.
 * - SW3 sends green blink toggle.
 * - SW4 sends blue blink toggle.
 * - POT sends blink speed level 0..4.
 * - Buzzer turns on when humidity is high.
 *********************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

// ================== BUTTON PINS ==================
#define SW_RED      32   // SW1
#define SW_YELLOW   33   // SW2
#define SW_GREEN    25   // SW3
#define SW_BLUE     26   // SW4

// ================== POT + BUZZER ==================
#define POT_PIN     34
#define BUZZER_PIN  13

// ================== LCD ==================
// If screen shows strange symbols, first try 0x27.
// If still bad, change 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================== HUMIDITY THRESHOLD ==================
const float HUMIDITY_THRESHOLD = 60.0;

// ================== OBJECTS ==================
WiFiClient espClient;
PubSubClient client(espClient);

// ================== STATES ==================
float lastHumidity = 0.0;

bool redState    = false;
bool yellowState = false;
bool greenState  = false;
bool blueState   = false;

int lastSpeedIndex = -1;

// ================== DEBOUNCE STATES ==================
bool lastRedReading    = HIGH;
bool lastYellowReading = HIGH;
bool lastGreenReading  = HIGH;
bool lastBlueReading   = HIGH;

bool redHandled    = false;
bool yellowHandled = false;
bool greenHandled  = false;
bool blueHandled   = false;

unsigned long lastRedDebounce    = 0;
unsigned long lastYellowDebounce = 0;
unsigned long lastGreenDebounce  = 0;
unsigned long lastBlueDebounce   = 0;

const unsigned long debounceMs = 50;

// ================== POT TIMING ==================
unsigned long lastPotTime = 0;
const unsigned long potInterval = 300;

// ================== FUNCTION DECLARATIONS ==================
void startWifi();
void connectBroker();
void callback(char* topic, byte* payload, unsigned int length);

void handleButtons();
void handleOneButton(
  int pin,
  bool &lastReading,
  bool &handled,
  unsigned long &lastDebounce,
  const char* payload,
  const char* name,
  bool &state
);

void handlePot();
void updateLcd();

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  pinMode(SW_RED, INPUT_PULLUP);
  pinMode(SW_YELLOW, INPUT_PULLUP);
  pinMode(SW_GREEN, INPUT_PULLUP);
  pinMode(SW_BLUE, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // IMPORTANT LCD FIX:
  // SDA = D21, SCL = D22
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("LCD OK");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");
  delay(1500);

  startWifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  connectBroker();

  lcd.clear();
  updateLcd();
}

// ================== LOOP ==================
void loop() {
  if (!client.connected()) {
    connectBroker();
  }

  client.loop();

  handleButtons();
  handlePot();
}

// ================== WIFI ==================
void startWifi() {
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(ssid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi...");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nWiFi connected!");
  Serial.print("Client ESP32 IP: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(1500);
}

// ================== MQTT CONNECT ==================
void connectBroker() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker... ");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT...");
    lcd.setCursor(0, 1);
    lcd.print("Connecting");

    if (client.connect("esp32-client")) {
      Serial.println("connected!");

      client.subscribe(TOPIC_HUMIDITY);

      Serial.println("Subscribed to humidity topic.");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT connected");
      delay(1000);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retry in 3 seconds");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT failed");
      lcd.setCursor(0, 1);
      lcd.print("rc=");
      lcd.print(client.state());

      delay(3000);
    }
  }
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  char buf[16];
  unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;

  memcpy(buf, payload, n);
  buf[n] = '\0';

  String t = String(topic);

  if (t == TOPIC_HUMIDITY) {
    lastHumidity = atof(buf);

    Serial.print("Humidity received: ");
    Serial.println(lastHumidity);

    if (lastHumidity > HUMIDITY_THRESHOLD) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    updateLcd();
  }
}

// ================== BUTTON HANDLING ==================
void handleButtons() {
  handleOneButton(
    SW_RED,
    lastRedReading,
    redHandled,
    lastRedDebounce,
    "r",
    "Red",
    redState
  );

  handleOneButton(
    SW_YELLOW,
    lastYellowReading,
    yellowHandled,
    lastYellowDebounce,
    "y",
    "Yellow",
    yellowState
  );

  handleOneButton(
    SW_GREEN,
    lastGreenReading,
    greenHandled,
    lastGreenDebounce,
    "g",
    "Green",
    greenState
  );

  handleOneButton(
    SW_BLUE,
    lastBlueReading,
    blueHandled,
    lastBlueDebounce,
    "b",
    "Blue",
    blueState
  );
}

void handleOneButton(
  int pin,
  bool &lastReading,
  bool &handled,
  unsigned long &lastDebounce,
  const char* payload,
  const char* name,
  bool &state
) {
  bool reading = digitalRead(pin);

  if (reading != lastReading) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > debounceMs) {
    // INPUT_PULLUP means pressed = LOW
    if (reading == LOW && !handled) {
      handled = true;

      state = !state;

      client.publish(TOPIC_LED_CONTROL, payload);

      Serial.print("Sent blink toggle: ");
      Serial.print(name);
      Serial.print(" -> ");
      Serial.println(state ? "ON" : "OFF");

      updateLcd();
    }

    if (reading == HIGH) {
      handled = false;
    }
  }

  lastReading = reading;
}

// ================== POT HANDLING ==================
void handlePot() {
  if (millis() - lastPotTime < potInterval) return;
  lastPotTime = millis();

  int raw = analogRead(POT_PIN);          // 0..4095
  int level = map(raw, 0, 4095, 0, 4);    // 5 levels: 0,1,2,3,4

  if (level < 0) level = 0;
  if (level > 4) level = 4;

  if (level != lastSpeedIndex) {
    lastSpeedIndex = level;

    char msg[2];
    msg[0] = '0' + level;
    msg[1] = '\0';

    client.publish(TOPIC_LED_SPEED, msg);

    Serial.print("Sent speed level: ");
    Serial.println(level);

    updateLcd();
  }
}

// ================== LCD DISPLAY ==================
void updateLcd() {
  lcd.clear();

  // Row 1: Humidity and level
  lcd.setCursor(0, 0);
  lcd.print("H:");
  lcd.print(lastHumidity, 1);
  lcd.print("% ");

  if (lastHumidity < 40.0) {
    lcd.print("LOW");
  }
  else if (lastHumidity <= HUMIDITY_THRESHOLD) {
    lcd.print("OK");
  }
  else {
    lcd.print("HIGH");
  }

  // Row 2: Speed and active LEDs
  lcd.setCursor(0, 1);
  lcd.print("S:");

  if (lastSpeedIndex < 0) {
    lcd.print("-");
  } else {
    lcd.print(lastSpeedIndex);
  }

  lcd.print(" ");

  lcd.print("R");
  lcd.print(redState ? "1" : "0");

  lcd.print("Y");
  lcd.print(yellowState ? "1" : "0");

  lcd.print("G");
  lcd.print(greenState ? "1" : "0");

  lcd.print("B");
  lcd.print(blueState ? "1" : "0");
}