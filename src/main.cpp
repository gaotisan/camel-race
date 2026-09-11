// CAMEL RACE - Fase A aislada: SOLO deteccion de barrera IR
//
// El motor queda desactivado a proposito (ENABLE_MOTOR 0) para que esta prueba
// tenga una unica variable: si el receptor IR cambia de estado o no.
//
// Comportamiento:
//   - Serial 115200.
//   - Informa de cada cambio estable de estado de la barrera.
//   - Cada segundo emite una linea de latido con la lectura CRUDA del pin y el
//     numero de transiciones acumuladas. Si ese contador nunca sube, el
//     problema es de hardware (alineacion, polaridad, resistencia, GND), no de
//     software.
//
// Para volver a habilitar el motor: poner ENABLE_MOTOR a 1.

#include <Arduino.h>

#define ENABLE_MOTOR 0

// Modo de diagnostico del receptor:
//   1 = digital con pull-up interno  (el modo normal de la prueba)
//   2 = analogico crudo, 0..4095     (requiere pull-up EXTERNO, ver abajo)
#define IR_DIAG_MODE 1

// ---------------------------------------------------------------------------
// Cableado (ver CLAUDE.md - no cambiar sin indicarlo expresamente)
// ---------------------------------------------------------------------------
constexpr int IR_SENSOR = 15;    // Señal del fototransistor receptor

#if ENABLE_MOTOR
constexpr int MOTOR_PIN_1 = 23;  // IN1 - naranja
constexpr int MOTOR_PIN_2 = 22;  // IN2 - amarillo
constexpr int MOTOR_PIN_3 = 21;  // IN3 - verde
constexpr int MOTOR_PIN_4 = 19;  // IN4 - azul
#endif

// Nivel logico que corresponde a "haz cortado".
// Si en la prueba fisica la logica sale invertida, cambiar a HIGH.
constexpr int IR_BLOCKED_LEVEL = LOW;

// ---------------------------------------------------------------------------
// Parametros de la prueba
// ---------------------------------------------------------------------------
constexpr unsigned long DEBOUNCE_MS = 30;     // filtro antirrebote
constexpr unsigned long HEARTBEAT_MS = 1000;  // periodo del latido de diagnostico

#if ENABLE_MOTOR
constexpr int TEST_STEPS = 256;               // half-steps por evento
constexpr unsigned long STEP_DELAY_US = 1500; // velocidad conservadora

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
#endif

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
bool beamBlocked = false;           // estado estable actual (ya filtrado)
int lastRawLevel = HIGH;            // ultima lectura cruda del pin
unsigned long lastRawChangeMs = 0;  // instante de ese ultimo cambio crudo
unsigned long rawTransitions = 0;   // cuantas veces ha cambiado la lectura cruda
unsigned long lastHeartbeatMs = 0;

#if ENABLE_MOTOR
// ---------------------------------------------------------------------------
// Motor (desactivado en esta fase)
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
#endif  // ENABLE_MOTOR

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
    rawTransitions++;
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

// Latido de diagnostico: lo que de verdad nos dice si el pin esta vivo.
void printHeartbeat() {
#if IR_DIAG_MODE == 2
  int value = analogRead(IR_SENSOR);
  Serial.print("[diag] analogico=");
  Serial.print(value);
  Serial.print(" / 4095   (haz libre y haz cortado deben dar valores distintos)");
  Serial.println();
#else
  Serial.print("[diag] pin crudo=");
  Serial.print(lastRawLevel == HIGH ? "HIGH" : "LOW ");
  Serial.print("  estado=");
  Serial.print(beamBlocked ? "BLOQUEADO" : "LIBRE    ");
  Serial.print("  transiciones=");
  Serial.println(rawTransitions);
#endif
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

#if ENABLE_MOTOR
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);
  releaseMotor();  // el motor nunca debe moverse al arrancar
#endif

#if IR_DIAG_MODE == 2
  // analogRead() desactiva el pull-up interno: hace falta uno externo.
  pinMode(IR_SENSOR, INPUT);
#else
  pinMode(IR_SENSOR, INPUT_PULLUP);
#endif

  Serial.println();
  Serial.println("CAMEL RACE - TEST BARRERA IR (motor desactivado)");
  Serial.print("Pin del receptor: GPIO");
  Serial.println(IR_SENSOR);
  Serial.print("Modo de lectura: ");
#if IR_DIAG_MODE == 2
  Serial.println("ANALOGICO (requiere pull-up externo)");
#else
  Serial.println("DIGITAL con pull-up interno");
#endif
  Serial.print("Nivel considerado BLOQUEADO: ");
  Serial.println(IR_BLOCKED_LEVEL == LOW ? "LOW" : "HIGH");
  Serial.println("Sistema iniciado");

  // Estado inicial: se muestra pero NO se trata como evento.
  lastRawLevel = digitalRead(IR_SENSOR);
  lastRawChangeMs = millis();
  lastHeartbeatMs = millis();
  beamBlocked = (lastRawLevel == IR_BLOCKED_LEVEL);

  Serial.print("Estado inicial -> IR: ");
  Serial.println(beamBlocked ? "BLOQUEADO" : "LIBRE");
  Serial.println("Tapa y destapa el haz. Si 'transiciones' no sube, es hardware.");
}

void loop() {
  if (updateBeamState()) {
    Serial.println(beamBlocked ? "IR: BLOQUEADO" : "IR: LIBRE");

#if ENABLE_MOTOR
    if (beamBlocked) {
      Serial.print("Evento -> avance de ");
      Serial.print(TEST_STEPS);
      Serial.println(" half-steps");
      moveMotor(TEST_STEPS);
      Serial.println("Movimiento completado. Motor desenergizado.");

      // Tras el movimiento bloqueante, re-sincronizar el filtro para no
      // interpretar como cambio lo ocurrido mientras el motor giraba.
      lastRawLevel = digitalRead(IR_SENSOR);
      lastRawChangeMs = millis();
    }
#endif
  }

  if (millis() - lastHeartbeatMs >= HEARTBEAT_MS) {
    lastHeartbeatMs = millis();
    printHeartbeat();
  }

  delay(2);
}
