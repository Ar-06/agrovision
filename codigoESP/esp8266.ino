#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <AccelStepper.h>

char ssid[] = "NOMBRE_WIFI";
char pass[] = "CONTRASEÑA_WIFI";

// ==================================================
// PUENTE H - MOTORES DC
// ==================================================

const int IN1 = 5;   // D1 - GPIO5
const int IN2 = 13;  // D7 - GPIO13
const int IN3 = 0;   // D3 - GPIO0
const int IN4 = 2;   // D4 - GPIO2

// ==================================================
// A4988 - NEMA 17
// ==================================================

const int STEP_PIN = 4;  // D2 - GPIO4
const int DIR_PIN  = 14; // D5 - GPIO14

// DRIVER: control mediante STEP y DIR
AccelStepper nema(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Distancia suficientemente grande para movimiento continuo
const long LIMITE_MOVIMIENTO = 1000000000L;

// Ajusta estos valores según el mecanismo
const float VELOCIDAD_MAXIMA = 700.0; // pasos por segundo
const float ACELERACION = 350.0;      // pasos por segundo²

bool botonSubirPresionado = false;
bool botonBajarPresionado = false;

// ==================================================
// DECLARACIÓN DE FUNCIONES
// ==================================================

void avanzar();
void retroceder();
void girarIzquierda();
void girarDerecha();
void frenar();

void actualizarMovimientoNema();

// ==================================================
// CONFIGURACIÓN
// ==================================================

void setup() {
  Serial.begin(115200);

  // Motores DC
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  frenar();

  // NEMA 17
  nema.setMaxSpeed(VELOCIDAD_MAXIMA);
  nema.setAcceleration(ACELERACION);

  // Ancho mínimo del pulso STEP para el A4988
  nema.setMinPulseWidth(2);

  Serial.println();
  Serial.println("Iniciando AgroVision Rover...");
  Serial.println("Conectando con Blynk...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Serial.println("AgroVision Rover conectado.");
}

// ==================================================
// LOOP PRINCIPAL
// ==================================================

void loop() {
  Blynk.run();

  /*
   * run() debe ejecutarse continuamente.
   * No colocar delays en el loop.
   */
  nema.run();
}

// ==================================================
// CONTROL DEL NEMA 17
// ==================================================

void actualizarMovimientoNema() {
  // Solo subir
  if (botonSubirPresionado && !botonBajarPresionado) {
    nema.moveTo(LIMITE_MOVIMIENTO);

    Serial.println("NEMA: subiendo");
  }

  // Solo bajar
  else if (botonBajarPresionado && !botonSubirPresionado) {
    nema.moveTo(-LIMITE_MOVIMIENTO);

    Serial.println("NEMA: bajando");
  }

  // Ninguno o los dos botones presionados
  else {
    /*
     * stop() calcula una posición de parada que permite
     * desacelerar progresivamente.
     */
    nema.stop();

    Serial.println("NEMA: deteniendo");
  }
}

// Botón SUBIR - Blynk V5
BLYNK_WRITE(V5) {
  botonSubirPresionado = param.asInt() == 1;
  actualizarMovimientoNema();
}

// Botón BAJAR - Blynk V6
BLYNK_WRITE(V6) {
  botonBajarPresionado = param.asInt() == 1;
  actualizarMovimientoNema();
}

// ==================================================
// MOVIMIENTO DE LAS RUEDAS
// ==================================================

void avanzar() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void retroceder() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarIzquierda() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarDerecha() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void frenar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ==================================================
// BOTONES BLYNK - RUEDAS
// ==================================================

// Avanzar
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    avanzar();
  } else {
    frenar();
  }
}

// Retroceder
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    retroceder();
  } else {
    frenar();
  }
}

// Girar a la izquierda
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) {
    girarIzquierda();
  } else {
    frenar();
  }
}

// Girar a la derecha
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    girarDerecha();
  } else {
    frenar();
  }
}