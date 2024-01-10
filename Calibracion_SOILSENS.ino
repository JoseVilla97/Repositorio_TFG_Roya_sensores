#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DISABLE_TLS_
#define TINY_GSM_MODEM_SIM800

#include <TinyGsmClient.h> //bibliotecas para comunicacion
#include <ThingerTinyGSM.h>
#include <ThingerESP32.h> 
#include <Simpletimer.h>//biblioteca para el timer

const float dry = 2.80; // Voltage value for dry soil
const float wet = 1.20; // Voltage value for wet soil

#define Soil_PIN 25
#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX
#define TXD1 26

float Voltaje;
float soil_value ;
int percent_Humididy;
float Contenido_Vol_real; // contenido volumètrico real 
float pendiente = 2.48; // slope from linear fit
float intercepcion = -0.72; // intercept from linear fit

Simpletimer timer1{};
char data;

void setup() {
  Serial.begin(115200);
  pinMode(Soil_PIN, INPUT);
  pinMode(23, OUTPUT);
  digitalWrite(23, LOW);
  delay(1000);
  digitalWrite(23, HIGH);
  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(1000);
  timer1.register_callback(Sensor);

}


void Sensor() {
/**pero para el ESP32 las entradas ADC o GPIO (int-out) trae por defecto 12 bits 
Es importante recalcar que por defecto se trabaja a 12 bits, osea me que tiene que dar un rango entre los (0-4095) 
pero podemos ajustarlo mediante código de 12 a 11 (0-2048) o 10 bits (0-1024) o bien de 9 bits (0-512), sin emabargo para este caso no lo vamos a aplicar**/
  
  soil_value  = analogRead(Soil_PIN);//recibe valores de entrada análoga, en este caso del sensor en el pin 25
  Serial.print("Soil Moisture Sensor Voltage: "); // Por medio de una simple regla de 3
  Voltaje = soil_value /4095*3.3; //Calculo el voltaje basado en la lectura del ADC del ESP32, teniendo en cuenta que trabaja a 12 bits por defecto.
  Serial.print(Voltaje);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
  Serial.println(" V");// lo ocupo en voltage
  
  //Formulas es θv=(a/V)+b //lectura de tensión tomada de nuestro sensor electromagnético puede relacionarse con el contenido volumétrico de agua mediante un ajuste lineal
  Contenido_Vol_real=(pendiente/Voltaje)+intercepcion; //Calculo de contenido volumètrico de agua
  Serial.print("Contenido volumetrico de agua en suelo: ");
  Serial.print(Contenido_Vol_real);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
  Serial.println(" cm^3/cm^3");// proporción de volumen de agua con respecto al volumen total del suelo
  //1 cm³/cm³ (o 1 m³/m³) = 100% de humedad: Significa que todo el volumen del suelo está lleno de agua
  //0 cm³/cm³ = 0% de humedad: Significa que no hay agua en el volumen del suelo. El suelo está completamente seco.


  //En esta secciòn puedo calcular el porcentaje de humedad segun 
  Serial.print("Soil Moisture bits: "); 
  Serial.println(soil_value); 

  
  percent_Humididy = map(Voltaje, dry, wet, 0, 100); //Mapeo del valor del sensor a un porcentaje de humedad entre 0 y 100, donde dry es 0 y wet es 100.
  if (percent_Humididy >= 100) {
    percent_Humididy = 100;
  }
  else if (percent_Humididy <= 0) {
    percent_Humididy = 0;
  }
  Serial.print("Soil moisture: ");
  Serial.print(percent_Humididy);
  Serial.println("%");
}

/*El contenido de agua en el punto de marchitez permanente (cuando las plantas ya no pueden extraer agua) 
y en la capacidad de campo (máximo contenido de agua que el suelo puede retener después de un riego o lluvia)
son valores clave a considerar para la gestión del agua en el suelo.*/

void loop() {
  timer1.run(5000);
}
