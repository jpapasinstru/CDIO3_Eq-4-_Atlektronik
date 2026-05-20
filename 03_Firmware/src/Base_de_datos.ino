#include <Preferences.h>
#include "HX711.h"

// --- CONFIGURACIÓN HARDWARE ---
const int DOUT = 21;
const int CLK = 22;
HX711 balanza;

float factorDeCalibracion = 9177.42; 
const float GRAVEDAD = 9.80665; // g ≈ 9.81 m/s²

// --- ESTRUCTURA DE DATOS ---
struct Usuario {
  char nombre[20];
  int edad;
  float peso;             // En kg
  float ultimosGolpes[5]; // En Newtons
  float recordPersonal;   // En Newtons
};

Usuario jugadorActual;
Preferences prefs;

// Detección de impacto
float umbralGolpe = 5.0; // kg
float picoActualKG = 0.0;
bool leyendoGolpe = false;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); 
  
  balanza.begin(DOUT, CLK);
  balanza.set_scale(factorDeCalibracion);
  balanza.tare(20);
  
  cargarDatos();
  actualizarNextion();
}

void loop() {
  // 1. Escuchar si la Nextion envía nuevos datos de usuario
  recibirDatosNextion();

  // 2. Lógica de detección de impacto (tu código actual del HX711)
  float lecturaKG = balanza.get_units(1);
  
  if (lecturaKG > umbralGolpe) {
    leyendoGolpe = true;
    if (lecturaKG > picoActualKG) picoActualKG = lecturaKG;
  } 
  else if (lecturaKG < umbralGolpe && leyendoGolpe) {
    float fuerzaNewton = picoActualKG * GRAVEDAD;
    registrarNuevoGolpe(fuerzaNewton, picoActualKG);
    
    picoActualKG = 0.0;
    leyendoGolpe = false;
    delay(1000); 
  }
}

// --- LÓGICA DE REGISTRO Y COMPARACIÓN ---

void registrarNuevoGolpe(float fuerzaN, float fuerzaKG) {
  // 1. Guardamos el golpe anterior para comparar mejora
  float golpeAnteriorN = jugadorActual.ultimosGolpes[0];
  
  // 2. Actualizar Récord Personal (en Newtons)
  if (fuerzaN > jugadorActual.recordPersonal) {
    jugadorActual.recordPersonal = fuerzaN;
  }

  // 3. Mapeo de la Barra j0 (0 a 600 kg -> 0 a 100%)
  int valorBarra = map(constrain(fuerzaKG, 0, 600), 0, 600, 0, 100);
  enviarComandoNextion("j0.val=" + String(valorBarra));

  // 4. Lógica de Evaluación (Los 4 Casos)
  float metaN = (jugadorActual.peso * 3.0) * GRAVEDAD; // Meta de 3 veces su peso
  bool esBuenGolpe = (fuerzaN >= metaN);
  bool estaMejorando = (fuerzaN > golpeAnteriorN);
  
  String mensaje = "";
  if (golpeAnteriorN > 0) {
    if (esBuenGolpe && estaMejorando) 
      mensaje = "El golpe esta en el rango de un buen golpeo y estas mejorando";
    else if (esBuenGolpe && !estaMejorando) 
      mensaje = "Tu golpe es indicado o bueno, pero no estas mejorando";
    else if (!esBuenGolpe && estaMejorando) 
      mensaje = "No estas en el rango del golpeo bien, pero estas mejorando";
    else 
      mensaje = "No estas en el rango del golpeo bien y no estas progresando";
  } else {
    mensaje = "Primer impacto registrado. ¡Sigue asi!";
  }

  // 5. Desplazamiento de Historial (Mantiene los últimos 5)
  for (int i = 4; i > 0; i--) {
    jugadorActual.ultimosGolpes[i] = jugadorActual.ultimosGolpes[i-1];
  }
  jugadorActual.ultimosGolpes[0] = fuerzaN;

  // 6. Envío de datos a los indicadores de la Nextion
  enviarComandoNextion("tAnalisis.txt=\"" + mensaje + "\""); // El recuadro de técnica
  enviarComandoNextion("Fuerza.val=" + String((int)fuerzaN));   // Indicador numérico actual
  enviarComandoNextion("Record.val=" + String((int)jugadorActual.recordPersonal)); // Récord histórico
  
  guardarDatos();      // Guarda en la memoria Flash del ESP32
  actualizarNextion(); // Actualiza los nombres Usuario1..5 y g0..4
}
// --- COMUNICACIÓN Y MEMORIA ---

void enviarComandoNextion(String cmd) {
  Serial2.print(cmd);
  Serial2.write(0xff); Serial2.write(0xff); Serial2.write(0xff);
}

void actualizarNextion() {
  // Actualización de los indicadores de historial gX y nombres UsuarioX
  for (int i = 0; i < 5; i++) {
    // Valores de golpe en Newtons
    String compValor = "g" + String(i);
    String txtValor = (jugadorActual.ultimosGolpes[i] > 0) ? String(jugadorActual.ultimosGolpes[i], 1) + " N" : "---";
    enviarComandoNextion(compValor + ".txt=\"" + txtValor + "\"");

    // Nombres del historial
    String compNombre = "Usuario" + String(i + 1);
    String txtNombre = (jugadorActual.ultimosGolpes[i] > 0) ? String(jugadorActual.nombre) : "Vacio";
    enviarComandoNextion(compNombre + ".txt=\"" + txtNombre + "\"");
  }
  
  // Actualizar indicadores principales
  enviarComandoNextion("Fuerza.val=" + String((int)jugadorActual.ultimosGolpes[0]));
  enviarComandoNextion("Record.val=" + String((int)jugadorActual.recordPersonal));
}

void guardarDatos() {
  prefs.begin("golpeometro", false);
  prefs.putBytes("usuario", &jugadorActual, sizeof(Usuario));
  prefs.end();
}

void cargarDatos() {
  prefs.begin("golpeometro", true);
  prefs.getBytes("usuario", &jugadorActual, sizeof(Usuario));
  prefs.end();
}
void recibirDatosNextion() {
  if (Serial2.available()) {
    String datos = Serial2.readStringUntil('\n');
    datos.trim();

    if (datos.startsWith("registro|")) {
      datos = datos.substring(9); // Saltamos "registro|"

      int primerPipe = datos.indexOf('|');
      int segundoPipe = datos.indexOf('|', primerPipe + 1);

      if (primerPipe != -1 && segundoPipe != -1) {
        // 1. Extraer Nombre (desde el inicio hasta el primer |)
        String nombreStr = datos.substring(0, primerPipe);
        nombreStr.toCharArray(jugadorActual.nombre, 20);

        // 2. Extraer Edad (entre el primer y segundo |)
        jugadorActual.edad = datos.substring(primerPipe + 1, segundoPipe).toInt();

        // 3. Extraer Peso (desde el segundo | hasta el final)
        jugadorActual.peso = datos.substring(segundoPipe + 1).toFloat();

        // Limpiamos historial para el nuevo usuario
        for(int i=0; i<5; i++) jugadorActual.ultimosGolpes[i] = 0;
        jugadorActual.recordPersonal = 0;

        guardarDatos();      // Guardamos en la Flash
        actualizarNextion(); // Refrescamos la pantalla con el nuevo nombre
        
        // Enviamos confirmación al recuadro de técnica
        enviarComandoNextion("tAnalisis.txt=\"Usuario: " + String(jugadorActual.nombre) + " registrado\"");
      }
    }
  }
}