# ACEBOTT ESP32 Car Shield V2.0 - QD001

Proyecto de pruebas y modos de control para el kit ACEBOTT QD001 con ESP32 Car Shield V2.0.

## Sketches principales

### `CAR-SHIELD-QD001`

Modo autonomo con servo y sensor ultrasonico.

- Evita obstaculos.
- Escanea antes de retroceder.
- Usa pulso de arranque para mejorar fuerza en llantas.
- Usa LEDs y buzzer digital.

### `CAR-SHIELD-QD001_APP`

Modo manual con la app ACEBOTT.

- Crea WiFi `ESP32-Car`.
- Password: `12345678`.
- Usa el protocolo de la app por puerto `100`.
- Usa LEDs direccionales, buzzer digital y pulso de arranque.

### `CAR-SHIELD-QD001_IR`

Modo manual con control infrarrojo del kit.

- Receptor IR en pin `4`.
- Flechas: avanzar, retroceder y girar.
- Botones `1` y `3`: movimiento lateral.
- Boton `OK`: detener.
- Usa LEDs direccionales, buzzer digital y pulso de arranque.

### `CAR-SHIELD-QD001_ESPNOW`

Modo receptor para control remoto ESP32 por ESP-NOW.

- Se sube al ESP32 del carrito.
- Recibe adelante/atras, lateral y giro desde `CONTROL-QD001_ESPNOW`.
- Se detiene si pierde enlace por mas de 500 ms.
- Usa LEDs direccionales, buzzer digital y pulso de arranque.

### `CONTROL-QD001_ESPNOW`

Control remoto con ESP32, joysticks analogicos y pantalla OLED.

- Basado en el control anterior del drone.
- Pines usados:
  - `34`: adelante/atras.
  - `32`: lateral izquierda/derecha.
  - `33`: giro izquierda/derecha.
  - `13`: boton `SW` del joystick izquierdo para claxon.
- OLED SSD1306 I2C en direccion `0x3C`.
- Envia comandos al carrito por ESP-NOW.
- MAC actual del carrito configurada: `30:76:F5:05:C2:74`.

### `CAR-SHIELD-QD003_CAMERA_TEST`

Prueba minima de la extension QD003 por UART.

- No mueve motores.
- Usa UART con `RX=21` y `TX=22`.
- Permite probar modos desde el Monitor Serial:
  - `m`: menu/default.
  - `q`: QR.
  - `b`: barcode.
  - `c`: color.
  - `f`: face.
  - `n`: number.
  - `v`: visual patrol.
  - `t`: traffic.
  - `i`: image.
  - `l`: machine learning.
- Imprime los frames recibidos en Serial.

### `CAR-SHIELD-QD003_UART0_AUTO_TEST`

Prueba automatica para QD003 conectada al puerto fisico `UART` del shield.

- No mueve motores.
- Usa `RX=GPIO3` y `TX=GPIO1`.
- Se debe subir con la camara desconectada.
- Despues de subir, conectar la camara al puerto `UART` y reiniciar.
- Cambia de modo automaticamente cada 5 segundos.
- Beep doble al iniciar.
- Beep agudo corto si recibe frames validos de la camara.

## Tests

La carpeta `TEST CAR-SHIELD` contiene pruebas separadas para LEDs, motores, servo serial, evasion basica y pruebas de evasion con ultrasonico.

## Placa usada

Compilar con:

```powershell
esp32:esp32:esp32
```

En Arduino IDE aparece como:

```text
ESP32 Dev Module
```
