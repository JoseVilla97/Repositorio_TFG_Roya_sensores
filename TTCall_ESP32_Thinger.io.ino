
#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DISABLE_TLS_ //TLS needs to be disabled if using ESP32 (not sure why, this is a known bug)// leido en un foro de arduino IDE y ahora funciona
#define TINY_GSM_MODEM_SIM800 //Usamos el modem correcto Nota: importante que esto se defina antes de hacer uso de las librerías

#include <TinyGsmClient.h> //bibliotecas para comunicacion
#include <ThingerTinyGSM.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <SimpleTimer.h>//biblioteca para el timer https://playground.arduino.cc/Code/SimpleTimer/
#include <ThingerESP32.h> //Configuracion del ESP32
#include <SparkFunBME280.h>
#include <Wire.h>

#define APN_NAME "kolbi3g" //Cargamos los datos del APN
#define APN_USER ""
#define APN_PSWD ""

//Thinger credentials
#define USERNAME "JOQ"    //Thinger Account User Name
#define DEVICE_ID "BME"    //Thinger device IC
#define DEVICE_CREDENTIAL "Zx9sUWVQVGl#Ds9w" //Thinger device credential (password)
ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);


#define Soil_PIN 25
#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX
#define TXD1 26
int soil_value = 0;

int percent_Humididy;

//Esto va a cambiar según la calibración
int dry = 3080; // 
int wet = 1160; // 

BME280 mySensor;
float humidity;
float pressure;
float altitud;
int temperature;


SimpleTimer timer;//se declara el objeto timer
char data;

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
  Serial.println(" Starting Thinger GSM conection o inicializando moden");//inicializo
  delay(1000);
  
  if (mySensor.beginI2C() == false) //Begin communication over I2C
  {
    Serial.println("The sensor did not respond. Please check wiring.");
    while(1); //Freeze
  }

  thing.setAPN(APN_NAME, APN_USER, APN_PSWD);// set APN, you can remove user and password from call if your apn does not require them
  thing["millis"] >> outputValue(millis());
  thing["BME280"] >> [](pson & out) { //Leemos los sensores y enviamos los datos
    out["humidity"] = humidity;
    out["temperature"] = temperature;
    out["pressure"] = pressure;
    out["altitud"] = altitud;
    out["Soil moisture"] = percent_Humididy;
    out["Device_1"] = String(DEVICE_ID);
  };
  timer.setInterval(5000, Sensor);// Puse 5 s para no ver tantos datos 

}


void loop() {
  thing.handle();// thinger.io: Please, take into account to do not add any delay inside the loop()
  timer.run();
}


void Sensor() { // a function to be executed periodically
  Serial.print("Humidity %: ");
  humidity = mySensor.readFloatHumidity();
  //0.89*humidity+11.267 //ecuacion de calibración mediante regresión
  Serial.print(humidity);
//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  Serial.print(" Pressure: ");
  pressure = mySensor.readFloatPressure();
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
  
  soil_value = analogRead(Soil_PIN);
  
  percent_Humididy = map(soil_value, dry, wet, 0, 100);//esto cambia según la calibración 1.5263*In (x) +1.011
  if (percent_Humididy >= 100) {
    percent_Humididy = 100;
  }
  else if (percent_Humididy <= 0) {
    percent_Humididy = 0;
  }
  Serial.print(" Soil moisture: ");
  Serial.println(percent_Humididy);

  float batteryLevel = getBatteryLevel();
  Serial.print("Nivel de bateria: ");
  Serial.print(batteryLevel);
  Serial.println("%");

}

//Prueba de lectura del nivel de bateria usando un pin para no usar AT comando
float getBatteryLevel() {
  int sensorValue = analogRead(35); // Leer el valor analógico del pin 35 (ADC1_CH7)
  float voltage = sensorValue / 4095.0 * 2.0 * 3.3; // Convertir el valor analógico en voltaje (suponiendo una referencia de 3.3V)
  float batteryLevel = (voltage - 3.3) / 0.6 * 100.0; // Calcular el nivel de batería en porcentaje (suponiendo una batería LiPo con voltaje nominal de 3.7V y voltaje mínimo de 3.0V)
  return batteryLevel;

}
