/*
 * Control remoto ESP32 + joysticks + OLED para ACEBOTT QD001.
 * Basado en el control de drone de PepeAngell.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// MAC del ESP32 del carrito vista al subir por USB.
uint8_t carAddress[] = {0x30, 0x76, 0xF5, 0x05, 0xC2, 0x74};

typedef struct CarCommand {
  int16_t forward;
  int16_t lateral;
  int16_t turn;
  uint8_t speed;
  uint8_t horn;
  uint32_t counter;
} CarCommand;

CarCommand command;
esp_now_peer_info_t peerInfo;

const int PIN_FORWARD = 34;  // Antes throttle
const int PIN_LATERAL = 32;  // Antes pitch
const int PIN_TURN    = 33;  // Antes roll
const int PIN_HORN    = 13;  // SW del joystick izquierdo

const int ADC_MIN = 0;
const int ADC_MAX = 4095;
const int AXIS_DEADZONE = 420;

const int MAX_AXIS = 100;
const int SPEED_MIN = 150;
const int SPEED_MAX = 245;

// Cambia a -1 si algun eje queda invertido.
const int FORWARD_DIR = 1;
const int LATERAL_DIR = 1;
const int TURN_DIR = 1;

bool lastSendOk = false;
int centerForward = 1904;
int centerLateral = 1904;
int centerTurn = 1904;

int readAverageRaw(int pin) {
  long sum = 0;

  for (int i = 0; i < 80; i++) {
    sum += analogRead(pin);
    delay(4);
  }

  return sum / 80;
}

void calibrateSticks() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("QD001 ESP-NOW TX");
  display.drawLine(0, 10, 128, 10, WHITE);
  display.setCursor(0, 24);
  display.println("Calibrando...");
  display.setCursor(0, 42);
  display.println("Suelta joysticks");
  display.display();

  delay(700);

  centerForward = readAverageRaw(PIN_FORWARD);
  centerLateral = readAverageRaw(PIN_LATERAL);
  centerTurn = readAverageRaw(PIN_TURN);

  Serial.print("Centro FWD: ");
  Serial.println(centerForward);
  Serial.print("Centro LAT: ");
  Serial.println(centerLateral);
  Serial.print("Centro TURN: ");
  Serial.println(centerTurn);
}

int readAxisPercent(int pin, int center, int direction) {
  int raw = analogRead(pin);
  int centered = raw - center;

  if (abs(centered) <= AXIS_DEADZONE) {
    return 0;
  }

  int value;

  if (centered > 0) {
    value = map(raw, center + AXIS_DEADZONE, ADC_MAX, 0, MAX_AXIS);
  } else {
    value = map(raw, ADC_MIN, center - AXIS_DEADZONE, -MAX_AXIS, 0);
  }

  return constrain(value * direction, -MAX_AXIS, MAX_AXIS);
}

uint8_t commandToSpeed(int a, int b, int c) {
  int strongest = max(abs(a), max(abs(b), abs(c)));

  if (strongest == 0) {
    return 0;
  }

  return map(strongest, 1, MAX_AXIS, SPEED_MIN, SPEED_MAX);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  lastSendOk = status == ESP_NOW_SEND_SUCCESS;
}

void drawScreen(int rawForward, int rawLateral, int rawTurn) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("QD001 ESP-NOW");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 16);
  display.print("FWD: "); display.print(command.forward);
  display.print("  LAT: "); display.println(command.lateral);
  display.print("TURN: "); display.print(command.turn);
  display.print(" SPD: "); display.println(command.speed);
  display.print("HORN: "); display.println(command.horn ? "ON" : "OFF");

  display.setCursor(0, 48);
  display.print("RAW ");
  display.print(rawForward);
  display.print("/");
  display.print(rawLateral);
  display.print("/");
  display.println(rawTurn);

  display.setCursor(0, 58);
  display.print(lastSendOk ? "LINK OK " : "SIN ACK ");

  display.display();
}

void setup() {
  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);
  pinMode(PIN_HORN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Error");
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("QD001 ESP-NOW TX");
  display.display();

  calibrateSticks();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init error");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  memcpy(peerInfo.peer_addr, carAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("No se pudo agregar peer");
    return;
  }

  Serial.println("Control QD001 listo");
}

void loop() {
  int rawForward = analogRead(PIN_FORWARD);
  int rawLateral = analogRead(PIN_LATERAL);
  int rawTurn = analogRead(PIN_TURN);

  command.forward = readAxisPercent(PIN_FORWARD, centerForward, FORWARD_DIR);
  command.lateral = readAxisPercent(PIN_LATERAL, centerLateral, LATERAL_DIR);
  command.turn = readAxisPercent(PIN_TURN, centerTurn, TURN_DIR);
  command.speed = commandToSpeed(command.forward, command.lateral, command.turn);
  command.horn = digitalRead(PIN_HORN) == LOW ? 1 : 0;
  command.counter++;

  esp_now_send(carAddress, (uint8_t *)&command, sizeof(command));

  drawScreen(rawForward, rawLateral, rawTurn);
  delay(35);
}
