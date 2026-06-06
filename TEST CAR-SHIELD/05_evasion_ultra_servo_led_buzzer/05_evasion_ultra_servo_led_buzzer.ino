#include <ACB_SmartCar_V2.h>
#include <ESP32Servo.h>

ACB_SmartCar_V2 ACB_SmartCar;
Servo servoScan;

#define SERVO_PIN 25
#define TRIG_PIN  13
#define ECHO_PIN  14
#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2

#define SERVO_LEFT    0
#define SERVO_CENTER  85
#define SERVO_RIGHT   180

// Si el motor no vence la friccion, un pulso corto ayuda a arrancar.
#define KICK_SPEED    255
#define KICK_MS       180

#define SPEED_MOVE    205
#define SPEED_BACK    205
#define SPEED_TURN    220
#define SPEED_SIDE    215

#define MOVE_WITH_KICK(direction, speed) \
  do { \
    ACB_SmartCar.Move(direction, KICK_SPEED); \
    delay(KICK_MS); \
    ACB_SmartCar.Move(direction, speed); \
  } while (0)

#define DIST_FREE     35
#define DIST_DANGER   24
#define DIST_CRITICAL 14

#define BACK_SHORT_MS 450
#define BACK_LONG_MS  750
#define SIDE_MS       500
#define TURN_MS       650
#define TURN_HARD_MS  1000

float medirDistanciaCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duracion == 0) return 999;

  float distancia = duracion * 0.0343 / 2.0;

  if (distancia < 2 || distancia > 400) return 999;

  return distancia;
}

float medirPromedio() {
  float suma = 0;
  int validas = 0;

  for (int i = 0; i < 4; i++) {
    float d = medirDistanciaCM();

    if (d > 2 && d < 400) {
      suma += d;
      validas++;
    }

    delay(35);
  }

  if (validas == 0) return 999;

  return suma / validas;
}

void ledsOff() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
}

void senalCentro() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, HIGH);
}

void senalIzquierda() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, LOW);
}

void senalDerecha() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, HIGH);
}

void beep(int frecuencia, int duracion) {
  tone(BUZZER, frecuencia, duracion);
  delay(duracion + 40);
}

void detener(int tiempo = 250) {
  ACB_SmartCar.Move(Stop, 0);
  delay(tiempo);
}

float mirar(int angulo, const char* nombre) {
  servoScan.write(angulo);
  delay(650);

  float d = medirPromedio();

  Serial.print(nombre);
  Serial.print(": ");
  Serial.print(d);
  Serial.println(" cm");

  return d;
}

void retroceder(int tiempo) {
  Serial.println("Retrocediendo");
  MOVE_WITH_KICK(Backward, SPEED_BACK);
  delay(tiempo);
  detener();
}

void escaparIzquierda(bool fuerte) {
  Serial.println("Escape hacia IZQUIERDA");
  senalIzquierda();

  MOVE_WITH_KICK(Move_Left, SPEED_SIDE);
  delay(SIDE_MS);
  detener(120);

  MOVE_WITH_KICK(Contrarotate, SPEED_TURN);
  delay(fuerte ? TURN_HARD_MS : TURN_MS);
  detener();
}

void escaparDerecha(bool fuerte) {
  Serial.println("Escape hacia DERECHA");
  senalDerecha();

  MOVE_WITH_KICK(Move_Right, SPEED_SIDE);
  delay(SIDE_MS);
  detener(120);

  MOVE_WITH_KICK(Clockwise, SPEED_TURN);
  delay(fuerte ? TURN_HARD_MS : TURN_MS);
  detener();
}

void evitarObstaculo() {
  detener(300);
  senalCentro();
  beep(1200, 120);

  Serial.println("Obstaculo detectado: ESCANEO ANTES DE RETROCEDER");

  float frente = mirar(SERVO_CENTER, "FRENTE");
  float izquierda = mirar(SERVO_LEFT, "IZQUIERDA");
  float derecha = mirar(SERVO_RIGHT, "DERECHA");

  servoScan.write(SERVO_CENTER);
  delay(250);

  bool irIzquierda = izquierda >= derecha;
  bool encerrado = izquierda < DIST_DANGER && derecha < DIST_DANGER;
  bool muyCerca = frente < DIST_CRITICAL;

  Serial.print("Decision base -> ");
  Serial.println(irIzquierda ? "IZQUIERDA" : "DERECHA");

  if (muyCerca || encerrado) {
    beep(700, 140);
    retroceder(BACK_LONG_MS);
  } else {
    retroceder(BACK_SHORT_MS);
  }

  if (irIzquierda) {
    escaparIzquierda(encerrado);
  } else {
    escaparDerecha(encerrado);
  }

  float verificacion = mirar(SERVO_CENTER, "VERIFICACION FRENTE");

  if (verificacion < DIST_DANGER) {
    Serial.println("Todavia bloqueado: segundo escape sin redecidir por retroceso");
    beep(700, 100);
    retroceder(BACK_SHORT_MS);

    if (irIzquierda) {
      escaparIzquierda(true);
    } else {
      escaparDerecha(true);
    }
  }

  servoScan.write(SERVO_CENTER);
  ledsOff();
  delay(200);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);

  ledsOff();

  ACB_SmartCar.Init();
  detener();

  servoScan.setPeriodHertz(50);
  servoScan.attach(SERVO_PIN, 500, 2400);
  servoScan.write(SERVO_CENTER);
  delay(800);

  Serial.println("ACEBOTT QD001 - prueba ultrasonico + servo + LEDs + buzzer");
  Serial.println("No usa sensores tracking");
  beep(1000, 120);
  beep(1400, 120);
}

void loop() {
  servoScan.write(SERVO_CENTER);
  delay(80);

  float frente = medirPromedio();

  Serial.print("Frente: ");
  Serial.print(frente);
  Serial.println(" cm");

  if (frente > DIST_DANGER) {
    ledsOff();
    MOVE_WITH_KICK(Forward, SPEED_MOVE);
  } else {
    evitarObstaculo();
  }

  delay(70);
}
