
/*Proyeccto monitoreo de condiciones ambientales
  Jose Villarreal
  Universidad de Costa Rica*/
//chrome-extension://efaidnbmnnnibpcajpcglclefindmkaj/https://riull.ull.es/xmlui/bitstream/915/2928/1/SISTEMA+DE+CONTROL+DE+RIEGOS+GESTIONADOS+TELEMATICAMENTE.pdf

#define THINGER_SERIAL_DEBUG                  //Habilita la depuracion en serie
#define _DEBUG_
#define _DISABLE_TLS_                       //TLS needs to be disabled if using ESP32 (Esto puede ser a algún bug de memoria, y no soporta algunas librerías)

//________________________________Configure TinyGSM library ⤵______________________________

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024         // Set RX buffer to 1Kb//permite que se puedan recibir y almacenar hasta 1024 bytes de datos en el buffer antes de que sea necesario leerlos o procesarlos


//________________________________Librerias para el uso de SIM800 ⤵______________________________

#include <ThingerESP32.h>
#include <TinyGsmClient.h>
#include <ThingerTinyGSM.h>
#include <SparkFunBME280.h>              //Librería del BME280
#include <Wire.h>                        //Libreria para la interfaz de comunicación I2C
#include <RTClib.h>                     // incluye libreria para el manejo del modulo DS3231
#include <Simpletimer.h>                //Libreria para la creación y el manejo de temporizadores y cronómetros simples

//________________________________Librerias para el uso de SD-CARD ⤵______________________________

#include <SPI.h>                        //contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include "FS.h"                         //Permite acceder y manipular archivos en dispositivos de almacenamiento como tarjetas SD, memoria flash SPI, etc
#include "SD.h"                         //Proporciona funciones y métodos para inicializar, leer y escribir en tarjetas SD
#include <ESP32Time.h>
#include "esp_adc_cal.h"
#include "esp_system.h"
#include <PubSubClient.h>

//________________________________Thinger.io necesitamos un usuario,nombre de dispositivo y credencial ⤵______________________________

//Modem Info: SIM7000G R1529
//The current network IP address is:10.113.132.89

//#define USERNAME "JOQ"    //Thinger Account User Name FQO
//#define DEVICE_ID "BME1"    //Thinger device IC
//#define DEVICE_CREDENTIAL "9SN0XcbFqtAVZ0oJ" //Thinger device credential (password)

#define USERNAME "FQO"    //Thinger Account User Name FQO
#define DEVICE_ID "BME_GPRS"    //Thinger device IC
#define DEVICE_CREDENTIAL "9SN0XcbFqtAVZ0oJ" //Thinger device credential (password)

//#define USERNAME "JOQ"    //Thinger Account User Name FQO
//#define DEVICE_ID "BME2"    //Thinger device IC
//#define DEVICE_CREDENTIAL "C?VqtI3BBtqj$EIm" //Thinger device credential (password)



//________________________________Cargamos los datos del APN, en este caso ya desactivamos las contraseñas de la SIM ⤵______________________________
//
//#define APN_NAME "kolbi3g"
//#define APN_USER ""
//#define APN_PSWD ""

// Your GPRS credentials, if any
const char apn[]      = "kolbi3g";
const char gprsUser[] = "";
const char gprsPass[] = "";

//#define APN_NAME "internet.movistar.cr"
//#define APN_USER ""
//#define APN_PSWD ""

//const char APN_NAME[] = "internet.movistar.cr"; // SET TO YOUR APN
//const char APN_USER[] = "";
//const char APN_PSWD[] = "";

//#define TINY_GSM_DEBUG Serial // TinyGSM debug messages to Seria

//// Your GPRS credentials, if any
//const char APN_NAME[] = "kolbi3g"; // SET TO YOUR APN
//const char APN_USER[] = "";
//const char APN_PSWD[] = "";


//________________________________Serial2: Este parámetro indica la interfaz serial que se utilizará para la comunicación con el módulo GSM_________________

#define TINY_GSM_DEBUG Serial // TinyGSM debug messages to Serial

ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);


//// Define how you're planning to connect to the internet
//#define TINY_GSM_USE_GPRS true
//#define TINY_GSM_USE_WIFI false
//#define USE_GSM



#define SerialAT Serial1        // Usar hardware Serial1 para hablar con el modem
#ifdef DUMP_AT_COMMANDS // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
PubSubClient mqtt(client);


//________________________________Definimos los pines segúnn el fabricante__________________________________

//#define MODEM_RST 5
#define PWR_PIN     4 //PIN DE ENCENDIDO DE LA PLCA
#define RXD1 26 //Segun la guia de pines del fabricante estos son los pines TX Y RX/ necesario invertirlos para que funcione
#define TXD1 27 //Los pines RX (recepción) y TX (transmisión) son los pines de comunicación serial que se utilizan para enviar y recibir datos desde y hacia el módulo.

String timeString; //Variable que usaremos para almacenar la fecha actual del RTC y enviar a la SD


//________________________________Definimos los direcciones del DS3231 y BME280 en los pines SDA Y SCL__________________________________

#define DS3231_I2C_ADDRESS 0x68 //Según el escaneo de direcciones para el DS3231 tenemos Addresses 0x57, 0x68
#define BME280_I2C_ADDRESS 0x76 //Según el escaneo para el BME280 tenemos Addresses 0x75, 0x77 la >>0x77<< es la que el escaneo indica como correcta
#define I2C_SDA 21//Definimos los pines SDA (Serial Data) para la transmisión de datos y SCL (Serial Clock) para la comunicación I2C en tu proyecto.
#define I2C_SCL 22



//Creamos objeto del tipo RTC_DS3231
RTC_DS3231 rtc;//Para interactuar con un módulo de reloj en tiempo real (RTC)
BME280 mySensor; //Para interactuar con un sensor de temperatura, humedad y presión atmosférica BME280

//Variables declaradas para el sensor bme280
float humidity;
float humidity_SET;
float temperature;
float temperature_SET;
float DewPoint;
float DewPoint_Ajustado;
float pressure;
float altitud;



//________________________________Definimos los pines para leer el voltaje de la bateria como método de aproximación__________________________________

#define BAT_ADC 35 //Pin de lectura para estimar el voltage 
float voltage;
int vref = 1100;
float vBat;
float vBat_panel;
uint32_t timeStamp = 0;

#define SOLAR_ADC 36         // Pin voltage for the solar panel.
#define LED_PIN 12
#define PIN_DTR 25
//#define GSM_PIN ""

//________________________________Definimos los pines y variables para el sensor capcitivo__________________________________

#define Soil_PIN 39 //Pin de lectura del sensor de humedad del suelo
int soil_value = 0;         //Almacena el valor de lectura del Soil_PIN
float Voltage_Soil_sensor;  //variable para el sensor de humedad del suelo
float Contenido_Vol_real;   // Valor real calculado con la relacion entre lectura de tension y contenido vol de agua en suelo
int Contenido_Vol_real_Porcent; //Valor calculado en porcentaje
int percent_Humididy;        //Valor de la tensión visto como porcentaje

//Variables del sensor de humedad del suelo calibradas
//Estos valores salen de la calibración en laboratorio según las lecturas vistas en las muestras del suelo de la finca
//const float dry = 2.53;     // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V como aproximado
//const float wet = 1.67;     // Voltage value para el suelo seco
const float dry = 3700;     // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V como aproximado
const float wet = 1700;     // Voltage value para el suelo seco


//ELMENTOS PARA QUE EL SISTEMA FUNCIONE
Simpletimer timer1{}; //se declara el objeto timer para evitar detener el sistema
char data;
int readCount = 0;// Definimos un contador de las veces que envia datos al thinger.io y limitar la cantidad de lecturas a enviar antes de dormir
bool enviarDatos = false;//Definimos una variable boolena para enviar o no datos cuando el timer se complete, de este modo enviaremos solo una lectura a la vez


//________________________________Definimos los pines y objetos para comunicarnos con el módulo SD  ⤵______________________________

SPIClass sdSPI(HSPI); //objeto SPIClass llamado sdSPI y lo inicializa con la interfaz de hardware SPI llamada HSPI.
#define MY_MOSI     15  //salida de datos (MOSI) para la comunicación SPI.
#define MY_SCK     14 //reloj de señalización (SCLK) para la comunicación SPI. 
#define MY_CS       13 //inicializa con la interfaz de hardware SPI llamada HSPI.
#define MY_MISO     2 //entrada de datos (MISO) para la comunicación SPI.

String dataMessage;//Variable que almacena


//________________________________Fijamos el tiempo que queremos que se duerma la placa  ⤵______________________________

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1200   /* Time ESP32 will go to sleep  (in seconds) */ //unos 20min

//________________________________Variables asocicadas al watchdog ⤵______________________________

const int button = 33;             //gpio to use to trigger delay //que se usa para simular un bloqueo al presionar un botón
const int wdtTimeout = 90000;     //time in ms to trigger the watchdog// tiempo en el que se quedó pegado por unos 60s debido a que pude tardar el modulo en capturar señal
hw_timer_t *timer = NULL;         //es un puntero a una estructura de tipo hw_timer_t, que se usará para manejar el temporizador de hardware.
long loopTime;

//________________________________Llamada de las funciones ⤵_____________________________

void Datos();
void Sensor_BME280();
void Sensor_humedad();
void print_porque_desperte();
void deepSleep1();
double dewPointC_ajustado();
void initSDCard();
void writeFile();
void appendFile();
void ARDUINO_ISR_ATTR resetModule();
float leerVoltaje();
void modemPowerOn();
void modemPowerOff();


//________________________________Función para encender la placa ⤵______________________________
void modemPowerOn() {
  digitalWrite(PWR_PIN, HIGH);
  delay(1000);
  digitalWrite(PWR_PIN, LOW);
  delay(10000);


}

void modemPowerOff() {
  Serial.println("Shutting down modem for energy saving");
  // Turn off modem by power hardware pin.  The AT shutdown command may restart automatically.
  Serial.println("Shutting down modem for energy saving");
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(2200); // If the pulse is greater than 2s the model will shudown.  See the 7000g manual.
  digitalWrite(PWR_PIN, LOW);
}



//________________________________Inicializar puerto serie ⤵______________________________

void setup() {
  Serial.begin(115200);
  Serial.println("");

  //________________________________Muchas veces la resolucion de los pines DAC puede caer, por lo tanto es mejor tener la herramientas para manipular la salida ⤵______________________________
  pinMode(Soil_PIN, INPUT);
  //analogReadResolution(12);   // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  //analogSetClockDiv(1);      // Set the divider for the ADC clock, default is 1, range is 1 - 255
  //analogSetPinAttenuation(Soil_PIN, ADC_11db); //// Sets the input attenuation, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db

  //________________________________Configuración para WatchDog⤵______________________________
  pinMode(button, INPUT_PULLUP);
  timer = timerBegin(0, 80, true);                  //timer 0, div 80 //Inicializa el temporizador de hardware timer con un divisor de 80 (lo que significa que el temporizador incrementa cada microsegundo).
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback //Adjunta la función resetModule al temporizador como la rutina de servicio de interrupción.
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //Establece la alarma del temporizador para que se active después de wdtTimeout microsegundos (3000 ms).
  timerAlarmEnable(timer);                          //enable interrupt Habilita la alarma del temporizador


  //__________________________________________Inicializamos el módulo y encendemos un módem⤵______________________________
  //  pinMode(PWR_PIN, OUTPUT);
  //  digitalWrite(PWR_PIN, LOW);
  //  modemPowerOn();

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  //________________________________Configuracion de precision ADC ⤵__________________________________________________________

  configADC();

  Serial2.begin(115200, SERIAL_8N1, RXD1, TXD1); //Es necesario invertir los pines para una correcta comunicacion UART
  //SerialAT.begin(115200, SERIAL_8N1, RXD1, TXD1); //Es necesario invertir los pines para una correcta comunicacion UART esto es para GPRS
  Serial.println("/**********************************************************/");
  Serial.println("To initialize the network test, please make sure your GPS");
  Serial.println("antenna has been connected to the GPS port on the board.");
  Serial.println("/**********************************************************/\n\n");

  delay(10000);


  // Args:
  // 2 Automatic
  // 13 GSM only
  // 38 LTE only
  // 51 GSM and LTE only
  // Set network mode to auto
  modem.setNetworkMode(13);
  //  /*
  //    1 CAT-M
  //    2 NB-Iot
  //    3 CAT-M and NB-IoT
  //  * * */
  uint8_t perferred = 3;
  modem.setPreferredMode(perferred);



  //________________________________Inicialización de sensores por medio de las direcciones preestablecidas ⤵______________________________

  Wire.begin(I2C_SDA, I2C_SCL);
  mySensor.setI2CAddress(BME280_I2C_ADDRESS);
  if (mySensor.beginI2C() == false) { //Begin communication over I2C para BME280
    Serial.println("The sensor bme did not respond. Please check wiring.");
    //while (1); delay(10);
  } else {
    Serial.println("Sensor BME280 encontrado, configurado y activado!");;
  }

  //________________________________Inicialización del DS3231 y verificación de encendido ⤵__________________________________________________________

  if (!rtc.begin()) {
    Serial.print("Couldn't find RTC");
    Serial.flush();
  } else {
    Serial.println("DS3231 stamos ok");
  }
  rtc.disable32K();//we don't need the 32K Pin, so disable it
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  //________________________________Configuramos la conexción al GSM  ⤵__________________________________________________________
  Serial.println("Configurado APN y usauario");
  //thing.setAPN(APN_NAME, APN_USER, APN_PSWD);// configurar APN, puede eliminar usuario y contraseña de la llamada si su apn no los requiere
  thing.setAPN(apn, gprsUser, gprsPass);// configurar APN, puede eliminar usuario y contraseña de la llamada si su apn no los requiere

  configADC();

  //________________________________Envio de datos a la plataforma Thinger.io⤵__________________________________________________________

  thing["millis"] >> outputValue(millis());
  thing["Bme"] >> [](pson & out) { //pson es un objeto especial proporcionado por la biblioteca Thinger.h para construir la estructura de datos
    out["Humidity (%)"] = humidity_SET;
    out["Temperature (°C)"] = temperature_SET;
    out["Dewpoint (°C)"] = DewPoint; //envio ambos datos para ver como se comporta el punto de rocio sin ajuste respecto al ajsutado.
    out["Dewpoint Ajustado (°C)"] = DewPoint_Ajustado;
    out["Pressure (Pa)"] = pressure;
    out["Altitude (m)"] = altitud;
    out["Volumetric water content ajustado(%)"] = Contenido_Vol_real_Porcent;
    out["Soil moisture map (%)"] = percent_Humididy;
    out["Battery voltage panel (V)"] = vBat_panel;
    out["Battery voltage2 (V)"] = vBat;
    out["Time hh:mm:ss DDD, DD MMM YYYY"] = timeString;
  };


  //________________________________Ajuste del tiempo para ir a dormir ⤵__________________________________________________________

  print_porque_desperte();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("");

  //________________________________OBTENEMOS LA HORA Y ASIGNAMOS LOS VALORES A UNA VARIABLE PARA ENVIAR A LA SD ⤵__________________________________________________________

  DateTime now = rtc.now();
  char buff[] = "hh:mm:ss, DD MMM YYYY";
  timeString = now.toString(buff);
  Serial.println(timeString);
  Serial.println("");

  //________________________________Inicializamos la SD con la llamada de initSDCard() y en caso de que no exista el archivo hamos uno ⤵__________________________________________________________

  Serial.println("Initializing SD card...");
  initSDCard();
  File file = SD.open("/Datos.csv");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    Serial.println("ENTRO A WRITEFILE");
    writeFile(SD, "/Datos.csv", "fecha, Temperature, Humidity, DewPoint, Dewpoint Ajustado, Contenido volumetrico %, Soil moisture map %, Pressure (Pa), Altitud (m) \r\n");
  }
  else {
    Serial.println("File already exists");
  }
  file.close();
  Serial.println("");


  //________________________________Se necesita una función periódica para evitar las función que relentizan el programa (NUNCA NUNCA PERO NUNCA USAR DELAY) ⤵______________________________

  timer1.register_callback(Datos);
}







//________________________________Ciclo para la lectura y envio de datos a la plataforma ⤵______________________________

void loop() {
  thing.handle();                                 //En cada iteración del bucle principal, se llama a thing.handle() para manejar las comunicaciones con la plataforma Thinger.io.
  if (readCount == 0 ) {
    timer1.run(500);                            // Ejecutar el temporizador con un intervalo de 55 segundos en la primera lectura
  } else {
    timer1.run(1000);                            // Ejecutar el temporizador con un intervalo de 25 segundos en las siguientes lecturas
  }

  if (enviarDatos) {                             //Logica para determinar cuándo se debe transmitir o registrar el recurso.
    //thing.write_bucket("CUBO1", "Bme");
    thing.write_bucket("Station1", "Bme");
    //    thing.write_bucket("CuboTwo", "Bme");
    enviarDatos = false;                          // Resetea la variable para que no se envíen datos continuamente
    Serial.println("");
  }
  if (readCount >= 2) {//Nose porque pero ocupo al     menos 2 corridas para que la plataforma tenga tiempo de subir los datos de la primera corrida
  // Turn off modem by power hardware pin.  The AT shutdown command may restart automatically.
    modemPowerOff();
    deepSleep1();  // Llama a deepSleep después de un cierto número de ciclos de la función void sensor
  }

  //________________________________Estructura del WatchDog ⤵______________________________
  timerWrite(timer, 0);                             //reset timer (feed watchdog) //Reinicia el temporizador (alimentando el watchdog) usando timerWrite(timer, 0).
  loopTime = millis();                         //Calcula el tiempo de ejecución del ciclo usando millis()
  while (!digitalRead(button)) {                    //Si el botón está presionado (!digitalRead(button)), entra en un bucle que imprime "button pressed"
    Serial.println("button pressed");
    //delay(500); // esto para probar un delay que trabe el programa
  }
  loopTime = millis() - loopTime;                   //Calcula el tiempo total del ciclo restando el tiempo actual (millis()) del tiempo inicial y lo imprime.
}


//________________________________Lectura de los datos ⤵__________________________________________________________________________________

void Datos() { // Función que se ejecuta periodicamente con el timer1

  Serial.print("loop time is = ");
  Serial.println(loopTime);

  //__________________________________________Llamada a las funciones que leen los sensores ⤵______________________________

  Sensor_BME280();    // Llamada sensor BME280
  Sensor_humedad();  // Llamada a la función sensor capcaitivo

  //__________________________________________Lectura del voltage de bateria por dos métodos ⤵______________________________

  vBat = leerVoltaje(BAT_ADC, 0); //Método 1
  vBat_panel = leerVoltaje(SOLAR_ADC, 1);
  Serial.println("");

  //__________________________________________Registramos los datos en la SD card ⤵______________________________

  Serial.println("Asignando valores a la variable dataMessage para ser almacenada");
  dataMessage = timeString + ", " + String(temperature_SET) + ", " + String(humidity_SET) + ", " + String(DewPoint) + ", " + String(DewPoint_Ajustado) + ", " + String(Contenido_Vol_real_Porcent) + ", " + String(percent_Humididy) + ", " + String(pressure) + ", " + String(altitud) + "\r\n";
  Serial.print("Saving data: ");
  Serial.println(dataMessage);   //Llama la función appendFile, pasando tres argumentos: SD como objeto de la sd, "/data.txt": Este es el nombre del archivo y dataMessage.c_str():
  appendFile(SD, "/Datos.csv", dataMessage.c_str());//Esto convierte el contenido de la variable dataMessage a un puntero de caracteres (char*)
  Serial.println("");

  readCount++;
  Serial.print("Corrida numero: ");
  Serial.println(readCount);
  Serial.println("");
  if (readCount == 1) { //queremos enviar la segunda corrida no la primera
    enviarDatos = true;
  }
}





//________________________________Lecturas del sensor barometrico ⤵______________________________

void Sensor_BME280() {
  //1. LECTURAS DEL SENSOR BME280
  Serial.println("");
  Serial.print("Humidity: ");
  humidity = mySensor.readFloatHumidity();
  Serial.print(humidity, 0);
  Serial.println(" %");
  Serial.print("Humidity SET: ");
  humidity_SET = (0.94 * humidity) + 7.59; //0.8892*humidity+11.267 //ecuacion de calibración mediante regresión
  Serial.print(humidity_SET, 0);
  Serial.println(" %");

  Serial.print("Temp: "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature = mySensor.readTempC();
  Serial.print(temperature, 2);
  Serial.println(" °C");
  Serial.print("Temp SET: "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature_SET = (0.88 * temperature) + 3.52;
  Serial.print(temperature_SET, 2);
  Serial.println(" °C");

  Serial.print("Dewpoint Programa: ");
  DewPoint = mySensor.dewPointC();
  Serial.print(DewPoint, 2);
  Serial.println("°C");

  Serial.print("Dewpoint Ajustado: ");
  DewPoint_Ajustado = dewPointC_ajustado();
  Serial.print(DewPoint_Ajustado, 2);
  Serial.println(" °C");

  Serial.print("Pressure: ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  pressure = mySensor.readFloatPressure();
  Serial.print(pressure, 1);
  Serial.println(" (Pa)");

  Serial.print("Alt: ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  altitud = mySensor.readFloatAltitudeMeters();
  Serial.print(altitud, 1);
  Serial.println(" (m)");
  Serial.println("");
}





//__________________________________________Lectura de los datos del sensor de humedad ⤵______________________________

void Sensor_humedad() {
  soil_value  = analogRead(Soil_PIN);//recibe valores de entrada análoga, en este caso del sensor en el pin 25
  Serial.print("Soil Moisture bits: ");
  Serial.println(soil_value);

  Serial.print("Soil Moisture Sensor Voltage: "); // Por medio de una simple regla de 3
  Voltage_Soil_sensor = (soil_value / 4095.0) * 3.3; //Calculo el voltaje basado en la lectura del ADC del ESP32, teniendo en cuenta que trabaja a 12 bits por defecto.
  Serial.print(Voltage_Soil_sensor);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095
  Serial.println(" V");// lo ocupo en voltage

  //Formulas es θv=(a/V)+b //lectura de tensión tomada de nuestro sensor electromagnético puede relacionarse con el contenido volumétrico de agua mediante un ajuste lineal
  Contenido_Vol_real = (1.25 * (1 / Voltage_Soil_sensor)) - 0.25; //Calculo de contenido volumètrico de agua
  Serial.print("Contenido volumetrico de agua en suelo: ");
  Serial.print(Contenido_Vol_real);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095
  Serial.println(" cm^3/cm^3");// proporción de volumen de agua con respecto al volumen total del suelo


  Contenido_Vol_real_Porcent = Contenido_Vol_real * 100;
  if (Contenido_Vol_real_Porcent >= 100) {
    Contenido_Vol_real_Porcent = 100;
  }
  else if (Contenido_Vol_real_Porcent <= 0) {
    Contenido_Vol_real_Porcent = 0;
  } else {
    Serial.print("Contenido volumetrico %: ");
    Serial.print(Contenido_Vol_real_Porcent);
    Serial.println("%");
  }

  //__________________________________________Lectura de los datos del sensor capacitivo de humedad suelo por rango de bits ⤵______________________________

  percent_Humididy = map(soil_value, dry, wet, 0, 100);; //Mapeo del valor del sensor a un porcentaje de humedad entre 0 y 100, donde dry es 0 y wet es 100.
  if (percent_Humididy >= 100) {
    percent_Humididy = 100;
  }
  else if (percent_Humididy <= 0) {
    percent_Humididy = 0;
  }
  Serial.print("Soil moisture map %: ");
  Serial.print(percent_Humididy);
  Serial.println(" %");
  Serial.println("");

}


//__________________________________________Función para manejar las interrupciones, uso del WatchDog timer ⤵______________________________

void ARDUINO_ISR_ATTR resetModule() {// Manejo de la interrupción
  Serial.println("");
  ets_printf("reboot\n"); //Cuando se llama, imprime "reboot" en el puerto serie y reinicia el ESP32 usando esp_restart().
  Serial.println("");
  esp_restart();
}




//________________________________Confirmamos porque despertó ⤵______________________________

void print_porque_desperte() {
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

//________________________________Intentamos ajsutar los pines de energia para un mejor lectura ⤵______________________________

void configADC() {

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars); // Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    Serial.print("eFuse Vref: ");
    Serial.print(adc_chars.vref);
    Serial.println("mV");
    vref = adc_chars.vref;
  }
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
  {
    Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    Serial.print("Two Point --> coeff_a: ");
    Serial.print(adc_chars.coeff_a);
    Serial.print("mV coeff_b: ");
    Serial.print(adc_chars.coeff_b);
    Serial.println("mV");
  }
  else
  {
    Serial.println("Default Vref: 1100mV");
  }
}

//________________________________Método 1 par ala lectura del voltage de batería⤵______________________________

float leerVoltaje(int vPin, int tipo) {
  uint16_t v = analogRead(vPin);
  // float voltPin = v;
  float pin_voltage;
  float sensorVoltMap;
  String voltage;

  // tipos:0="Bat" 1="Panel" 2="Sensor"
  switch (tipo)
  {
    case 0:
      pin_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
      voltage = "Voltaje Bat: " + String(pin_voltage) + " V\n";

      Serial.println(voltage);

      return pin_voltage;
      break;
    case 1:
      pin_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
      voltage = "Voltaje Panel: " + String(pin_voltage) + " V\n";

      Serial.println(voltage);

      return pin_voltage;
      break;
    //  case 2:                                           // se invierte y mapea la lectura del sensor de 0 a 1000
    //    pin_voltage = ((float)v / 4095.0) * 3.3 * vref; // vref no se divide entre 1000 para poder usar map()
    //    voltage = "Voltaje Sensor: " + String(pin_voltage) + "V\n";
    //    sensorVoltMap = map(pin_voltage, 0.0, 3300, 1000.0, 0.0);
    //    Serial.println(sensorVoltMap); // muestra el valor mapeado
    //
    //    return sensorVoltMap;
    //    break;

    default:
      return 99; // numero para default
      break;
  }
}


double dewPointC_ajustado() { //Calculamos un ajuste del dewpoint con los nuevos valores de temperatura y humedad relativa
  double RATIO = 373.15 / (273.15 + temperature_SET);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);
  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity_SET;
  // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP / 0.61078); // temp var
  return (241.88 * T) / (17.558 - T);
  //return(dewPointC() * 1.8 + 32); //Convert C to F
}




void initSDCard() { // Initialize SD card
  sdSPI.begin(MY_SCK, MY_MISO, MY_MOSI, MY_CS);
  if (!SD.begin(MY_CS, sdSPI)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}




void writeFile(fs::FS &fs, const char * path, const char * message) {// Write to the SD card
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    Serial.println("estamos fallando al abrir el file");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}



// Append data to the SD card
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

//________________________________Función enviar a dormir la placa y ahorrar energía ⤵______________________________

void deepSleep1() {

  for (int i = 3; i >= 0; i--) {
    Serial.print("Going back to sleep in ");
    Serial.println(i);
  }
  readCount = 0;
  Serial.println("");
  esp_deep_sleep_start();  // Inicia el sueño profundo
  Serial.println("Esto no debería desplegarse"); // O eso esperamos
}
