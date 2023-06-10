#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DISABLE_TLS_
#define TINY_GSM_MODEM_SIM800

#include <TinyGsmClient.h> //bibliotecas para comunicacion
#include <ThingerTinyGSM.h>
#include <ThingerESP32.h> 
#include <SimpleTimer.h>//biblioteca para el timer

const float dry = 2.80; // value for dry soil
const float wet = 1.20; // value for wet soil


#define sensor_PIN 25
#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX
#define TXD1 26
float sensorVal;
int percent_Humididy;
float Contenido_Vol_real;

SimpleTimer timer;//se declara el objeto timer
char data;

void setup() {
  Serial.begin(115200);
  pinMode(sensor_PIN, INPUT);

  pinMode(23, OUTPUT);
  digitalWrite(23, LOW);
  delay(1000);
  digitalWrite(23, HIGH);
  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(1000);
  
  timer.setInterval(5000, Sensor);

}



void Sensor() { // a function to be executed periodically
  sensorVal = analogRead(sensor_PIN);//recibe valores de entrada análoga, en este caso del sensor en el pin 25


/**pero para el ESP32 las entradas ADC o GPIO (int-out) trae por defecto 12 bits 
Es importante recalcar que por defecto se trabaja a 12 bits, osea me que tiene un rango entre los (0-4095) 
Pero podemos ajustarlo mediante código de 12 a 11 (0-2048) o 10 bits (0-1024) o bien de 9 bits (0-512)**/

  Serial.print("Soil Moisture Sensor Voltage: ");
  float Voltaje =sensorVal/4095*3.3;
  Serial.print(Voltaje);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
  Serial.println(" V");// lo ocupo en voltage
  ////Formulas es θv=(2,48/V)-0,72
//  Contenido_Vol_real=(2,48/Voltaje)-0,72;
////convertir a hunedad
  //porcentaje = (Contenido_Vol_real / Contenido_Vol_real_MAX) * 100;//convertir el voltage a porcentaje
//  Serial.print("Soil moisture real: ");
//  Serial.print(Contenido_Vol_real);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
//  Serial.println(" θv");// lo ocupo en voltage

  
//  Serial.print("Soil Moisture bits: "); 
//  Serial.println(sensorVal); 
//  percent_Humididy = map(sensorVal, dry, wet, 0, 100);
//  if (percent_Humididy >= 100) {
//    percent_Humididy = 100;
//  }
//  else if (percent_Humididy <= 0) {
//    percent_Humididy = 0;
//  }
//  Serial.print(" Soil moisture: ");
//  Serial.println(percent_Humididy);

}


void loop() {
  timer.run();
}
