#include <ACB_SmartCar_V2.h>
#include <IRremote.h>

ACB_SmartCar_V2 ACB_SmartCar;

#define IR_PIN    4
#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2

#define KICK_SPEED 255
#define KICK_MS    160
#define SPEED_IR   220

#define IR_UP          0xB946FF00
#define IR_DOWN        0xEA15FF00
#define IR_LEFT_TURN   0xBB44FF00
#define IR_RIGHT_TURN  0xBC43FF00
#define IR_LEFT_SIDE   0xE916FF00
#define IR_RIGHT_SIDE  0xF20DFF00
#define IR_OK_STOP     0xBF40FF00

IRrecv myIRrecv(IR_PIN);

uint32_t lastDecode = 0;
byte lastAction = Stop;
bool moving = false;
unsigned long lastCommandMs = 0;
const unsigned long commandTimeoutMs = 180;

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

void moveCar(byte action) {
  if (action == Stop) {
    stopCar();
    return;
  }

  if (!moving || lastAction != action) {
    ACB_SmartCar.Move(action, KICK_SPEED);
    delay(KICK_MS);
  }

  ACB_SmartCar.Move(action, SPEED_IR);
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
  }
}

void handleIrCode(uint32_t code) {
  Serial.print("IR: 0x");
  Serial.println(code, HEX);

  switch (code) {
    case IR_UP:
      moveCar(Forward);
      break;
    case IR_DOWN:
      moveCar(Backward);
      break;
    case IR_LEFT_TURN:
      moveCar(Contrarotate);
      break;
    case IR_RIGHT_TURN:
      moveCar(Clockwise);
      break;
    case IR_LEFT_SIDE:
      moveCar(Move_Left);
      break;
    case IR_RIGHT_SIDE:
      moveCar(Move_Right);
      break;
    case IR_OK_STOP:
      stopCar();
      beep(50);
      break;
    default:
      break;
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

  myIRrecv.enableIRIn();

  Serial.println("QD001 - Control infrarrojo");
  Serial.println("Flechas: avanzar, retroceder y girar");
  Serial.println("Boton 1/3: movimiento lateral");

  beep(80);
  delay(80);
  beep(80);
}

void loop() {
  if (myIRrecv.decode()) {
    uint32_t code = myIRrecv.decodedIRData.decodedRawData;

    if (myIRrecv.decodedIRData.flags) {
      code = lastDecode;
    } else {
      lastDecode = code;
    }

    handleIrCode(code);
    myIRrecv.resume();
  }

  if (moving && millis() - lastCommandMs > commandTimeoutMs) {
    stopCar();
  }
}
