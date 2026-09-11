# CLAUDE.md — Camel Race Game

## Objetivo inmediato

Estamos desarrollando el firmware de un juego físico de carrera de camellos controlado por un ESP32.

La prueba actual es deliberadamente mínima: validar UN sensor de barrera infrarroja y UN motor 28BYJ-48 con ULN2003.

Comportamiento esperado:

1. El ESP32 arranca.
2. Por Serial se informa del estado de la barrera IR.
3. Mientras el haz está libre, el motor permanece parado.
4. Cuando se corta la barrera IR, se detecta el evento.
5. Como respuesta al corte, el motor realiza un movimiento corto y controlado.
6. Un mismo corte del haz debe provocar un único movimiento, no movimientos repetidos mientras el haz siga bloqueado.
7. Al liberar el haz, el sistema queda preparado para detectar el siguiente corte.

Esta prueba NO es todavía el firmware completo del juego.

---

## Entorno de desarrollo

- VS Code
- PlatformIO
- Framework: Arduino para ESP32
- Lenguaje: C++
- Placa: ESP32-WROOM-32 / NodeMCU con USB-C y CP2102

Crear/mantener un proyecto PlatformIO sencillo y fácil de depurar.

Estructura esperada:

```text
CamelRace/
├── platformio.ini
├── CLAUDE.md
└── src/
    └── main.cpp
```

No introducir librerías externas para esta primera prueba salvo que sean realmente necesarias.

---

## Hardware de la prueba

### ESP32

Placa basada en ESP32-WROOM-32.

Se alimenta/programa mediante USB-C.

### Motor

- 1 × 28BYJ-48 de 5 V
- 1 × driver ULN2003
- El motor está conectado al ULN2003 mediante su conector original de 5 hilos.

### Barrera infrarroja

La barrera está formada por:

- 1 LED emisor IR de 5 mm / 940 nm
- 1 fototransistor receptor IR de 5 mm

El emisor lleva resistencia limitadora, aproximadamente 220 Ω como valor inicial.

Conexión conceptual del emisor:

```text
3V3 -> resistencia ~220 Ω -> LED IR -> GND
```

El receptor se probará inicialmente usando el pull-up interno del ESP32:

```cpp
pinMode(IR_SENSOR, INPUT_PULLUP);
```

IMPORTANTE: la polaridad física exacta de las patas del fototransistor todavía debe considerarse parte de la validación experimental. No asumir que un problema de lectura es necesariamente de software.

---

## Cableado confirmado del prototipo actual

### Motor 1 / ULN2003

Mantener exactamente estos GPIO:

| ULN2003 | GPIO ESP32 | Color físico |
|---|---:|---|
| IN1 | GPIO 23 | naranja |
| IN2 | GPIO 22 | amarillo |
| IN3 | GPIO 21 | verde |
| IN4 | GPIO 19 | azul |

Definiciones:

```cpp
constexpr int MOTOR_PIN_1 = 23;
constexpr int MOTOR_PIN_2 = 22;
constexpr int MOTOR_PIN_3 = 21;
constexpr int MOTOR_PIN_4 = 19;
```

No cambiar estos pines sin indicarlo expresamente.

Si el motor vibra pero no gira, o gira de forma incorrecta, revisar primero que el orden físico IN1/IN2/IN3/IN4 del ULN2003 coincide realmente con naranja/amarillo/verde/azul antes de cambiar la secuencia de software.

### Sensor IR

Señal del receptor:

```cpp
constexpr int IR_SENSOR = 15;
```

GPIO físico confirmado:

- GPIO 15 / D15

Configuración inicial:

```cpp
pinMode(IR_SENSOR, INPUT_PULLUP);
```

### Masa

Todos los elementos deben compartir GND.

Existe una mini protoboard utilizada como distribuidor de masa:

```text
GND ESP32 -> cable negro -> grupo común de mini protoboard
                         -> GND sensor
                         -> GND resto del prototipo
```

Los agujeros solo están conectados entre sí cuando pertenecen al mismo grupo eléctrico de la protoboard.

---

### Alimentación del motor (confirmado en pruebas, 2026-09-10)

El ULN2003 y el motor NO deben alimentarse desde el pin 5V del ESP32.

Al hacerlo, el consumo del 28BYJ-48 (del orden de 250-400 mA, con picos
mayores) provoca caída de tensión, salta el detector de brownout y el ESP32
entra en bucle de reinicio. Síntoma observado: el monitor serie repite
indefinidamente la misma cabecera de arranque ilegible.

Alimentación correcta:

```text
Fuente 5V externa (>=1 A) -> borne de alimentación del ULN2003
GND de esa fuente         -> mini protoboard de masa (GND común con el ESP32)
5V del ESP32              -> SIN CONECTAR al ULN2003
```

El GND común es obligatorio.

---

## Fase actual de la prueba (2026-09-11)

La validación está partida en dos porque el motor ya arranca pero el receptor
IR no daba ninguna señal.

`src/main.cpp` está ahora en **modo barrera aislada**:

- `#define ENABLE_MOTOR 0` deja todo el código del motor fuera de compilación.
- Cada segundo se emite una línea `[diag]` con la lectura cruda del pin y un
  contador de transiciones. **Si ese contador no sube al tapar y destapar el
  haz, el problema es de hardware**, no de software.
- `#define IR_DIAG_MODE 2` cambia a lectura analógica cruda (0..4095) para ver
  si el fototransistor responde aunque no llegue a cruzar el umbral digital.
  Ese modo desactiva el pull-up interno, así que exige un pull-up EXTERNO de
  unos 10 kΩ entre GPIO15 y 3V3.

Nota sobre el pull-up: el interno del ESP32 es débil (~45 kΩ). Para un
fototransistor conviene un pull-up externo de 10 kΩ, que da flancos más
limpios y menos sensibilidad a la luz ambiente.

Cuando la barrera esté certificada, volver a poner `ENABLE_MOTOR` a 1.

---

## Primera implementación solicitada

Crear `src/main.cpp`.

Queremos código simple, explícito y fácil de comprobar por Serial.

### Fase A — diagnóstico IR

Al arrancar:

```text
CAMEL RACE - TEST IR + MOTOR
Sistema iniciado
```

Configurar Serial a 115200 baud.

Leer GPIO15 y mostrar cambios de estado, evitando inundar continuamente el monitor serie.

Por ejemplo:

```text
IR: LIBRE
IR: BLOQUEADO
IR: LIBRE
```

No asumir inicialmente que HIGH significa necesariamente LIBRE.

Crear una constante fácilmente modificable para definir cuál es el nivel activo:

```cpp
constexpr int IR_BLOCKED_LEVEL = LOW;
```

Si durante la prueba física se observa lógica invertida, deberá bastar con cambiar esa constante a `HIGH`.

### Fase B — motor

Cuando se produzca la transición de LIBRE a BLOQUEADO:

- ejecutar un único avance corto del motor;
- después dejar el motor desenergizado;
- no repetir el movimiento mientras el sensor permanezca BLOQUEADO.

El siguiente movimiento solo podrá producirse después de:

```text
BLOQUEADO -> LIBRE -> BLOQUEADO
```

Usar una secuencia half-step estándar del 28BYJ-48 de 8 estados.

No necesitamos todavía precisión mecánica real del camello. Para esta prueba basta un movimiento claramente visible.

Definir el tamaño de la prueba mediante una constante, por ejemplo:

```cpp
constexpr int TEST_STEPS = 256;
```

y una velocidad conservadora mediante un pequeño retardo entre half-steps.

Crear funciones separadas y legibles, por ejemplo:

```cpp
void motorStep(int stepIndex);
void moveMotor(int steps);
void releaseMotor();
bool isBeamBlocked();
```

No bloquear el proyecto con una arquitectura compleja.

Para este test se acepta que `moveMotor()` sea bloqueante.

---

## Seguridad y comportamiento al arrancar

El motor NO debe moverse automáticamente al encender el ESP32.

Si el ESP32 arranca cuando la barrera ya está bloqueada, NO interpretar ese estado inicial como un nuevo evento.

El sistema debe:

1. leer el estado inicial;
2. mostrarlo por Serial;
3. esperar a que exista una transición real posterior de LIBRE a BLOQUEADO.

Al terminar un movimiento, poner las cuatro entradas del ULN2003 a LOW para evitar mantener innecesariamente energizado el motor:

```cpp
releaseMotor();
```

---

## Antirrebote / filtrado

Aunque el sensor es óptico, no reaccionar a cambios extremadamente breves.

Para esta prueba implementar un filtro sencillo de aproximadamente 20–50 ms antes de aceptar un cambio de estado.

La prioridad es evitar falsos disparos sin complicar el código.

---

## platformio.ini

Preparar una configuración equivalente a:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
```

Si PlatformIO detecta que la placa concreta requiere otra definición compatible, explicar el motivo antes de cambiarla.

---

## Criterios de éxito

La prueba se considera correcta cuando:

- el proyecto compila;
- se puede cargar al ESP32;
- el monitor serie funciona a 115200;
- tapar/destapar la barrera cambia de forma estable entre LIBRE y BLOQUEADO;
- cada nuevo corte de barrera provoca exactamente un avance corto;
- mantener el dedo delante del sensor no genera movimientos sucesivos;
- retirar y volver a cortar el haz genera un nuevo avance;
- el motor queda parado y desenergizado después del movimiento.

---

## Si algo falla

Diagnosticar en este orden.

### El sensor nunca cambia

Revisar:

1. alineación emisor/receptor;
2. resistencia y alimentación del LED IR;
3. GND común;
4. GPIO15;
5. orientación/polaridad del fototransistor;
6. probar `IR_BLOCKED_LEVEL = HIGH` si la señal existe pero la interpretación está invertida.

### El motor no gira

Revisar:

1. alimentación del ULN2003/motor;
2. GND común;
3. conector de 5 hilos del 28BYJ-48;
4. GPIO 23/22/21/19;
5. correspondencia real de esos cables con IN1/IN2/IN3/IN4;
6. secuencia half-step.

### El motor vibra pero no avanza

No asumir inmediatamente que faltan pasos.

Normalmente comprobar primero el orden IN1-IN4 y la alimentación.

---

## Contexto del juego completo

El juego final actual es para 2 jugadores y tendrá 2 camellos, por tanto 2 motores 28BYJ-48 y 2 ULN2003.

La detección del proyecto evolucionó desde microswitches mecánicos a barreras IR porque las bolas de madera no accionaban los microswitches de forma suficientemente fiable.

La previsión completa es utilizar barreras IR para puntuación y HOME óptico.

Sin embargo, NO implementar todavía:

- segundo motor;
- resto de sensores;
- HOME;
- puntuaciones +1/+2/+3;
- START/RESET;
- lógica de carrera;
- detección de ganador;
- pantalla;
- sonido;
- 74HCT595.

Primero debemos certificar este test mínimo.

---

## Reglas para Claude Code

1. Antes de modificar cableado/pines en el código, consultar este archivo.
2. No inventar conexiones eléctricas no documentadas.
3. Mantener GPIO 23, 22, 21 y 19 para el motor de esta prueba.
4. Mantener GPIO15 para el sensor IR.
5. Priorizar código sencillo y observable por Serial.
6. Explicar brevemente cualquier cambio importante.
7. No sobrediseñar el firmware antes de validar el hardware.
8. Si una observación física contradice este documento, la prueba física manda: actualizar primero la documentación y después el código.
9. No añadir componentes ni librerías innecesarias.
10. Tras cada cambio, comprobar que el proyecto sigue compilando.

---

## Siguiente tarea para Claude Code

Crear el proyecto mínimo PlatformIO, incluyendo:

- `platformio.ini`
- `src/main.cpp`

Implementar la prueba:

```text
cortar barrera IR -> mover motor una vez
```

con monitor serie de diagnóstico y respetando exactamente los GPIO documentados en este archivo.
