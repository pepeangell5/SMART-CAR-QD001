/*
 * Receptor ESP-NOW para ACEBOTT QD001.
 * Recibe comandos de CONTROL-QD001_ESPNOW.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <ACB_SmartCar_V2.h>

ACB_SmartCar_V2 ACB_SmartCar;

#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2

#define KICK_SPEED 255
#define KICK_MS    140

typedef struct CarCommand {
  int16_t forward;
  int16_t lateral;
  int16_t turn;
  uint8_t speed;
  uint8_t horn;
  uint32_t counter;
} CarCommand;

CarCommand incoming;

byte currentAction = Stop;
bool moving = false;
bool linkActive = false;
bool hornActive = false;
unsigned long lastPacketMs = 0;
uint32_t lastCounter = 0;

void ledsOff() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
}

void ledsBoth() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, HIGH);
}

void ledLeft() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, LOW);
}

void ledRight() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, HIGH);
}

void beep(int duracion) {
  tone(BUZZER, 1400, duracion);
  delay(duracion + 20);
}

void stopCar() {
  ACB_SmartCar.Move(Stop, 0);
  currentAction = Stop;
  moving = false;
  ledsOff();
}

void updateHorn() {
  if (incoming.horn && !hornActive) {
    tone(BUZZER, 1250);
    hornActive = true;
  } else if (!incoming.horn && hornActive) {
    noTone(BUZZER);
    hornActive = false;
  }
}

byte chooseAction(const CarCommand &cmd) {
  int absForward = abs(cmd.forward);
  int absLateral = abs(cmd.lateral);
  int absTurn = abs(cmd.turn);

  if (cmd.speed == 0 || (absForward == 0 && absLateral == 0 && absTurn == 0)) {
    return Stop;
  }

  if (absForward >= absLateral && absForward >= absTurn) {
    return cmd.forward > 0 ? Forward : Backward;
  }

  if (absLateral >= absForward && absLateral >= absTurn) {
    return cmd.lateral > 0 ? Move_Right : Move_Left;
  }

  return cmd.turn > 0 ? Clockwise : Contrarotate;
}

void applyAction(byte action, uint8_t speedValue) {
  if (action == Stop) {
    stopCar();
    return;
  }

  if (!moving || currentAction != action) {
    ACB_SmartCar.Move(action, KICK_SPEED);
    delay(KICK_MS);
  }

  ACB_SmartCar.Move(action, speedValue);
  currentAction = action;
  moving = true;

  switch (action) {
    case Forward:
    case Backward:
      ledsBoth();
      break;
    case Move_Left:
    case Contrarotate:
      ledLeft();
      break;
    case Move_Right:
    case Clockwise:
      ledRight();
      break;
  }
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(CarCommand)) {
    return;
  }

  memcpy(&incoming, data, sizeof(incoming));
  lastPacketMs = millis();
  lastCounter = incoming.counter;

  if (!linkActive) {
    linkActive = true;
    beep(60);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);
  ledsOff();

  ACB_SmartCar.Init();
  stopCar();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init error");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("QD001 ESP-NOW receptor listo");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  beep(80);
  delay(80);
  beep(80);
}

void loop() {
  if (millis() - lastPacketMs > 500) {
    if (linkActive) {
      Serial.println("Link perdido");
    }

    linkActive = false;
    stopCar();
    noTone(BUZZER);
    hornActive = false;
    delay(20);
    return;
  }

  byte action = chooseAction(incoming);
  applyAction(action, incoming.speed);
  updateHorn();

  static unsigned long lastPrintMs = 0;
  if (millis() - lastPrintMs > 300) {
    Serial.print("F:");
    Serial.print(incoming.forward);
    Serial.print(" L:");
    Serial.print(incoming.lateral);
    Serial.print(" T:");
    Serial.print(incoming.turn);
    Serial.print(" S:");
    Serial.print(incoming.speed);
    Serial.print(" H:");
    Serial.print(incoming.horn);
    Serial.print(" #");
    Serial.println(lastCounter);
    lastPrintMs = millis();
  }

  delay(20);
}
