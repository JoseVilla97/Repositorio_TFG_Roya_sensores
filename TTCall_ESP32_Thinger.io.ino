/*// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22*/


#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DISABLE_TLS_ //TLS needs to be disabled if using ESP32 (Esto puede ser a algún bug de memoria, y no soporta algunas librerías)
#define TINY_GSM_MODEM_SIM800 //Usamos el modem correcto Nota: importante que esto se defina antes de hacer uso de las librerías
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb//permite que se puedan recibir y almacenar hasta 1024 bytes de datos en el buffer antes de que sea necesario leerlos o procesarlos


#include <TinyGsmClient.h> //bibliotecas para comunicacion
#include <ThingerTinyGSM.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <SimpleTimer.h>//biblioteca para el timer https://playground.arduino.cc/Code/SimpleTimer/
#include <ThingerESP32.h> //Configuracion del ESP32
#include <SparkFunBME280.h>
#include <Wire.h>
#include <RTClib.h>   // incluye libreria para el manejo del modulo RTC
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI. por si tengo que llegar a usar esos pines del BME280
#include <ESP32Time.h>
#include <SimpleTimer.h>//biblioteca para el timer https://playground.arduino.cc/Code/SimpleTimer/
#include <esp_sleep.h>

#define APN_NAME "kolbi3g" //Cargamos los datos del APN
#define APN_USER ""
#define APN_PSWD ""

//Thinger credentials
#define USERNAME "JOQ"    //Thinger Account User Name
#define DEVICE_ID "BME"    //Thinger device IC
#define DEVICE_CREDENTIAL "Zx9sUWVQVGl#Ds9w" //Thinger device credential (password)
ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);

#define MODEM_RST            5//PIN DE RESET MODULO GSM
#define Soil_PIN 25 //sensor de humedad del suelo
#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX/ necesario invertirlos para que funcione
#define TXD1 26//Ademas son los pines dedicados a la SIM800
#define ADC7 35 //Pin de lectura de voltaje
#define CLOCK_INTERRUPT_PIN 2 // Pin conectado al SQW del RTC

//Es posible que se presenten problemas con las direcciones en el BUS I2C pero podemos usar el programa ESCANERRTC que está en el reporsitorio de GIT 
#define DS3231_I2C_ADDRESS 0x75 //Según el escaneo de direcciones para el DS3231 tenemos Addresses 0x57, 0x68, 0x75
#define BME280_I2C_ADDRESS 0x77 //Según el escaneo para el BME280 tenemos Addresses 0x77


RTC_DS3231 rtc;     // crea objeto del tipo RTC_DS3231
RTC_DATA_ATTR int bootCount = 0;//variable para contar cuántas veces se ha iniciado el dispositivo. Esta variable se guarda incluso después de reinicios gracias al atributo RTC_DATA_ATTR
bool alarma = true;    // variable de control con valor verdadero

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60         /* Tiempo en que ESP32 entrará en reposo (en segundos) */

BME280 mySensor;
float humidity;
float humidity_SET;
float pressure;
float altitud;
float temperature;
float temperature_SET;
float DewPoint;
float batteryLevel;
int sensorValue;
float voltage;

int soil_value = 0;//almacena el valor de lectura del Soil_PIN 
float Voltage2;
//float sensorVal;
float percent_Humididy;
float Contenido_Vol_real; // valor real calculado con la relacion entre lectura de tension y contenido vol de agua en suelo
float pendiente = 2.48; // Pendiente obtenida de la regresion
float intercepcion = -0.72; // intercepcion obtenida de la regresion
//float dry = 2.80; // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V
//float wet = 1.20; // Voltage value for wet soil
float dry = 3.15; // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V
float wet = 1.9; // Voltage value for wet soil


SimpleTimer timer;//se declara el objeto timer para evitar detener el sistema
char data;

void setup() {
  Serial.begin(115200);// open serial for debugging
  delay(10);
  
    // Realizar un reset del módulo al iniciar
  digitalWrite(MODEM_RST, LOW); //Al colocar el código de reset en este punto, nos aseguramos de que el módulo GSM comience en un estado conocido antes de que se realicen otras operaciones.
  delay(100);  // Esperar 100 milisegundos
  digitalWrite(MODEM_RST, HIGH);
  delay(10);   // Pequeña espera adicional tras el reset

  pinMode(23, OUTPUT); //Encender el ESP32, ver el Pin Diagram del fabricante
  digitalWrite(23, LOW);
  delay(1000);
  digitalWrite(23, HIGH);

  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(6000);
  Serial.println("Starting Thinger GSM conection o inicializando moden");
  delay(1000);
  Wire.begin();
  rtc.begin();
  
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  if (!rtc.begin()) {
    Serial.print("RTC no encontrado");
    //while (1);
  }
  //rtc.adjust(DateTime(__DATE__, __TIME__));  // funcion que permite establecer fecha y horario del ordenador


  mySensor.setI2CAddress(BME280_I2C_ADDRESS);
  if (mySensor.beginI2C() == false) { //Begin communication over I2C para BME280
    Serial.println("The sensor did not respond. Please check wiring.");
    //while (1); //Si no se cumple condicional que se detenga (Freeze)
  }

  thing.setAPN(APN_NAME, APN_USER, APN_PSWD);// set APN, you can remove user and password from call if your apn does not require them
  thing["millis"] >> outputValue(millis());
  thing["BME280"] >> [](pson & out) { //Leemos los sensores y enviamos los datos
    out["humidity"] = humidity_SET;
    out["temperature"] = temperature_SET;
    out["Dewpoint"] = DewPoint;
    out["pressure"] = pressure;
    out["altitude"] = altitud;
    out["Soil moisture"] = percent_Humididy;
    out["Battery level"] = batteryLevel;
    out["Device_1"] = String(DEVICE_ID);
  };

  bootCount = bootCount + 1;
  Serial.println("Boot number: " + String(bootCount));

  print_wakeup_reason();
  timer.setInterval(5000, Sensor);// Puse 4 s para no ver tantos datos
}


void loop() {
  thing.handle();// thinger.io: Please, take into account to do not add any delay inside the loop()
  timer.run();
}

void print_wakeup_reason() { //muestra en el monitor serie la razón por la cual el ESP32 salió del modo de sueño profundo.
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default :
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void Sensor() { // Función que se ejecuta periodicamente
  DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  Serial.print(fecha.day());        // funcion que obtiene el dia de la fecha completa
  Serial.print("/");                // caracter barra como separador
  Serial.print(fecha.month());      // funcion que obtiene el mes de la fecha completa
  Serial.print("/");                // caracter barra como separador
  Serial.print(fecha.year());       // funcion que obtiene el año de la fecha completa
  Serial.print(" ");                // caracter espacio en blanco como separador
  Serial.print(fecha.hour());       // funcion que obtiene la hora de la fecha completa
  Serial.print(":");                // caracter dos puntos como separador
  Serial.print(fecha.minute());     // funcion que obtiene los minutos de la fecha completa
  Serial.print(":");                // caracter dos puntos como separador
  Serial.println(fecha.second());   // funcion que obtiene los segundos de la fecha completa

  if (fecha.hour() == 18 && fecha.minute() == 5) { //si la hora es 6:00 am
    if ( alarma == true ) {       // Activar un sensor a las 18:05 (6:05 pm) si la alarma está activada.
      Serial.println("El sensor se activará por 1 minuto");

      //código para activar el sensor
      Serial.print(fecha.day());        // funcion que obtiene el dia de la fecha completa
      Serial.print("/");                // caracter barra como separador
      Serial.print(fecha.month());      // funcion que obtiene el mes de la fecha completa
      Serial.print("/");                // caracter barra como separador
      Serial.print(fecha.year());       // funcion que obtiene el año de la fecha completa
      Serial.print(" ");                // caracter espacio en blanco como separador
      Serial.print(fecha.hour());       // funcion que obtiene la hora de la fecha completa
      Serial.print(":");                // caracter dos puntos como separador
      Serial.print(fecha.minute());     // funcion que obtiene los minutos de la fecha completa
      Serial.print(":");                // caracter dos puntos como separador
      Serial.println(fecha.second());   // funcion que obtiene los segundos de la fecha completa
      alarma = false;         // carga valor falso en variable de control
      //código para apagar el sensor
      Serial.println("El sensor se ha apagado");
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //Luego, el ESP32 se desactiva después de 1 minuto
      esp_deep_sleep_start();//y el dispositivo entra en modo de sueño profundo.
    }
  }

  if ( fecha.hour() == 2 && fecha.minute() == 0 ) {    // si hora = 2 y minutos = 0 restablece valor de variable de control en verdadero
    alarma = true;
  }

  Serial.println();
  Serial.print("Humidity : ");
  humidity = mySensor.readFloatHumidity();
  humidity_SET = (0.94 * humidity) + 7.59; //0.8892*humidity+11.267 //ecuacion de calibración mediante regresión
  Serial.print(humidity_SET, 0);
  Serial.println("%");

  Serial.print("Humidity 2 %: ");
  humidity = mySensor.readFloatHumidity();
  Serial.print(humidity, 0);
  Serial.println();

  Serial.print("Pressure : ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  pressure = mySensor.readFloatPressure();
  Serial.print(pressure, 0);
  Serial.println("(Pa)");

  Serial.print("Alt : ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  altitud = mySensor.readFloatAltitudeMeters();
  Serial.print(altitud, 1);
  Serial.println("(m)");

  Serial.print("Temp (°C): "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature = mySensor.readTempC();
  temperature_SET = (0.88 * temperature) + 3.52; //Nose si log solo acepta double
  Serial.print(temperature_SET, 2);
  Serial.println();

  Serial.print("Temp 2 : "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature = mySensor.readTempC();
  Serial.print(temperature, 2);
  Serial.println("°C");

  Serial.print("Dewpoint Programa: ");
  DewPoint = mySensor.dewPointC();
  Serial.print(DewPoint);
  Serial.println("°C");

  soil_value = analogRead(Soil_PIN);
  Serial.print("Soil Moisture Sensor Voltage: "); // Por medio de una simple regla de 3
  Voltage2 =(soil_value/4095)*3.3; //Calculo el voltaje basado en la lectura del ADC del ESP32, teniendo en cuenta que trabaja a 12 bits por defecto.
  Serial.print(Voltage2);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
  Serial.println(" V");// lo ocupo en voltage

  //Formulas es θv=(a/V)+b //lectura de tensión tomada de nuestro sensor electromagnético puede relacionarse con el contenido volumétrico de agua mediante un ajuste lineal
  Contenido_Vol_real=(pendiente*(1/Voltage2))+intercepcion; //Calculo de contenido volumètrico de agua
  Serial.print("Contenido volumetrico de agua en suelo: ");
  Serial.print(Contenido_Vol_real);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095 
  Serial.println(" cm^3/cm^3");// proporción de volumen de agua con respecto al volumen total del suelo
  
  percent_Humididy = map(soil_value, dry, wet, 0, 100);//esto cambia según la calibración 1.5263*In (x) +1.011
  if (percent_Humididy >= 100) {
    percent_Humididy = 100;
  }
  else if (percent_Humididy <= 0) {
    percent_Humididy = 0;
  }
  Serial.print("Soil moisture: ");
  Serial.print(percent_Humididy, 1);
  Serial.println("%");

  Serial.print("Battery level: ");
  batteryLevel = getBatteryLevel();
  Serial.print(batteryLevel, 1);
  Serial.println("%");

}

//Prueba de lectura del nivel de bateria usando un pin para no usar AT comando
float getBatteryLevel() {
  sensorValue = analogRead(ADC7); // Leer el valor analógico del pin 35 (ADC1_CH7)
  voltage = sensorValue / 4095.0 * 2.0 * 3.3; // Convertir el valor analógico en voltaje (suponiendo una referencia de 3.3V)
  batteryLevel = ((voltage - 3.0) / 0.7 ) * 100.0; // Calcular el nivel de batería en porcentaje (suponiendo una batería LiPo con voltaje nominal de 3.7V y voltaje mínimo de 3.0V)
  if (batteryLevel > 100) batteryLevel = 100; //Porcentaje=(Vactual−Vmin)/(Vmax−Vmin)×100
  else if (batteryLevel < 5) batteryLevel = 0;
  return batteryLevel;
  //Con una resolución de 12bits, una resolución de 3.3 voltios / 4096 unidades, es equivalente a 0.8 mV por paso.
  //Usamos el 2.0 para mantener el voltaje del ADC en un rango seguro/ teniendo en cuenta un divisor de tensión que reduce a la mitad el voltaje de la Bat
}
