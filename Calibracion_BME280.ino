
#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DISABLE_TLS_ //TLS needs to be disabled if using ESP32 (not sure why, this is a known bug)// leido en un foro de arduino IDE
#define TINY_GSM_MODEM_SIM800 //Usamos el modem correcto Nota: importante que esto se defina antes de hacer uso de las librerías

#include <TinyGsmClient.h> //bibliotecas para comunicacion
#include <ThingerTinyGSM.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <SimpleTimer.h>//biblioteca para el timer https://playground.arduino.cc/Code/SimpleTimer/
#include <ThingerESP32.h> //Configuracion del ESP32
#include <SparkFunBME280.h>
#include <Wire.h>
#include "SparkFunBME280.h"

BME280 mySensor;
//PIN 22 SCL Y PIN 21 SDA

#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX
#define TXD1 26

#define APN_NAME "kolbi3g" //Cargamos los datos del APN
#define APN_USER ""
#define APN_PSWD ""
//Thinger credentials
#define USERNAME "JOQ"    //Thinger Account User Name
#define DEVICE_ID "BME"    //Thinger device IC
#define DEVICE_CREDENTIAL "Zx9sUWVQVGl#Ds9w" //Thinger device credential (password)
ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);

SimpleTimer timer;//se declara el objeto timer
char data;

int humidity;
int pressure;
float altitud;
float temperature;


/************************************************************************************************************************************************************************/

void setup() {
  Serial.begin(115200);// open serial for debugging
  Wire.begin();
  delay(10);
  pinMode(23, OUTPUT); //ENCENDER EL ESP32, ver el Pin Diagram del fabricante
  digitalWrite(23, LOW);
  delay(1000);
  digitalWrite(23, HIGH);

  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(1000);
  Serial.println();
  Serial.println(" Starting Thinger GSM conection o inicializando moden");
  delay(1000);
  thing.setAPN(APN_NAME, APN_USER, APN_PSWD);// set APN, you can remove user and password from call if your apn does not require them
  
  if (mySensor.beginI2C() == false) //Begin communication over I2C
  {
    Serial.println("The sensor did not respond. Please check wiring.");
    while(1); //Freeze
  }

  timer.setInterval(5000, Sensor);//Se recomienda tomar medidas cada 5 segundos. Si se utiliza un periodo menor puede ocasionar que los datos no sean precisos.
}


void loop() {
  thing.handle();// thinger.io: Please, take into account to do not add any delay inside the loop()
  timer.run();
}

void Sensor() { // a function to be executed periodically
  Serial.print("Humidity %: ");
  humidity = mySensor.readFloatHumidity();
  Serial.print(humidity);

  Serial.print(" Pressure: ");
  pressure = mySensor.readFloatPressure();
  //Y = 1.0*X + 0.33
  //Presion_Ajus = 1.0*pressure + 0.33
  Serial.print(pressure);

  Serial.print(" Alt (m): ");
  altitud = mySensor.readFloatAltitudeMeters();
  Serial.print(altitud);
  //Serial.print(mySensor.readFloatAltitudeFeet(), 1);

  Serial.print(" Temp (°C): ");
  temperature = mySensor.readTempC();
  Serial.print(temperature);
  //Serial.print(mySensor.readTempF(), 2);
  Serial.println();
}
