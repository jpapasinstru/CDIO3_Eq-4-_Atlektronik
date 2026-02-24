Código fuente .ino, .c, .py
CODIGO DEL PROCESO DE CALIBRACION DE LA CELDA DE CARGA Y MODULO HX711 ADC
#include "HX711.h"

// Pines para el módulo HX711
const int DT_PIN = 2;
const int SCK_PIN = 3;

HX711 scale;

// --- Parámetros de ajuste ---
float factor_calibracion = -7050.0; // Este valor lo ajustas con un peso conocido
float fuerza_maxima = 0;
unsigned long tiempo_ultimo_golpe = 0;
const int tiempo_espera = 3000; // 3 segundos para mostrar el resultado antes de resetear

void setup() {
  Serial.begin(9600);
  Serial.println("--- Golpeómetro Iniciado ---");
  
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(factor_calibracion); 
  scale.tare(); // Poner a cero al iniciar (asegúrate que no haya peso sobre la celda)
  
  Serial.println("Listo para el impacto...");
}

void loop() {
  // 1. Obtener la lectura actual en la unidad deseada (kg, lb, etc.)
  float lectura_actual = scale.get_units(1); // Promedio de 1 lectura para mayor velocidad

  // 2. Detectar el pico del golpe
  if (lectura_actual > fuerza_maxima) {
    fuerza_maxima = lectura_actual;
    tiempo_ultimo_golpe = millis(); // Reiniciar el temporizador al detectar fuerza
  }

  // 3. Mostrar el resultado si hubo un impacto significativo (ej. mayor a 1kg)
  if (fuerza_maxima > 1.0) {
    Serial.print("Impacto Detectado: ");
    Serial.print(fuerza_maxima, 2);
    Serial.println(" kg");

    // 4. Lógica de Reset: Si pasan 3 segundos sin una fuerza mayor, reiniciamos el marcador
    if (millis() - tiempo_ultimo_golpe > tiempo_espera) {
      Serial.println("-------------------------");
      Serial.println("Pico registrado. Reiniciando...");
      Serial.println("Listo para el siguiente golpe!");
      fuerza_maxima = 0;
      scale.tare(); // Re-calibrar el cero por si se movió la estructura
    }
  }
}
Codigo celda de carga
