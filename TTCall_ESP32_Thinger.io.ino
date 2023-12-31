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
#define TINY_GSM_MODEM_SIM800 //Usamos el modem correcto Nota: importante que esto se define antes de hacer uso de las librerías
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb//permite que se puedan recibir y almacenar hasta 1024 bytes de datos en el buffer antes de que sea necesario leerlos o procesarlos

#include <TinyGsmClient.h>
#include <ThingerTinyGSM.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <SimpleTimer.h>//biblioteca para el timer https://playground.arduino.cc/Code/SimpleTimer/
#include <ThingerESP32.h> //Configuracion del ESP32
#include <SparkFunBME280.h>//Librería del BME280
#include <Wire.h>
#include <RTClib.h>  // incluye libreria para el manejo del modulo DS3231
#include <ESP32Time.h>
#include <esp_sleep.h>


#define APN_NAME "kolbi3g" //Cargamos los datos del APN
#define APN_USER ""
#define APN_PSWD ""

#define USERNAME "JOQ"    //Thinger Account User Name
#define DEVICE_ID "BME"    //Thinger device IC
#define DEVICE_CREDENTIAL "Zx9sUWVQVGl#Ds9w" //Thinger device credential (password)
ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);

#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23

#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX/ necesario invertirlos para que funcione
#define TXD1 26//Ademas son los pines dedicados a la SIM800
#define ADC7 35 //Pin de lectura de voltaje
#define Soil_PIN 25 //sensor de humedad del suelo
const int alarmPin = 15;

//Es posible que se presenten problemas con las direcciones en el BUS I2C pero podemos usar el programa ESCANERRTC que está en el reporsitorio de GIT
#define DS3231_I2C_ADDRESS 0x68 //Según el escaneo de direcciones para el DS3231 tenemos Addresses 0x57, 0x68
//#define DS3231_ALARM1 0x07    ///< Alarm 1 register
//#define DS3231_ALARM2 0x0B    ///< Alarm 2 register
#define BME280_I2C_ADDRESS 0x77 //Según el escaneo para el BME280 tenemos Addresses 0x75, 0x77 la 77 es la que el escaneo indica como correcta
#define I2C_SDA 21
#define I2C_SCL 22

RTC_DS3231 rtc;     // crea objeto del tipo RTC_DS3231
BME280 mySensor;

//VARIABLES DECLARADAS PARA EL SENSOR BME280
float humidity;
float humidity_SET;
float pressure;
float altitud;
float temperature;
float temperature_SET;
float DewPoint;

//VARIABLES PARA LECTURA DEL NIVEL DE BATERIA
float batteryLevel;
int sensorValue;
float voltage;


//VARAIBLES DEL SENSOR DE HUMEDAD DEL SUELO
int soil_value = 0;//almacena el valor de lectura del Soil_PIN
float Voltage2;
float percent_Humididy;
float Contenido_Vol_real; // valor real calculado con la relacion entre lectura de tension y contenido vol de agua en suelo
float pendiente = 2.48; // Pendiente obtenida de la regresion
float intercepcion = -0.72; // intercepcion obtenida de la regresion
float dry = 3.15; // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V
float wet = 1.9; // Voltage value for wet soil
int readCount = 0;// conteo de las veces que envia datos al thinger.io

SimpleTimer timer;//se declara el objeto timer para evitar detener el sistema
char data;


void Sensor() { // Función que se ejecuta periodicamente
  for (int i = 0; i < 10; i++) {//Para que me tire solo 10 lecturas al menos antes de ir a deep sleep
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
    Voltage2 = (soil_value / 4095) * 3.3; //Calculo el voltaje basado en la lectura del ADC del ESP32, teniendo en cuenta que trabaja a 12 bits por defecto.
    Serial.print(Voltage2);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095
    Serial.println(" V");// lo ocupo en voltage

    //Formulas es θv=(a/V)+b //lectura de tensión tomada de nuestro sensor electromagnético puede relacionarse con el contenido volumétrico de agua mediante un ajuste lineal
    Contenido_Vol_real = (pendiente * (1 / Voltage2)) + intercepcion; //Calculo de contenido volumètrico de agua
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

    delay(1000);
  }
  // Check if SQW pin shows alarm has fired
  Serial.println("");
  Serial.println("Lectura SQW>> 0 == Disparada SQW>>1 == No disparada ");
  Serial.print("] SQW: ");
  Serial.println(digitalRead(alarmPin));
  Serial.print(" Alarm1 : ");
  Serial.println(rtc.alarmFired(1));
  Serial.println("");
  delay(1000);

//  //SEGUNDA FORMA CON LOS FIRED O CON LAS INTERRUPCIONES QUE TRAE EL DS3231 EN LA LIBRERIA INTERRUPCIÓN POR MEDIO DEL SQW O disparado de la alarma 1 y 2
//  if (rtc.alarmFired(1) == true ) { // true == 1 y false == 0 o estado bajo //|| rtc.alarmFired(2) == true por eso al monitorear entes se muestra puro 0
//    Serial.print("] SQW: ");
//    Serial.println(digitalRead(alarmPin));
//    Serial.print(" Alarm 1 : ");
//    Serial.print(rtc.alarmFired(1));
//    Serial.println("  >>>La alarma 1 acaba de dispararse");
//
//    //rtc.disableAlarm(1); // Not used as only disables the alarm on the SQW pin
//    rtc.clearAlarm(1);
//    Serial.println(" - Alarmas are cleared");
//    Serial.println("");
//
//    DateTime now = rtc.now(); // Get the current time
//    char buff[] = "Alarm reiniciada y activada a las hh:mm:ss DDD, DD MMM YYYY";
//    Serial.println(now.toString(buff));
//    //if (!rtc.setAlarm1(DateTime(2023, 6, 25, 15, 5, 30), DS3231_A1_Minute)) {
//    if (!rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 0, 35), DS3231_A1_Second)) {
//      Serial.println("Error, alarm 1 nwasn't set!");
//    } else {
//      Serial.println("Alarm 1 will happen in 10 seconds!");
//    }
//    delay(1000);
//    Serial.println("");
//    deepSleep();
//  } else {
//    delay(1000);
//    Serial.println("");
//    deepSleep();
//  }


  //  if (rtc.alarmFired(2) == true ) { // true == 1 y false == 0 o estado bajo //|| rtc.alarmFired(2) == true por eso al monitorear entes se muestra puro 0
  //    Serial.print("] SQW: ");
  //    Serial.println(digitalRead(alarmPin));
  //    Serial.print(" Alarm 2 : ");
  //    Serial.print(rtc.alarmFired(2));
  //    Serial.println("  >>>La alarma 2 acaba de dispararse");
  //
  //    //rtc.disableAlarm(1); // Not used as only disables the alarm on the SQW pin
  //    rtc.clearAlarm(2);
  //    Serial.println(" - Alarma is cleared");
  //    Serial.println("");
  //
  //    DateTime now = rtc.now(); // Get the current time
  //    char buff[] = "Alarm reiniciada y activada a las hh:mm:ss DDD, DD MMM YYYY";
  //    Serial.println(now.toString(buff));
  //    //if (!rtc.setAlarm2(DateTime(2023, 6, 25, 15, 5, 30), DS3231_A2_Minute)) {// no se puede por segundos solo por minutos
  //    if (!rtc.setAlarm2(rtc.now() + TimeSpan(0, 0, 1, 55), DS3231_A2_Minute)) {
  //      Serial.println("Error, alarm 1 nwasn't set!");
  //    } else {
  //      Serial.println("Alarm 1 will happen in 10 seconds!");
  //    }
  //    delay(1000);
  //    Serial.println("");
  //    deepSleep();
  //  } else {
  //    delay(1000);
  //    Serial.println("");
  //    deepSleep();
  //  }


  //if (rtc.alarmFired(2) == true ) { // true == 1 y false == 0 o estado bajo //|| rtc.alarmFired(2) == true por eso al monitorear entes se muestra puro 0
  if (rtc.alarmFired(1) == true || rtc.alarmFired(2) == true) {
    int alarmNumber;
    if (rtc.alarmFired(1)) {
      alarmNumber = 1;
    } else {
      alarmNumber = 2;
    }
    Serial.print("] SQW: ");
    Serial.println(digitalRead(alarmPin));
    Serial.print(" Alarm : ");
    Serial.print(alarmNumber);
    Serial.print(" : ");
    Serial.print(rtc.alarmFired(alarmNumber));
    Serial.println("  >>>La alarma " + String(alarmNumber) + " acaba de dispararse");

    //rtc.disableAlarm(1); // Not used as only disables the alarm on the SQW pin
    rtc.clearAlarm(alarmNumber);
    Serial.println(" - Alarma is cleared");
    Serial.println("");

    DateTime now = rtc.now(); // Get the current time
    char buff[] = "Alarm reiniciada y activada a las hh:mm:ss DDD, DD MMM YYYY";
    Serial.println(now.toString(buff));
    if (alarmNumber == 1) {
      if (!rtc.setAlarm1(DateTime(2023, 6, 25, 6, 35, 30), DS3231_A1_Minute)) {
      //if (!rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 0, 35), DS3231_A1_Second)) {
        Serial.println("Error, alarma 1 no fue configurada correctamente!");
      } else {
        Serial.println("Alarma 1 ocurrirá en 55 segundos!");
      }
    } else {
      if (!rtc.setAlarm2(DateTime(2023, 6, 25, 15, 37, 30), DS3231_A2_Minute)) { // no se puede por segundos solo por minutos
      //if (!rtc.setAlarm2(rtc.now() + TimeSpan(0, 0, 2, 0), DS3231_A2_Minute)) {
        Serial.println("Error, alarma 2 no fue configurada correctamente!");
      } else {
        Serial.println("Alarma 2 ocurrirá en 55 segundos!");
      }
    } //SI alguna de las alarmas se dispara envia 10 ciclos de datos y duerme al salir del condicional
    delay(1000);
    Serial.println("");
    deepSleep();
  }else { // Si no se ha disparado ninguna alarma que vuelva a mimir
      delay(1000);
      Serial.println("");
      deepSleep();
    }

}



void setup() {
  Serial.begin(115200);// open serial for debugging
  delay(10);
  pinMode(alarmPin, INPUT_PULLUP); // en condiciones normales, leerá un valor HIGH/ Cuando la alarma del RTC lo lleve a LOW (FALLING), se activará la interrupción.

  // Realizar un reset del módulo al iniciar
  pinMode(MODEM_PWKEY, OUTPUT); //Encender el ESP32, ver el Pin Diagram del fabricante
  digitalWrite(MODEM_PWKEY, LOW); //Al colocar el código de reset en este punto, nos aseguramos de que el módulo GSM comience en un estado conocido antes de que se realicen otras operaciones.
  delay(100);
  //digitalWrite(MODEM_PWKEY, HIGH); debe ir en LOW

  pinMode(MODEM_RST, OUTPUT); //Encender el ESP32, ver el Pin Diagram del fabricante
  digitalWrite(MODEM_RST, LOW);
  delay(1000);
  digitalWrite(MODEM_RST, HIGH);

  pinMode(MODEM_POWER_ON, OUTPUT); //Encender el ESP32, ver el Pin Diagram del fabricante
  digitalWrite(MODEM_POWER_ON, LOW);
  delay(1000);
  digitalWrite(MODEM_POWER_ON, HIGH);

  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(6000);
  Serial.println("Starting Thinger GSM conection o inicializando moden");
  delay(1000);

  // 3. Inicialización de sensores
  Wire.begin(I2C_SDA, I2C_SCL);
  mySensor.setI2CAddress(BME280_I2C_ADDRESS);
  if (mySensor.beginI2C() == false) { //Begin communication over I2C para BME280
    Serial.println("The sensor did not respond. Please check wiring.");
    while (1); delay(10);
  } else {
    Serial.println("BME Estamos ok");
  }

  //Wire.beginTransmission(DS3231_I2C_ADDRESS);
  if (!rtc.begin()) {
    Serial.print("RTC no encontrado");
    while (1) delay(10);
  } else {
    Serial.println("DS3231 stamos ok");
  }
  if (rtc.lostPower()) { // Si el RTC pierde energía, ajusta el tiempo
    // this will adjust to the date and time at compilation
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.disable32K();//we don't need the 32K Pin, so disable it
  delay(1000);

  Serial.println("Configurado APN y usauario");
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
  /*---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

  // Desactivamos y limpiamos ambas alarmas
  Serial.println("Alarmas reinicidas");
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  Serial.println("Pin SQW en modo de interrupción de alarma"); //Puedes hacer esto utilizando el método rtc.writeSqwPinMode() con diferentes opciones, como DS3231_SquareWave1Hz o DS3231_SquareWave1kHz, para ver esa diferencia de polaridad
  rtc.writeSqwPinMode(DS3231_OFF); // Coloca el pin SQW en modo de interrupción de alarma //
  delay(1000);
  Serial.println("");

  // imprime razon pro la cual se despertó...
  print_porque_desperte(); //https://github.com/CaracolElectronics/Esp32/blob/master/Esp32_ExternalWakeUp/Esp32_ExternalWakeUp.ino
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); //1 = High, 0 = Low // en el momento que se dispare la alarma se depierta. //ext1 sería para varios pines
  Serial.println("");

  DateTime now = rtc.now();
  char buff[] = "Start time is hh:mm:ss DDD, DD MMM YYYY"; //En realidad esto no es necesario
  Serial.println(now.toString(buff));
  Serial.println("");

  Serial.print("Alarma 1 : ");
  if (!rtc.setAlarm1(DateTime(2023, 6, 25, 15, 35, 30), DS3231_A1_Minute)) {
  //if (!rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 0, 35), DS3231_A1_Second)) {// esto es util si quiero que se despierte cada hora por ejemplo.
    Serial.println(" Error, alarm 1 wasn't set!");
  } else {
    Serial.println(" Alarm 1 configurada correctamente!");//no matching function for call to 'RTC_DS3231::setAlarm2(DateTime, Ds3231Alarm1Mode)'
  }

  Serial.print("Alarma 2 : ");
  if (!rtc.setAlarm2(DateTime(2023, 6, 25, 15, 37, 30), DS3231_A2_Minute)) {
  //if (!rtc.setAlarm2(rtc.now() + TimeSpan(0, 0, 2, 0), DS3231_A2_Minute)) {
    Serial.println(" Error, alarm 2 wasn't set!");
  } else {
    Serial.println(" Alarm 2 configurada correctamente!");
  }
  delay(3000);
  Serial.println("");

  timer.setInterval(5000, Sensor);// Puse 5 s para no ver tantos datos
}


void loop() {
  thing.handle();// thinger.io: Please, take into account to do not add any delay inside the loop()
  timer.run();
}

void deepSleep() {
  DateTime now = rtc.now(); // Get the current time
  char buff[] = "Activating deep sleep mode el hh:mm:ss DDD, DD MMM YYYY";
  Serial.println(now.toString(buff));
  Serial.flush();
  //pod[ria poner alguna variable dentro de un bucle que hasta no subir 5 veces la lectura de los sensores que no entre al deep sleep
  for (int i = 3; i >= 0; i--) {
    Serial.print("Going back to sleep in ");
    Serial.println(i);
    delay(1000);
  }
  Serial.println("");
  esp_deep_sleep_start();  // Inicia el sueño profundo
  Serial.println("Esto no debería desplegarse"); // O eso esperamos
}

void print_porque_desperte() { //Para confirmar que se despertó por la interrupción externa
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Me desperte por una senal externa usando RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Me desperte por una senal externa usando RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Me desperte por el timer!"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Me desperte por el touchpad"); break;
  }
}


float getBatteryLevel() { //Prueba de lectura del nivel de bateria usando un pin para no usar AT comando
  sensorValue = analogRead(ADC7); // Leer el valor analógico del pin 35 (ADC1_CH7)
  voltage = sensorValue / 4095.0 * 2.0 * 3.3; // Convertir el valor analógico en voltaje (suponiendo una referencia de 3.3V)
  batteryLevel = ((voltage - 3.0) / 0.7 ) * 100.0; // Calcular el nivel de batería en porcentaje (suponiendo una batería LiPo con voltaje nominal de 3.7V y voltaje mínimo de 3.0V)
  if (batteryLevel > 100) batteryLevel = 100; //Porcentaje=(Vactual−Vmin)/(Vmax−Vmin)×100
  else if (batteryLevel < 5) batteryLevel = 0;
  return batteryLevel;
  //Con una resolución de 12bits, una resolución de 3.3 voltios / 4096 unidades, es equivalente a 0.8 mV por paso.
  //Usamos el 2.0 para mantener el voltaje del ADC en un rango seguro/ teniendo en cuenta un divisor de tensión que reduce a la mitad el voltaje de la Bat
}
