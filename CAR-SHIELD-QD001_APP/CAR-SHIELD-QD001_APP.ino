#include <WiFi.h>
#include <ACB_SmartCar_V2.h>

#define CMD_RUN 1

#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2

#define KICK_SPEED 255
#define KICK_MS    160

const char *ssid = "ESP32-Car";
const char *password = "12345678";

WiFiServer server(100);
WiFiClient client;
ACB_SmartCar_V2 ACB_SmartCar;

int speedApp = 220;
byte dataLen = 0;
byte index_a = 0;
byte buffer[52];
byte prevc = 0;
bool isStart = false;
bool moving = false;
byte lastAction = Stop;
unsigned long lastCommandMs = 0;
bool clientWasConnected = false;

byte readBuffer(int index_r) {
  return buffer[index_r];
}

void writeBuffer(int index_w, byte c) {
  if (index_w < sizeof(buffer)) {
    buffer[index_w] = c;
  }
}

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
  digitalWrite(BUZZER, HIGH);
  delay(duracion);
  digitalWrite(BUZZER, LOW);
}

void stopCar() {
  ACB_SmartCar.Move(Stop, 0);
  moving = false;
  lastAction = Stop;
  ledsOff();
}

void moveCar(byte action, int speedValue) {
  if (action == Stop) {
    stopCar();
    return;
  }

  if (!moving || lastAction != action) {
    ACB_SmartCar.Move(action, KICK_SPEED);
    delay(KICK_MS);
  }

  ACB_SmartCar.Move(action, speedValue);
  moving = true;
  lastAction = action;
  lastCommandMs = millis();

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
    default:
      ledsOff();
      break;
  }
}

void runModule(int device) {
  byte val = readBuffer(12);

  switch (device) {
    case 0x0C:
      switch (val) {
        case 0x01:
          moveCar(Forward, speedApp);
          break;
        case 0x02:
          moveCar(Backward, speedApp);
          break;
        case 0x03:
          moveCar(Move_Left, speedApp);
          break;
        case 0x04:
          moveCar(Move_Right, speedApp);
          break;
        case 0x09:
          moveCar(Contrarotate, speedApp);
          break;
        case 0x0A:
          moveCar(Clockwise, speedApp);
          break;
        case 0x00:
          stopCar();
          break;
      }
      break;

    case 0x0D:
      speedApp = constrain(val, 120, 255);
      Serial.print("Velocidad app: ");
      Serial.println(speedApp);
      break;
  }
}

void parseData() {
  int action = readBuffer(9);
  int device = readBuffer(10);

  if (action == CMD_RUN) {
    runModule(device);
  }
}

void receiveByte(byte c) {
  if (c == 0x55 && !isStart) {
    if (prevc == 0xFF) {
      index_a = 1;
      isStart = true;
    }
  } else {
    prevc = c;

    if (isStart) {
      if (index_a == 2) {
        dataLen = c;
      } else if (index_a > 2) {
        dataLen--;
      }

      writeBuffer(index_a, c);
    }
  }

  index_a++;

  if (index_a >= sizeof(buffer)) {
    index_a = 0;
    isStart = false;
  }

  if (isStart && dataLen == 0 && index_a > 3) {
    isStart = false;
    parseData();
    index_a = 0;
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

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 5);
  server.begin();

  Serial.println("QD001 - Control por app ACEBOTT");
  Serial.print("WiFi: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  beep(80);
  delay(80);
  beep(80);
}

void loop() {
  if (!client || !client.connected()) {
    if (clientWasConnected) {
      stopCar();
      clientWasConnected = false;
      Serial.println("App desconectada");
    }

    WiFiClient newClient = server.available();

    if (newClient) {
      client = newClient;
      clientWasConnected = true;
      stopCar();
      Serial.println("App conectada");
      beep(100);
    }
  }

  if (client && client.connected()) {
    while (client.available()) {
      byte c = client.read() & 0xFF;
      receiveByte(c);
    }
  }
}
