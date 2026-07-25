#define BLYNK_TEMPLATE_ID "TMPL2BbcTN23o"
#define BLYNK_TEMPLATE_NAME "Agrovision"
#define BLYNK_AUTH_TOKEN "fxwDnQ_0F01d8_gJJEpq9sQCQGTlpesf"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <AccelStepper.h>

char ssid[] = "OPPO A38 d5es";
char pass[] = "ernestin";

// ==================================================
// PUENTE H L298N - MOTORES DC
// ==================================================

const int IN1 = 5;   // D1 - GPIO5
const int IN2 = 13;  // D7 - GPIO13
const int IN3 = 0;   // D3 - GPIO0
const int IN4 = 2;   // D4 - GPIO2

const int ENA = 12;  // D6 - GPIO12
const int ENB = 15;  // D8 - GPIO15

// ==================================================
// A4988 - NEMA 17
// ==================================================

const int STEP_PIN = 4;   // D2 - GPIO4
const int DIR_PIN = 14;   // D5 - GPIO14

AccelStepper nema(
  AccelStepper::DRIVER,
  STEP_PIN,
  DIR_PIN
);

const long LIMITE_MOVIMIENTO = 1000000000L;

const float VELOCIDAD_MAXIMA = 700.0;
const float ACELERACION = 350.0;

bool botonSubirPresionado = false;
bool botonBajarPresionado = false;

// ==================================================
// CONTROL DE VELOCIDAD DE LAS RUEDAS
// ==================================================

// Valor del slider de Blynk
int porcentajeVelocidad = 40;

// PWM correspondiente a la velocidad seleccionada
int velocidadPWM = 0;

// PWM que actualmente se está aplicando
int pwmActual = 0;

// Impulso inicial para vencer la fricción
const int PWM_IMPULSO = 1023;

// Duración del impulso inicial
const unsigned long DURACION_IMPULSO = 120;

// Configuración del frenado progresivo
const unsigned long INTERVALO_FRENADO = 15;
const int PASO_FRENADO = 20;

bool impulsoActivo = false;
bool carroEnMovimiento = false;
bool frenando = false;

unsigned long inicioImpulso = 0;
unsigned long ultimoCambioPWM = 0;

// ==================================================
// DECLARACIÓN DE FUNCIONES
// ==================================================

void avanzar();
void retroceder();
void girarIzquierda();
void girarDerecha();
void frenar();

void actualizarMovimientoNema();

void establecerVelocidad(int porcentaje);
void aplicarPWM(int pwm);
void iniciarMovimiento();
void actualizarMotoresDC();

// ==================================================
// CONFIGURACIÓN
// ==================================================

void setup() {
  Serial.begin(115200);

  // ------------------------------
  // Configuración del L298N
  // ------------------------------

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  analogWriteRange(1023);
  analogWriteFreq(1000);

  // Motores inicialmente detenidos
  aplicarPWM(0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  // ------------------------------
  // Configuración del NEMA 17
  // ------------------------------

  nema.setMaxSpeed(VELOCIDAD_MAXIMA);
  nema.setAcceleration(ACELERACION);

  // Ancho mínimo del pulso STEP para el A4988
  nema.setMinPulseWidth(2);

  Serial.println();
  Serial.println("Iniciando AgroVision Rover...");
  Serial.println("Conectando con Blynk...");

  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  Serial.println("AgroVision Rover conectado.");
}

// ==================================================
// LOOP PRINCIPAL
// ==================================================

void loop() {
  Blynk.run();

  // Movimiento no bloqueante del NEMA
  nema.run();

  // Impulso y frenado progresivo de las ruedas
  actualizarMotoresDC();
}

// ==================================================
// CONTROL DEL NEMA 17
// ==================================================

void actualizarMovimientoNema() {
  // Solo subir
  if (
    botonSubirPresionado &&
    !botonBajarPresionado
  ) {
    nema.moveTo(LIMITE_MOVIMIENTO);

    Serial.println("NEMA: subiendo");
  }

  // Solo bajar
  else if (
    botonBajarPresionado &&
    !botonSubirPresionado
  ) {
    nema.moveTo(-LIMITE_MOVIMIENTO);

    Serial.println("NEMA: bajando");
  }

  // Ningún botón o ambos presionados
  else {
    nema.stop();

    Serial.println("NEMA: deteniendo");
  }
}

// Botón SUBIR - Blynk V5
BLYNK_WRITE(V5) {
  botonSubirPresionado =
    param.asInt() == 1;

  actualizarMovimientoNema();
}

// Botón BAJAR - Blynk V6
BLYNK_WRITE(V6) {
  botonBajarPresionado =
    param.asInt() == 1;

  actualizarMovimientoNema();
}

// ==================================================
// CONTROL PWM DE LAS RUEDAS
// ==================================================

void aplicarPWM(int pwm) {
  pwm = constrain(pwm, 0, 1023);

  analogWrite(ENA, pwm);
  analogWrite(ENB, pwm);

  pwmActual = pwm;
}

/*
 * Convierte el porcentaje del slider en PWM.
 *
 * 0 %   = motores apagados
 * 1 %   = PWM 450
 * 100 % = PWM 1023
 */
void establecerVelocidad(int porcentaje) {
  porcentaje = constrain(
    porcentaje,
    0,
    100
  );

  if (porcentaje == 0) {
    velocidadPWM = 0;
  } else {
    velocidadPWM = map(
      porcentaje,
      1,
      100,
      450,
      1023
    );
  }

  aplicarPWM(velocidadPWM);
}

// ==================================================
// IMPULSO INICIAL
// ==================================================

void iniciarMovimiento() {
  carroEnMovimiento = true;
  frenando = false;

  impulsoActivo = true;
  inicioImpulso = millis();

  // Impulso breve al 100 %
  aplicarPWM(PWM_IMPULSO);

  Serial.println("Ruedas: impulso inicial");
}

// ==================================================
// FRENADO PROGRESIVO
// ==================================================

void frenar() {
  carroEnMovimiento = false;
  impulsoActivo = false;
  frenando = true;

  ultimoCambioPWM = millis();

  Serial.println("Ruedas: frenado progresivo");
}

// ==================================================
// ACTUALIZACIÓN DE RUEDAS SIN DELAY
// ==================================================

void actualizarMotoresDC() {
  unsigned long tiempoActual = millis();

  // ----------------------------------------------
  // Finalizar el impulso inicial
  // ----------------------------------------------

  if (impulsoActivo) {
    if (
      tiempoActual - inicioImpulso >=
      DURACION_IMPULSO
    ) {
      impulsoActivo = false;

      // Bajar a la velocidad elegida
      establecerVelocidad(
        porcentajeVelocidad
      );

      Serial.print("Velocidad normal: ");
      Serial.print(porcentajeVelocidad);
      Serial.println("%");
    }

    return;
  }

  // ----------------------------------------------
  // Frenado progresivo
  // ----------------------------------------------

  if (frenando) {
    if (
      tiempoActual - ultimoCambioPWM >=
      INTERVALO_FRENADO
    ) {
      ultimoCambioPWM = tiempoActual;

      pwmActual -= PASO_FRENADO;

      if (pwmActual <= 0) {
        pwmActual = 0;
        frenando = false;

        aplicarPWM(0);

        // Apagar completamente las entradas
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);

        Serial.println("Ruedas: detenidas");
      } else {
        aplicarPWM(pwmActual);
      }
    }
  }
}

// ==================================================
// MOVIMIENTO DE LAS RUEDAS
// ==================================================

void avanzar() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  iniciarMovimiento();

  Serial.println("Ruedas: avanzando");
}

void retroceder() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  iniciarMovimiento();

  Serial.println("Ruedas: retrocediendo");
}

void girarIzquierda() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  iniciarMovimiento();

  Serial.println("Ruedas: girando a la izquierda");
}

void girarDerecha() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  iniciarMovimiento();

  Serial.println("Ruedas: girando a la derecha");
}

// ==================================================
// BOTONES BLYNK - RUEDAS
// ==================================================

// Avanzar - V1
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    avanzar();
  } else {
    frenar();
  }
}

// Retroceder - V2
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    retroceder();
  } else {
    frenar();
  }
}

// Girar a la izquierda - V3
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) {
    girarIzquierda();
  } else {
    frenar();
  }
}

// Girar a la derecha - V4
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    girarDerecha();
  } else {
    frenar();
  }
}

// ==================================================
// SLIDER DE VELOCIDAD - V7
// ==================================================

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V7);
}

BLYNK_WRITE(V7) {
  porcentajeVelocidad = constrain(
    param.asInt(),
    0,
    100
  );

  /*
   * Si el carrito ya está moviéndose y terminó
   * el impulso, se actualiza inmediatamente.
   */
  if (
    carroEnMovimiento &&
    !impulsoActivo &&
    !frenando
  ) {
    establecerVelocidad(
      porcentajeVelocidad
    );
  }

  Serial.print("Velocidad seleccionada: ");
  Serial.print(porcentajeVelocidad);
  Serial.println("%");
}