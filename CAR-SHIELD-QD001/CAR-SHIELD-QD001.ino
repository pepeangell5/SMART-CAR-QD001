#include <ACB_SmartCar_V2.h>
#include <ESP32Servo.h>

ACB_SmartCar_V2 ACB_SmartCar;
Servo servoScan;
bool avanzando = false;

#define SERVO_PIN 25
#define TRIG_PIN  13
#define ECHO_PIN  14
#define BUZZER    33
#define LED_LEFT  12
#define LED_RIGHT 2

// Tu calibración real
#define SERVO_LEFT    0
#define SERVO_CENTER  85
#define SERVO_RIGHT   180

// Velocidades
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

// Distancias
#define DIST_SAFE     32   // si hay más de esto, avanza
#define DIST_DANGER   22   // si hay menos de esto, evita
#define DIST_CRITICAL 14   // demasiado cerca

// Maniobras
#define BACK_TIME_SHORT  450
#define BACK_TIME_LONG   750
#define TURN_TIME        950
#define SIDE_TIME        700

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

  for (int i = 0; i < 5; i++) {
    float d = medirDistanciaCM();

    if (d > 2 && d < 400) {
      suma += d;
      validas++;
    }

    delay(45);
  }

  if (validas == 0) return 999;

  return suma / validas;
}

float mirar(int angulo, const char* nombre) {
  Serial.print("Mirando ");
  Serial.println(nombre);

  servoScan.write(angulo);
  delay(750);

  float d = medirPromedio();

  Serial.print(nombre);
  Serial.print(": ");
  Serial.print(d);
  Serial.println(" cm");

  return d;
}

void ledsOff() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
}

void ledsFrente() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, HIGH);
}

void ledIzquierda() {
  digitalWrite(LED_LEFT, HIGH);
  digitalWrite(LED_RIGHT, LOW);
}

void ledDerecha() {
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, HIGH);
}

void beep(int frecuencia, int duracion) {
  int ciclos = frecuencia > 1200 ? 3 : 2;

  for (int i = 0; i < ciclos; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(duracion / ciclos / 2);
    digitalWrite(BUZZER, LOW);
    delay(duracion / ciclos / 2);
  }

  delay(40);
}

void detener(int tiempo = 250) {
  ACB_SmartCar.Move(Stop, 0);
  avanzando = false;
  delay(tiempo);
}

void retroceder(int tiempo) {
  Serial.println("Retrocediendo...");
  ledsFrente();
  MOVE_WITH_KICK(Backward, SPEED_BACK);
  delay(tiempo);
  detener();
}

void girarIzquierdaFuerte() {
  Serial.println("Maniobra: giro fuerte izquierda");
  ledIzquierda();
  MOVE_WITH_KICK(Contrarotate, SPEED_TURN);
  delay(TURN_TIME);
  detener();
}

void girarDerechaFuerte() {
  Serial.println("Maniobra: giro fuerte derecha");
  ledDerecha();
  MOVE_WITH_KICK(Clockwise, SPEED_TURN);
  delay(TURN_TIME);
  detener();
}

void moverLateralIzquierda() {
  Serial.println("Maniobra: movimiento lateral izquierda");
  ledIzquierda();
  MOVE_WITH_KICK(Move_Left, SPEED_SIDE);
  delay(SIDE_TIME);
  detener();
}

void moverLateralDerecha() {
  Serial.println("Maniobra: movimiento lateral derecha");
  ledDerecha();
  MOVE_WITH_KICK(Move_Right, SPEED_SIDE);
  delay(SIDE_TIME);
  detener();
}

bool frenteLibre() {
  servoScan.write(SERVO_CENTER);
  delay(250);

  float frente = medirPromedio();

  Serial.print("Verificacion frente: ");
  Serial.print(frente);
  Serial.println(" cm");

  return frente > DIST_SAFE;
}

void evitarObstaculo() {
  detener(300);
  ledsFrente();
  beep(1200, 120);

  Serial.println("Obstaculo detectado. Escaneando antes de retroceder...");

  float frenteInicial = mirar(SERVO_CENTER, "FRENTE");
  float izquierda = mirar(SERVO_LEFT, "IZQUIERDA");
  float derecha   = mirar(SERVO_RIGHT, "DERECHA");

  servoScan.write(SERVO_CENTER);
  delay(350);

  Serial.print("Frente inicial: ");
  Serial.print(frenteInicial);
  Serial.println(" cm");

  Serial.print("Comparacion -> Izq: ");
  Serial.print(izquierda);
  Serial.print(" cm | Der: ");
  Serial.print(derecha);
  Serial.println(" cm");

  if (frenteInicial < DIST_CRITICAL) {
    beep(700, 140);
    retroceder(BACK_TIME_LONG);
  } else {
    retroceder(BACK_TIME_SHORT);
  }

  if (izquierda > derecha) {
    Serial.println("Decision: escapar hacia IZQUIERDA");
    ledIzquierda();
    beep(950, 100);

    // Primero aprovecha Mecanum: se mueve lateralmente
    moverLateralIzquierda();

    // Luego gira un poco para cambiar orientación
    girarIzquierdaFuerte();
  } else {
    Serial.println("Decision: escapar hacia DERECHA");
    ledDerecha();
    beep(1450, 100);

    moverLateralDerecha();
    girarDerechaFuerte();
  }

  // Verificación obligatoria antes de avanzar
  int intentos = 0;

  while (!frenteLibre() && intentos < 3) {
    Serial.println("Aun hay obstaculo al frente. Reintentando escape...");
    ledsFrente();
    beep(700, 100);

    retroceder(BACK_TIME_SHORT);

    if (izquierda > derecha) {
      girarIzquierdaFuerte();
    } else {
      girarDerechaFuerte();
    }

    intentos++;
  }

  if (intentos >= 3) {
    Serial.println("Zona complicada. Retroceso largo y giro extra.");
    retroceder(BACK_TIME_LONG);

    if (izquierda > derecha) {
      girarIzquierdaFuerte();
      girarIzquierdaFuerte();
    } else {
      girarDerechaFuerte();
      girarDerechaFuerte();
    }
  }

  servoScan.write(SERVO_CENTER);
  ledsOff();
  delay(300);
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
  delay(1000);

  Serial.println("QD001 - Evasion robusta");
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
    ledsFrente();

    if (!avanzando) {
      MOVE_WITH_KICK(Forward, SPEED_MOVE);
      avanzando = true;
    } else {
      ACB_SmartCar.Move(Forward, SPEED_MOVE);
    }
  } else {
    evitarObstaculo();
  }

  delay(80);
}
