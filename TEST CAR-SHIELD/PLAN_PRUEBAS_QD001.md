# Plan de pruebas QD001

Ruta principal:

`C:\PEPEANGELL\AA_VSCODE_PROYECTOS\ACEBOTT-ESP32-CAR-SHIELD-V2.0\CAR-SHIELD-QD001\CAR-SHIELD-QD001.ino`

## Estado actual

- El sketch principal compila con `ESP32 Dev Module` (`esp32:esp32:esp32`).
- Windows no detecta un puerto COM activo por ahora.
- Los tests `.txt` originales se dejaron intactos y tambien quedaron como sketches listos para compilar en `TEST CAR-SHIELD`.

## Orden recomendado para grabar

1. `01_leds`
   - Objetivo: confirmar LEDs izquierdo/derecho y buzzer.
   - Toma sugerida: plano cercano del frente, LEDs parpadeando cada medio segundo.
   - Resultado esperado: ambos LEDs alternan ON/OFF y suena un beep al iniciar.

2. `02_motores`
   - Objetivo: validar sentido de ruedas mecanum.
   - Toma sugerida: carrito sobre base elevada o con espacio libre.
   - Resultado esperado: adelante, atras, lateral izquierda, lateral derecha, giro horario y giro antihorario.

3. `03_servo_serial`
   - Objetivo: calibrar el centro real del servo.
   - Toma sugerida: mostrar monitor serial y movimiento del sensor.
   - Resultado esperado: aceptar valores 0 a 180; centro actual usado en el proyecto: `85`.

4. `04_evasion_basica`
   - Objetivo: demostrar deteccion frontal y decision izquierda/derecha.
   - Toma sugerida: obstaculo a 15-20 cm, luego liberar un lado.
   - Resultado esperado: se detiene, escanea, compara distancias y gira al lado libre.

5. `CAR-SHIELD-QD001`
   - Objetivo: prueba final con evasion robusta.
   - Toma sugerida: pasillo corto con obstaculos separados, dejando espacio a ambos lados.
   - Resultado esperado: avanza, retrocede si esta muy cerca, usa movimiento lateral y verifica el frente antes de seguir.

## Prueba nueva: evasion sin tracking

Sketch:

`TEST CAR-SHIELD\05_evasion_ultra_servo_led_buzzer\05_evasion_ultra_servo_led_buzzer.ino`

Objetivo:

- Usar motores, LEDs, buzzer, servo y sensor ultrasonico.
- No usar sensores tracking.
- Corregir el problema donde el carrito retrocede, escanea despues del retroceso y decide girar hacia donde estaba el obstaculo original.

Cambio clave:

- El carrito escanea izquierda/frente/derecha antes de retroceder.
- Despues del retroceso conserva esa decision y escapa hacia el lado que se vio mas libre.
- Si aun queda bloqueado al frente, hace un segundo escape hacia el mismo lado en vez de redescubrir desde una posicion ya cambiada.

Velocidades iniciales:

- Avance: `175`
- Retroceso: `165`
- Giro: `175`
- Lateral: `165`

Si patina o gira demasiado, bajar de 10 en 10. Si sigue lento, subir de 10 en 10 sin pasar de `220` para esta prueba.

## Comandos utiles

Compilar sketch principal:

```powershell
& 'C:\Users\PepeAngell\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 'C:\PEPEANGELL\AA_VSCODE_PROYECTOS\ACEBOTT-ESP32-CAR-SHIELD-V2.0\CAR-SHIELD-QD001'
```

Subir cuando aparezca el puerto COM:

```powershell
& 'C:\Users\PepeAngell\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' upload -p COMx --fqbn esp32:esp32:esp32 'C:\PEPEANGELL\AA_VSCODE_PROYECTOS\ACEBOTT-ESP32-CAR-SHIELD-V2.0\CAR-SHIELD-QD001'
```

Cambia `COMx` por el puerto real, por ejemplo `COM3`.

## Checklist antes de subir

- Usar cable USB de datos, no solo carga.
- Encender el switch de la placa si aplica.
- Instalar driver CH340 desde:
  `1.Tutoriales\Arduino (Alumno experimentado)\6.Configuración del entorno de programación\1.Instalar Arduino IDE y el controlador CH340`
- Revisar que aparezca un puerto COM en Administrador de dispositivos o con Arduino IDE.
- Para pruebas de motores, levantar el carrito o dejarlo en una zona despejada.
