/*
 * Prueba automatica QD003 por puerto UART del shield.
 *
 * Usar cuando la camara esta conectada al puerto fisico "UART" del carro.
 * Ese puerto parece compartir UART0: RX0 GPIO3 / TX0 GPIO1.
 *
 * Instrucciones:
 * 1. Sube este sketch con la camara DESCONECTADA.
 * 2. Desconecta USB o apaga el carro.
 * 3. Conecta la camara al puerto UART.
 * 4. Enciende/reinicia el carro.
 *
 * No usa Monitor Serial porque el UART queda ocupado por la camara.
 * No mueve motores.
 */

#define CAM_RX_PIN 3
#define CAM_TX_PIN 1
#define BUZZER_PIN 33
#define CAM_BAUD   115200

HardwareSerial CameraSerial(1);

const uint8_t MODE_MENU = 0;
const uint8_t MODE_COLOR = 1;
const uint8_t MODE_QR = 2;
const uint8_t MODE_FACE = 4;
const uint8_t MODE_VISUAL_PATROL = 8;

uint8_t modeIndex = 0;
unsigned long lastModeMs = 0;
unsigned long lastReadMs = 0;

void beep(int freq, int ms) {
  tone(BUZZER_PIN, freq, ms);
  delay(ms + 20);
}

void sendPacket(const uint8_t *data, size_t len) {
  CameraSerial.write(data, len);
  CameraSerial.flush();
}

void setSimpleMode(uint8_t mode) {
  uint8_t packet[] = {mode, 13, 10};
  sendPacket(packet, sizeof(packet));
}

void setColorMode() {
  uint8_t packet[] = {
    MODE_COLOR,
    0,
    0x02, 0x58,  // area threshold 600
    0x01, 0x90,  // pixels threshold 400
    13,
    10
  };

  sendPacket(packet, sizeof(packet));
}

void sendNextMode() {
  switch (modeIndex) {
    case 0:
      setSimpleMode(MODE_MENU);
      beep(800, 80);
      break;
    case 1:
      setColorMode();
      beep(1000, 80);
      break;
    case 2:
      setSimpleMode(MODE_QR);
      beep(1200, 80);
      break;
    case 3:
      setSimpleMode(MODE_FACE);
      beep(1400, 80);
      break;
    case 4:
      setSimpleMode(MODE_VISUAL_PATROL);
      beep(1600, 80);
      break;
  }

  modeIndex = (modeIndex + 1) % 5;
}

void readCameraFrames() {
  if (!CameraSerial.available()) {
    return;
  }

  int len = CameraSerial.read();

  if (len <= 0 || len > 120) {
    return;
  }

  unsigned long startMs = millis();
  while (CameraSerial.available() < len) {
    if (millis() - startMs > 150) {
      return;
    }
  }

  while (len-- > 0) {
    CameraSerial.read();
  }

  // Si llega cualquier frame valido, la comunicacion esta viva.
  beep(2200, 35);
  lastReadMs = millis();
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);

  CameraSerial.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  delay(1200);

  beep(900, 80);
  beep(1300, 80);

  sendNextMode();
  lastModeMs = millis();
}

void loop() {
  readCameraFrames();

  if (millis() - lastModeMs > 5000) {
    sendNextMode();
    lastModeMs = millis();
  }
}
