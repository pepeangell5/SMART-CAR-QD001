/*
 * QD003 APP CONTROL
 *
 * Controla el carro desde la app ACEBOTT y manda comandos a la camara QD003.
 *
 * Importante:
 * - Si la QD003 esta conectada al puerto fisico UART del shield, sube este
 *   sketch con la camara desconectada. Luego conecta la camara y reinicia.
 * - QD003 no transmite video MJPEG como ESP32-CAM/QD002; entrega resultados
 *   de vision por UART y muestra la imagen en su propia pantalla.
 */

#include <WiFi.h>
#include <ACB_SmartCar_V2.h>

#define CMD_RUN 1
#define CMD_STANDBY 3

#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2
#define CAM_SERVO_PIN 25
#define CAM_SERVO_MIN 20
#define CAM_SERVO_HOME 35
#define CAM_SERVO_MAX 105
#define CAM_SERVO_HOLD_MS 700

#define TRIG_PIN 13
#define ECHO_PIN 14
#define LINE_LEFT_PIN 35
#define LINE_MID_PIN 36
#define LINE_RIGHT_PIN 39
#define LINE_BLACK LOW

// Puerto UART fisico del shield. Para subir codigo, desconecta la camara.
#define CAM_RX_PIN 3
#define CAM_TX_PIN 1
#define CAM_BAUD   115200

#define KICK_SPEED 255
#define KICK_MS    160
#define SPEED_AUTO 205
#define SPEED_TURN 220
#define SPEED_LINE 170
#define SPEED_FOLLOW 185
#define SPEED_CAMERA_REACTION 190

#define DIST_FOLLOW_NEAR 16
#define DIST_FOLLOW_FAR 34
#define DIST_AVOID_DANGER 23
#define DIST_AVOID_CRITICAL 13
#define DIST_PATROL_CLEAR 30

#define MODE_MANUAL 0
#define MODE_AVOID 1
#define MODE_LINE 2
#define MODE_FOLLOW 3
#define MODE_PATROL 4
#define MODE_TRAFFIC_AI 5

#define DEV_DIRECT_DIGITAL 0x01
#define DEV_SERVO          0x02
#define DEV_BUZZER         0x03
#define DEV_LED_LEFT       0x04
#define DEV_LED_RIGHT      0x05
#define DEV_LED_BOTH       0x06
#define DEV_MODE           0x0E
#define DEV_AUTO_MODE      0x0F

#define CMD_QR_CODE                30
#define CMD_BARCODE                31
#define CMD_DIGITAL_RECOGNITION    32
#define CMD_COLOR_RECOGNITION      33
#define CMD_IMAGE_RECOGNITION      34
#define CMD_COLOR_TRACKING         35
#define CMD_VISUAL_INSPECTION      36
#define CMD_TRAFFIC_IDENTIFICATION 37
#define CMD_MACHINE_LEARNING       38
#define CMD_FACE_RECOGNITION       39
#define CMD_RGB_RED                41
#define CMD_RGB_GREEN              42
#define CMD_RGB_BLUE               43
#define CMD_TAKE_STOP              50

const char *ssid = "ESP32_QD003";
const char *password = "12345678";

WiFiServer server(100);
WiFiClient client;
ACB_SmartCar_V2 ACB_SmartCar;
HardwareSerial CameraSerial(1);

int speedApp = 220;
int cameraAngle = 90;
bool cameraServoAttached = false;
unsigned long lastServoMoveMs = 0;
byte dataLen = 0;
byte index_a = 0;
byte buffer[52];
byte prevc = 0;
bool isStart = false;
bool moving = false;
bool clientWasConnected = false;
byte lastAction = Stop;
uint8_t activeMode = MODE_MANUAL;
uint8_t camMode = 0;
uint8_t rgbRed = 0;
uint8_t rgbGreen = 0;
uint8_t rgbBlue = 0;
unsigned long lastCameraReadMs = 0;
unsigned long lastAutoStepMs = 0;
unsigned long lastBlinkMs = 0;
uint8_t lastTrafficCode = 0;
bool ledBlinkState = false;
uint8_t camFrameLen = 0;
uint8_t camFramePos = 0;
uint8_t camPayload[128];
unsigned long camFrameStartMs = 0;
unsigned long lastCameraReactionMs = 0;
unsigned long cameraReactionEndMs = 0;
byte cameraReactionAction = Stop;

void setupCameraServo() {
  if (!cameraServoAttached) {
    ledcAttach(CAM_SERVO_PIN, 50, 16);
    cameraServoAttached = true;
  }
}

void releaseCameraServo() {
  if (cameraServoAttached) {
    ledcDetach(CAM_SERVO_PIN);
    pinMode(CAM_SERVO_PIN, INPUT);
    cameraServoAttached = false;
  }
}

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

void beep(int freq, int duration) {
  tone(BUZZER, freq, duration);
  delay(duration + 20);
}

void beepOk() {
  tone(BUZZER, 1500, 70);
}

void beepMode(uint8_t mode) {
  for (uint8_t i = 0; i <= mode; i++) {
    tone(BUZZER, 900 + (mode * 80), 45);
    delay(70);
  }
}

void stopCar() {
  ACB_SmartCar.Move(Stop, 0);
  moving = false;
  lastAction = Stop;
  if (activeMode == MODE_MANUAL) {
    ledsOff();
  }
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

  switch (action) {
    case Forward:
    case Backward:
      ledsBoth();
      break;
    case Move_Left:
    case Contrarotate:
    case Top_Left:
    case Bottom_Left:
      ledLeft();
      break;
    case Move_Right:
    case Clockwise:
    case Top_Right:
    case Bottom_Right:
      ledRight();
      break;
    default:
      ledsOff();
      break;
  }
}

float measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 26000);
  if (duration == 0) return 999;

  float distance = duration * 0.0343 / 2.0;
  if (distance < 2 || distance > 400) return 999;
  return distance;
}

float measureDistanceAverage(uint8_t samples = 3) {
  float sum = 0;
  uint8_t valid = 0;

  for (uint8_t i = 0; i < samples; i++) {
    float d = measureDistanceCm();
    if (d > 2 && d < 400) {
      sum += d;
      valid++;
    }
    delay(12);
  }

  if (valid == 0) return 999;
  return sum / valid;
}

float lookAt(int angle) {
  moveCameraServo(angle);
  delay(260);
  return measureDistanceAverage(3);
}

bool lineLeftActive() {
  return digitalRead(LINE_LEFT_PIN) == LINE_BLACK;
}

bool lineMidActive() {
  return digitalRead(LINE_MID_PIN) == LINE_BLACK;
}

bool lineRightActive() {
  return digitalRead(LINE_RIGHT_PIN) == LINE_BLACK;
}

void setMode(uint8_t mode) {
  activeMode = mode;
  stopCar();
  ledsOff();
  lastAutoStepMs = 0;

  beepMode(mode);
}

void handleDirectDigital() {
  byte pin = readBuffer(11);
  byte val = readBuffer(12);

  if (pin == LED_LEFT || pin == LED_RIGHT) {
    digitalWrite(LED_LEFT, val ? HIGH : LOW);
    digitalWrite(LED_RIGHT, val ? HIGH : LOW);
  } else if (pin == BUZZER) {
    digitalWrite(pin, val ? HIGH : LOW);
  }
}

void cameraWrite(const uint8_t *data, size_t len) {
  CameraSerial.write(data, len);
  CameraSerial.flush();
}

void cameraDefaultMenu() {
  camMode = 0;
  const uint8_t packet[] = {0, 13, 10};
  cameraWrite(packet, sizeof(packet));
}

void cameraSimpleMode(uint8_t mode) {
  camMode = mode;
  const uint8_t packet[] = {mode, 13, 10};
  cameraWrite(packet, sizeof(packet));
}

void cameraColorMode(uint8_t colorIndex) {
  camMode = 1;

  const uint8_t packet[] = {
    1,
    colorIndex,
    0x02, 0x58,  // area threshold 600
    0x01, 0x90,  // pixels threshold 400
    13,
    10
  };

  cameraWrite(packet, sizeof(packet));
}

void cameraTrafficMode(uint8_t mode) {
  camMode = 7;
  const uint8_t packet[] = {7, mode, 13, 10};
  cameraWrite(packet, sizeof(packet));
}

void cameraRgbSet() {
  const uint8_t packet[] = {camMode, 255, rgbRed, rgbGreen, rgbBlue, 13, 10};
  cameraWrite(packet, sizeof(packet));
}

uint16_t readCameraU16(const uint8_t *payload, uint8_t index) {
  return ((uint16_t)payload[index] << 8) | payload[index + 1];
}

bool cameraModeCanReact() {
  return camMode == 2 || camMode == 3 || camMode == 5 || camMode == 7 || camMode == 9;
}

void startCameraReaction(const uint8_t *payload, uint8_t len) {
  if (!cameraModeCanReact()) return;
  if (activeMode != MODE_MANUAL) return;
  if (millis() - lastCameraReactionMs < 1400) return;

  lastCameraReactionMs = millis();
  tone(BUZZER, 1800, 120);

  byte action = Stop;

  if (len >= 9) {
    uint16_t centerX = readCameraU16(payload, 6);

    if (centerX < 130) {
      action = Contrarotate;
      ledLeft();
    } else if (centerX > 190) {
      action = Clockwise;
      ledRight();
    } else {
      ledsBoth();
    }
  } else {
    ledsBoth();
  }

  cameraReactionAction = action;

  if (action != Stop) {
    moveCar(action, SPEED_CAMERA_REACTION);
    cameraReactionEndMs = millis() + 360;
  } else {
    cameraReactionEndMs = millis() + 220;
  }
}

void serviceCameraReaction() {
  if (cameraReactionEndMs == 0) return;
  if (millis() < cameraReactionEndMs) return;

  if (cameraReactionAction != Stop) {
    ACB_SmartCar.Move(Stop, 0);
    moving = false;
    lastAction = Stop;
  }

  cameraReactionAction = Stop;
  cameraReactionEndMs = 0;
  ledsOff();
}

void moveCameraServo(int angle) {
  setupCameraServo();
  cameraAngle = constrain(angle, CAM_SERVO_MIN, CAM_SERVO_MAX);

  uint32_t pulseUs = map(cameraAngle, 0, 180, 500, 2500);
  uint32_t duty = (pulseUs * 65535UL) / 20000UL;
  ledcWrite(CAM_SERVO_PIN, duty);
  lastServoMoveMs = millis();
}

void handleCameraDevice(int device, byte val) {
  switch (device) {
    case DEV_DIRECT_DIGITAL:
      handleDirectDigital();
      break;

    case DEV_SERVO:
      moveCameraServo(val);
      break;

    case DEV_BUZZER:
      if (val == 0) {
        noTone(BUZZER);
        digitalWrite(BUZZER, LOW);
      } else {
        tone(BUZZER, map(val, 1, 255, 700, 2200), 120);
      }
      break;

    case DEV_LED_LEFT:
      digitalWrite(LED_LEFT, val ? HIGH : LOW);
      digitalWrite(LED_RIGHT, val ? HIGH : LOW);
      break;

    case DEV_LED_RIGHT:
      digitalWrite(LED_LEFT, val ? HIGH : LOW);
      digitalWrite(LED_RIGHT, val ? HIGH : LOW);
      break;

    case DEV_LED_BOTH:
      digitalWrite(LED_LEFT, val ? HIGH : LOW);
      digitalWrite(LED_RIGHT, val ? HIGH : LOW);
      break;

    case DEV_MODE:
    case DEV_AUTO_MODE:
      if (val <= MODE_TRAFFIC_AI) {
        setMode(val);
      } else {
        setMode(MODE_MANUAL);
      }
      break;

    case CMD_QR_CODE:
      setMode(MODE_MANUAL);
      cameraSimpleMode(2);
      break;
    case CMD_BARCODE:
      setMode(MODE_MANUAL);
      cameraSimpleMode(3);
      break;
    case CMD_DIGITAL_RECOGNITION:
      setMode(MODE_MANUAL);
      cameraSimpleMode(6);
      break;
    case CMD_COLOR_RECOGNITION:
    case CMD_COLOR_TRACKING:
      setMode(MODE_MANUAL);
      cameraColorMode(val);
      break;
    case CMD_IMAGE_RECOGNITION:
      setMode(MODE_MANUAL);
      cameraSimpleMode(5);
      break;
    case CMD_VISUAL_INSPECTION:
      setMode(MODE_MANUAL);
      cameraSimpleMode(8);
      break;
    case CMD_TRAFFIC_IDENTIFICATION:
      setMode(MODE_MANUAL);
      cameraTrafficMode(val);
      break;
    case CMD_MACHINE_LEARNING:
      setMode(MODE_MANUAL);
      cameraSimpleMode(9);
      break;
    case CMD_FACE_RECOGNITION:
      setMode(MODE_MANUAL);
      cameraSimpleMode(4);
      break;
    case CMD_RGB_RED:
      rgbRed = val;
      cameraRgbSet();
      break;
    case CMD_RGB_GREEN:
      rgbGreen = val;
      cameraRgbSet();
      break;
    case CMD_RGB_BLUE:
      rgbBlue = val;
      cameraRgbSet();
      break;
    case CMD_TAKE_STOP:
      setMode(MODE_MANUAL);
      cameraDefaultMenu();
      stopCar();
      break;
  }
}

void runModule(int device) {
  byte val = readBuffer(12);

  switch (device) {
    case 0x0C:
      setMode(MODE_MANUAL);
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
        case 0x05:
          moveCar(Top_Left, speedApp);
          break;
        case 0x06:
          moveCar(Bottom_Left, speedApp);
          break;
        case 0x07:
          moveCar(Top_Right, speedApp);
          break;
        case 0x08:
          moveCar(Bottom_Right, speedApp);
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
        case 0x11:
          setMode(MODE_AVOID);
          break;
        case 0x12:
          setMode(MODE_LINE);
          break;
        case 0x13:
          setMode(MODE_FOLLOW);
          break;
        case 0x14:
          setMode(MODE_PATROL);
          break;
        case 0x15:
          setMode(MODE_TRAFFIC_AI);
          break;
      }
      break;

    case 0x0D:
      speedApp = constrain(val, 120, 255);
      break;

    case 0x03:
      beep(1200, 120);
      break;

    default:
      handleCameraDevice(device, val);
      break;
  }
}

void runAvoidanceStep() {
  if (millis() - lastAutoStepMs < 120) return;
  lastAutoStepMs = millis();

  moveCameraServo(CAM_SERVO_HOME);
  float front = measureDistanceAverage(3);

  if (front > DIST_AVOID_DANGER) {
    moveCar(Forward, SPEED_AUTO);
    return;
  }

  stopCar();
  beepOk();

  float left = lookAt(CAM_SERVO_MIN);
  float right = lookAt(CAM_SERVO_MAX);
  moveCameraServo(CAM_SERVO_HOME);

  if (front < DIST_AVOID_CRITICAL) {
    moveCar(Backward, SPEED_AUTO);
    delay(420);
    stopCar();
  }

  if (left > right) {
    moveCar(Contrarotate, SPEED_TURN);
    delay(520);
  } else {
    moveCar(Clockwise, SPEED_TURN);
    delay(520);
  }

  stopCar();
}

void runLineStep() {
  if (millis() - lastAutoStepMs < 60) return;
  lastAutoStepMs = millis();

  bool left = lineLeftActive();
  bool mid = lineMidActive();
  bool right = lineRightActive();

  if (mid && !left && !right) {
    moveCar(Forward, SPEED_LINE);
  } else if (left && !right) {
    moveCar(Contrarotate, SPEED_LINE);
  } else if (right && !left) {
    moveCar(Clockwise, SPEED_LINE);
  } else if (left && mid && right) {
    stopCar();
  } else if (left && right) {
    moveCar(Forward, SPEED_LINE);
  } else {
    stopCar();
  }
}

void runFollowStep() {
  if (millis() - lastAutoStepMs < 120) return;
  lastAutoStepMs = millis();

  moveCameraServo(CAM_SERVO_HOME);
  float front = measureDistanceAverage(3);

  if (front < DIST_FOLLOW_NEAR) {
    moveCar(Backward, SPEED_FOLLOW);
  } else if (front > DIST_FOLLOW_FAR && front < 160) {
    moveCar(Forward, SPEED_FOLLOW);
  } else {
    stopCar();
  }
}

void runPatrolStep() {
  bool onLine = lineLeftActive() || lineMidActive() || lineRightActive();

  if (onLine) {
    runLineStep();
    return;
  }

  if (millis() - lastAutoStepMs < 120) return;
  lastAutoStepMs = millis();

  moveCameraServo(CAM_SERVO_HOME);
  float front = measureDistanceAverage(2);

  if (front > DIST_PATROL_CLEAR) {
    moveCar(Forward, SPEED_AUTO);
  } else {
    stopCar();
    moveCar(Clockwise, SPEED_TURN);
    delay(420);
    stopCar();
  }
}

void applyTrafficCode(uint8_t code) {
  switch (code) {
    case 1:
    case 'F':
    case 'f':
      moveCar(Forward, SPEED_AUTO);
      break;

    case 2:
    case 'L':
    case 'l':
      moveCar(Contrarotate, SPEED_TURN);
      delay(360);
      stopCar();
      break;

    case 3:
    case 'R':
    case 'r':
      moveCar(Clockwise, SPEED_TURN);
      delay(360);
      stopCar();
      break;

    case 5:
    case 'B':
    case 'b':
      moveCar(Backward, SPEED_AUTO);
      delay(300);
      stopCar();
      break;

    case 4:
    case 6:
    case 'S':
    case 's':
    case 'P':
    case 'p':
      stopCar();
      break;

    default:
      stopCar();
      break;
  }
}

void runTrafficAiStep() {
  stopCar();
}

void serviceAutonomousMode() {
  if (activeMode != MODE_MANUAL && millis() - lastBlinkMs > 260) {
    lastBlinkMs = millis();
    ledBlinkState = !ledBlinkState;
    digitalWrite(LED_LEFT, ledBlinkState ? HIGH : LOW);
    digitalWrite(LED_RIGHT, ledBlinkState ? LOW : HIGH);
  }

  switch (activeMode) {
    case MODE_AVOID:
      runAvoidanceStep();
      break;
    case MODE_LINE:
      runLineStep();
      break;
    case MODE_FOLLOW:
      runFollowStep();
      break;
    case MODE_PATROL:
      runPatrolStep();
      break;
    case MODE_TRAFFIC_AI:
      runTrafficAiStep();
      break;
  }
}

void parseData() {
  int action = readBuffer(9);
  int device = readBuffer(10);

  switch (action) {
    case CMD_RUN:
      runModule(device);
      break;

    case CMD_STANDBY:
      setMode(MODE_MANUAL);
      stopCar();
      moveCameraServo(CAM_SERVO_HOME);
      cameraDefaultMenu();
      break;

    default:
      handleCameraDevice(device, readBuffer(12));
      break;
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

void readCameraFrames() {
  while (CameraSerial.available()) {
    uint8_t b = CameraSerial.read();

    if (camFrameLen == 0) {
      if (b > 0 && b <= sizeof(camPayload)) {
        camFrameLen = b;
        camFramePos = 0;
        camFrameStartMs = millis();
      }
      continue;
    }

    camPayload[camFramePos++] = b;

    if (camFramePos >= camFrameLen) {
      lastCameraReadMs = millis();
      startCameraReaction(camPayload, camFrameLen);
      camFrameLen = 0;
      camFramePos = 0;
    }
  }

  if (camFrameLen != 0 && millis() - camFrameStartMs > 60) {
    camFrameLen = 0;
    camFramePos = 0;
  }
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LINE_LEFT_PIN, INPUT);
  pinMode(LINE_MID_PIN, INPUT);
  pinMode(LINE_RIGHT_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);
  ledsOff();

  ACB_SmartCar.Init();
  stopCar();

  moveCameraServo(CAM_SERVO_HOME);

  CameraSerial.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  delay(800);
  cameraDefaultMenu();

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 5);
  server.begin();

  beep(1000, 80);
  beep(1400, 80);
}

void loop() {
  if (!client || !client.connected()) {
    if (clientWasConnected) {
      stopCar();
      clientWasConnected = false;
    }

    WiFiClient newClient = server.available();

    if (newClient) {
      client = newClient;
      clientWasConnected = true;
      stopCar();
      beep(1500, 70);
    }
  }

  if (client && client.connected()) {
    while (client.available()) {
      byte c = client.read() & 0xFF;
      receiveByte(c);
    }
  }

  readCameraFrames();
  serviceCameraReaction();
  serviceAutonomousMode();

  if (cameraServoAttached && millis() - lastServoMoveMs > CAM_SERVO_HOLD_MS) {
    releaseCameraServo();
  }
}
