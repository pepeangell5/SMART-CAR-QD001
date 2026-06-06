#include <ESP32Servo.h>

Servo servoScan;

#define SERVO_PIN 25

void moverServo(int angulo) {
  Serial.print("Moviendo servo a: ");
  Serial.println(angulo);

  servoScan.write(angulo);
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  servoScan.setPeriodHertz(50);
  servoScan.attach(SERVO_PIN, 500, 2400);

  Serial.println("ACEBOTT QD001 - calibracion de servo");
  Serial.println("Prueba automatica iniciando...");

  moverServo(40);
  moverServo(90);
  moverServo(140);
  moverServo(90);

  Serial.println("--------------------------");
  Serial.println("Escribe un numero entre 0 y 180 y presiona Enter");
  Serial.println("Ejemplo: 95");
}

void loop() {
  if (Serial.available() > 0) {
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();

    if (entrada.length() > 0) {
      int angulo = entrada.toInt();

      if (angulo >= 0 && angulo <= 180) {
        moverServo(angulo);
      } else {
        Serial.println("Angulo fuera de rango. Usa 0 a 180.");
      }
    }
  }
}
