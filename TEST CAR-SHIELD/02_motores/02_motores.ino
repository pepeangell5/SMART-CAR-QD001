#include <ACB_SmartCar_V2.h>

ACB_SmartCar_V2 ACB_SmartCar;

#define BUZZER 33

void beep() {
  tone(BUZZER, 1200, 150);
  delay(250);
}

void detener() {
  ACB_SmartCar.Move(Stop, 0);
  delay(500);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER, OUTPUT);

  ACB_SmartCar.Init();

  Serial.println("ACEBOTT QD001 - prueba de motores");
  beep();

  int speed = 150;

  Serial.println("Adelante");
  ACB_SmartCar.Move(Forward, speed);
  delay(800);
  detener();

  Serial.println("Atras");
  ACB_SmartCar.Move(Backward, speed);
  delay(800);
  detener();

  Serial.println("Mover izquierda");
  ACB_SmartCar.Move(Move_Left, speed);
  delay(800);
  detener();

  Serial.println("Mover derecha");
  ACB_SmartCar.Move(Move_Right, speed);
  delay(800);
  detener();

  Serial.println("Giro horario");
  ACB_SmartCar.Move(Clockwise, speed);
  delay(800);
  detener();

  Serial.println("Giro antihorario");
  ACB_SmartCar.Move(Contrarotate, speed);
  delay(800);
  detener();

  Serial.println("Prueba terminada");
  beep();
}

void loop() {
  ACB_SmartCar.Move(Stop, 0);
}
