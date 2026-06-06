/*
 * Prueba minima QD003 / CanMV por UART.
 *
 * No mueve motores.
 * Sirve para validar cable, firmware y comunicacion con la camara.
 *
 * Cable UART:
 * QD003 TX -> ESP32 RX GPIO21
 * QD003 RX -> ESP32 TX GPIO22
 * QD003 VCC -> VCC
 * QD003 GND -> GND
 */

#define CAM_RX_PIN 21
#define CAM_TX_PIN 22
#define BUZZER_PIN 33
#define CAM_BAUD   115200

HardwareSerial CameraSerial(1);

enum CameraMode : uint8_t {
  MODE_MENU = 0,
  MODE_COLOR = 1,
  MODE_QR = 2,
  MODE_BARCODE = 3,
  MODE_FACE = 4,
  MODE_IMAGE = 5,
  MODE_NUMBER = 6,
  MODE_TRAFFIC = 7,
  MODE_VISUAL_PATROL = 8,
  MODE_MACHINE_LEARNING = 9
};

uint8_t currentMode = MODE_MENU;
unsigned long lastCommandMs = 0;
unsigned long lastHelpMs = 0;

void beepOk() {
  tone(BUZZER_PIN, 1400, 80);
}

void beepFrame() {
  tone(BUZZER_PIN, 900, 35);
}

void flushCamera() {
  while (CameraSerial.available()) {
    CameraSerial.read();
  }
}

void sendPacket(const uint8_t *data, size_t len) {
  CameraSerial.write(data, len);
  CameraSerial.flush();
}

void setMode(uint8_t mode) {
  currentMode = mode;

  uint8_t packet[] = {mode, 13, 10};
  sendPacket(packet, sizeof(packet));

  Serial.print("Modo enviado: ");
  Serial.println(mode);

  lastCommandMs = millis();
}

void setColorMode(uint8_t colorIndex = 0, uint16_t pixelsThreshold = 400, uint16_t areaThreshold = 600) {
  currentMode = MODE_COLOR;

  uint8_t packet[] = {
    MODE_COLOR,
    colorIndex,
    (uint8_t)((areaThreshold >> 8) & 0xFF),
    (uint8_t)(areaThreshold & 0xFF),
    (uint8_t)((pixelsThreshold >> 8) & 0xFF),
    (uint8_t)(pixelsThreshold & 0xFF),
    13,
    10
  };

  sendPacket(packet, sizeof(packet));
  Serial.println("Modo enviado: color recognition");
  lastCommandMs = millis();
}

bool readFrame(uint8_t *payload, uint8_t *payloadLen, unsigned long timeoutMs = 220) {
  unsigned long startMs = millis();

  while (!CameraSerial.available()) {
    if (millis() - startMs > timeoutMs) {
      return false;
    }
  }

  int len = CameraSerial.read();

  if (len <= 0 || len > 120) {
    return false;
  }

  startMs = millis();
  while (CameraSerial.available() < len) {
    if (millis() - startMs > timeoutMs) {
      return false;
    }
  }

  for (int i = 0; i < len; i++) {
    payload[i] = CameraSerial.read();
  }

  *payloadLen = len;
  return true;
}

uint16_t readU16(const uint8_t *payload, int index) {
  return ((uint16_t)payload[index] << 8) | payload[index + 1];
}

void printTextTail(const uint8_t *payload, uint8_t len, int startIndex) {
  if (len <= startIndex) {
    Serial.println("(sin etiqueta)");
    return;
  }

  for (int i = startIndex; i < len; i++) {
    char c = (char)payload[i];
    if (c >= 32 && c <= 126) {
      Serial.print(c);
    }
  }
  Serial.println();
}

void printFrame(const uint8_t *payload, uint8_t len) {
  Serial.print("Frame len=");
  Serial.print(len);
  Serial.print(" modo=");
  Serial.println(currentMode);

  switch (currentMode) {
    case MODE_QR:
    case MODE_BARCODE:
      if (len >= 6) {
        Serial.print("X:");
        Serial.print(readU16(payload, 0));
        Serial.print(" Y:");
        Serial.print(payload[2]);
        Serial.print(" W:");
        Serial.print(readU16(payload, 3));
        Serial.print(" H:");
        Serial.print(payload[5]);
        Serial.print(" TAG:");
        printTextTail(payload, len, 6);
      }
      break;

    case MODE_COLOR:
    case MODE_IMAGE:
    case MODE_TRAFFIC:
      if (len >= 9) {
        Serial.print("X:");
        Serial.print(readU16(payload, 0));
        Serial.print(" Y:");
        Serial.print(payload[2]);
        Serial.print(" W:");
        Serial.print(readU16(payload, 3));
        Serial.print(" H:");
        Serial.print(payload[5]);
        Serial.print(" CX:");
        Serial.print(readU16(payload, 6));
        Serial.print(" CY:");
        Serial.print(payload[8]);
        Serial.print(" TAG:");
        printTextTail(payload, len, 9);
      }
      break;

    case MODE_FACE:
      if (len >= 10) {
        Serial.print("Face X:");
        Serial.print(readU16(payload, 0));
        Serial.print(" Y:");
        Serial.print(payload[2]);
        Serial.print(" W:");
        Serial.print(readU16(payload, 3));
        Serial.print(" H:");
        Serial.print(payload[5]);
        Serial.print(" ID:");
        Serial.println(payload[9]);
      }
      break;

    case MODE_NUMBER:
      if (len >= 1) {
        Serial.print("Numero: ");
        Serial.println(payload[0]);
      }
      break;

    case MODE_VISUAL_PATROL:
      if (len >= 1) {
        Serial.print("Visual data: ");
        Serial.println((int)payload[0] - 60);
      }
      break;

    case MODE_MACHINE_LEARNING:
      if (len >= 1) {
        Serial.print("Clase: ");
        Serial.println(payload[0]);
      }
      break;

    default:
      Serial.print("RAW:");
      for (int i = 0; i < len; i++) {
        Serial.print(" 0x");
        if (payload[i] < 16) Serial.print("0");
        Serial.print(payload[i], HEX);
      }
      Serial.println();
      break;
  }
}

void printHelp() {
  Serial.println();
  Serial.println("QD003 CAMERA TEST");
  Serial.println("Comandos por Monitor Serial:");
  Serial.println("m = menu/default");
  Serial.println("q = QR");
  Serial.println("b = barcode");
  Serial.println("c = color");
  Serial.println("f = face");
  Serial.println("n = number");
  Serial.println("v = visual patrol");
  Serial.println("t = traffic");
  Serial.println("i = image");
  Serial.println("l = machine learning");
  Serial.println();
}

void handleSerialCommand(char command) {
  switch (command) {
    case 'm':
      setMode(MODE_MENU);
      break;
    case 'q':
      setMode(MODE_QR);
      break;
    case 'b':
      setMode(MODE_BARCODE);
      break;
    case 'c':
      setColorMode();
      break;
    case 'f':
      setMode(MODE_FACE);
      break;
    case 'n':
      setMode(MODE_NUMBER);
      break;
    case 'v':
      setMode(MODE_VISUAL_PATROL);
      break;
    case 't': {
      uint8_t packet[] = {MODE_TRAFFIC, 0, 13, 10};
      currentMode = MODE_TRAFFIC;
      sendPacket(packet, sizeof(packet));
      Serial.println("Modo enviado: traffic");
      lastCommandMs = millis();
      break;
    }
    case 'i':
      setMode(MODE_IMAGE);
      break;
    case 'l':
      setMode(MODE_MACHINE_LEARNING);
      break;
    case '?':
      printHelp();
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);

  CameraSerial.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  delay(300);
  flushCamera();

  Serial.println("QD003 UART iniciado");
  Serial.print("RX GPIO");
  Serial.print(CAM_RX_PIN);
  Serial.print(" / TX GPIO");
  Serial.println(CAM_TX_PIN);

  printHelp();
  setMode(MODE_MENU);
  beepOk();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    handleSerialCommand(c);
  }

  uint8_t payload[128];
  uint8_t len = 0;

  if (readFrame(payload, &len, 20)) {
    beepFrame();
    printFrame(payload, len);
  }

  if (millis() - lastHelpMs > 7000) {
    Serial.println("Tip: escribe q, c, f, v, t, n, i, b, l o m en el Monitor Serial.");
    lastHelpMs = millis();
  }
}
