// CAMEL RACE - Test minimo: 1 barrera IR + 1 motor 28BYJ-48 (ULN2003)
//
// Comportamiento:
//   - Serial 115200, informa del estado de la barrera IR.
//   - Un corte del haz (LIBRE -> BLOQUEADO) provoca UN unico avance corto.
//   - Mientras el haz siga bloqueado no se repite el movimiento.
//   - Tras el movimiento el motor queda desenergizado.
//   - El estado inicial NO cuenta como evento (no se mueve al arrancar).

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Cableado (ver CLAUDE.md - no cambiar sin indicarlo expresamente)
// ---------------------------------------------------------------------------
constexpr int MOTOR_PIN_1 = 23;  // IN1 - naranja
constexpr int MOTOR_PIN_2 = 22;  // IN2 - amarillo
constexpr int MOTOR_PIN_3 = 21;  // IN3 - verde
constexpr int MOTOR_PIN_4 = 19;  // IN4 - azul

constexpr int IR_SENSOR = 15;    // Señal del fototransistor receptor

// Nivel logico que corresponde a "haz cortado".
// Si en la prueba fisica la logica sale invertida, cambiar a HIGH.
constexpr int IR_BLOCKED_LEVEL = LOW;

// ---------------------------------------------------------------------------
// Parametros de la prueba
// ---------------------------------------------------------------------------
constexpr int TEST_STEPS = 256;              // half-steps por evento
constexpr unsigned long STEP_DELAY_US = 1500; // velocidad conservadora
constexpr unsigned long DEBOUNCE_MS = 30;    // filtro antirrebote

// Secuencia half-step estandar de 8 estados del 28BYJ-48
constexpr uint8_t HALF_STEP_SEQUENCE[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
bool beamBlocked = false;         // estado estable actual (ya filtrado)
int lastRawLevel = HIGH;          // ultima lectura cruda del pin
unsigned long lastRawChangeMs = 0; // instante de ese ultimo cambio crudo

// ---------------------------------------------------------------------------
// Motor
// ---------------------------------------------------------------------------
void motorStep(int stepIndex) {
  const uint8_t *s = HALF_STEP_SEQUENCE[stepIndex & 0x07];
  digitalWrite(MOTOR_PIN_1, s[0] ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_2, s[1] ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_3, s[2] ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_4, s[3] ? HIGH : LOW);
}

void releaseMotor() {
  digitalWrite(MOTOR_PIN_1, LOW);
  digitalWrite(MOTOR_PIN_2, LOW);
  digitalWrite(MOTOR_PIN_3, LOW);
  digitalWrite(MOTOR_PIN_4, LOW);
}

// Movimiento bloqueante: aceptable para este test.
void moveMotor(int steps) {
  for (int i = 0; i < steps; i++) {
    motorStep(i);
    delayMicroseconds(STEP_DELAY_US);
  }
  releaseMotor();
}

// ---------------------------------------------------------------------------
// Sensor IR
// ---------------------------------------------------------------------------
bool isBeamBlocked() {
  return digitalRead(IR_SENSOR) == IR_BLOCKED_LEVEL;
}

// Devuelve true solo cuando se confirma un cambio de estado estable.
bool updateBeamState() {
  int raw = digitalRead(IR_SENSOR);

  if (raw != lastRawLevel) {
    lastRawLevel = raw;
    lastRawChangeMs = millis();
    return false;
  }

  if (millis() - lastRawChangeMs < DEBOUNCE_MS) {
    return false;
  }

  bool stable = (raw == IR_BLOCKED_LEVEL);
  if (stable != beamBlocked) {
    beamBlocked = stable;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);
  releaseMotor();  // el motor nunca debe moverse al arrancar

  pinMode(IR_SENSOR, INPUT_PULLUP);

  Serial.println();
  Serial.println("CAMEL RACE - TEST IR + MOTOR");
  Serial.println("Sistema iniciado");

  // Estado inicial: se muestra pero NO se trata como evento.
  lastRawLevel = digitalRead(IR_SENSOR);
  lastRawChangeMs = millis();
  beamBlocked = (lastRawLevel == IR_BLOCKED_LEVEL);

  Serial.print("Estado inicial -> IR: ");
  Serial.println(beamBlocked ? "BLOQUEADO" : "LIBRE");
  Serial.println("Esperando transicion LIBRE -> BLOQUEADO...");
}

void loop() {
  if (updateBeamState()) {
    if (beamBlocked) {
      Serial.println("IR: BLOQUEADO");
      Serial.print("Evento -> avance de ");
      Serial.print(TEST_STEPS);
      Serial.println(" half-steps");
      moveMotor(TEST_STEPS);
      Serial.println("Movimiento completado. Motor desenergizado.");

      // Tras el movimiento bloqueante, re-sincronizar el filtro para no
      // interpretar como cambio lo ocurrido mientras el motor giraba.
      lastRawLevel = digitalRead(IR_SENSOR);
      lastRawChangeMs = millis();
    } else {
      Serial.println("IR: LIBRE");
    }
  }

  delay(2);
}
