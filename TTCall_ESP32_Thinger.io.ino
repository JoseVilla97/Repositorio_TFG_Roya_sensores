/*// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22*/

/*Proyeccto monitoreo de condiciones ambientales
  Jose Villarreal
  Universidad de Costa Rica*/

#define THINGER_SERIAL_DEBUG //Habilita la depuracion en serie
#define _DEBUG_
#define _DISABLE_TLS_  //TLS needs to be disabled if using ESP32 (Esto puede ser a algún bug de memoria, y no soporta algunas librerías)
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb//permite que se puedan recibir y almacenar hasta 1024 bytes de datos en el buffer antes de que sea necesario leerlos o procesarlos

#include <ThingerESP32.h> //Configuracion del ESP con la biblioteca de thinger
#include <TinyGsmClient.h>
#include <ThingerTinyGSM.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <SparkFunBME280.h>//Librería del BME280
#include <Wire.h>
#include <RTClib.h>  // incluye libreria para el manejo del modulo DS3231
#include <Simpletimer.h>

//#define USERNAME "JOQ"    //Thinger Account User Name FQO
//#define DEVICE_ID "BME1"    //Thinger device IC
//#define DEVICE_CREDENTIAL "9SN0XcbFqtAVZ0oJ" //Thinger device credential (password)

#define USERNAME "FQO"    //Thinger Account User Name FQO
#define DEVICE_ID "BME1"    //Thinger device IC
#define DEVICE_CREDENTIAL "9SN0XcbFqtAVZ0oJ" //Thinger device credential (password)

#define APN_NAME "kolbi3g" //Cargamos los datos del APN
#define APN_USER ""
#define APN_PSWD ""

ThingerTinyGSM thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL, Serial2);

#define MODEM_RST            5 // VER DIAGRAMA DE FABRICANTE
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23 //PIN DE ENCENDIDO DE LA PLCA

#define RXD1 27 //Segun la guia de pines del fabricante estos son los pines TX Y RX/ necesario invertirlos para que funcione
#define TXD1 26//Ademas son los pines dedicados a la SIM800
#define ADC7 35 //Pin de lectura de voltaje
#define Soil_PIN 33 //Pin de lectura del sensor de humedad del suelo
const int alarmPin = 15;// Pin por meddio del cual se emiten las einterrupciones del DS3231

//Es posible que se presenten problemas con las direcciones en el BUS I2C pero podemos usar el programa ESCANERRTC que está en el reporsitorio de GIT
#define DS3231_I2C_ADDRESS 0x68 //Según el escaneo de direcciones para el DS3231 tenemos Addresses 0x57, 0x68
//#define DS3231_ALARM1 0x07    ///< Alarm 1 register
//#define DS3231_ALARM2 0x0B    ///< Alarm 2 register
#define BME280_I2C_ADDRESS 0x77 //Según el escaneo para el BME280 tenemos Addresses 0x75, 0x77 la >>0x77<< es la que el escaneo indica como correcta
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
//ESTOS VALORES SALEN DE LA CALIBRACIÓN EN LABORATORIO SEGÚN LAS LECTURAS VISTAS EN LAS MUESTRAS DEL SUELOO DE LA FINCA
const float dry = 2.96; // Voltage value for dry soil deberia ser entre 0 y 3.3 V / en el aire deveria ser 3.15 y si esta en suelo 1.9-3V
const float wet = 1.8; // Voltage value para el suelo seco

float Voltage2; //variable para el sensor de humedad del suelo
float soil_value ;//almacena el valor de lectura del Soil_PIN
int percent_Humididy;
float Contenido_Vol_real; // valor real calculado con la relacion entre lectura de tension y contenido vol de agua en suelo
float pendiente = 2.48; // Pendiente obtenida de la regresion
float intercepcion = -0.72; // intercepcion obtenida de la regresion



//ELMENTOS PARA QUE EL SISTEMA FUNCIONE
Simpletimer timer1{}; //se declara el objeto timer para evitar detener el sistema
char data;
int readCount = 0;// conteo de las veces que envia datos al thinger.io
void Sensor();//FUNCIÓN PARA LA LECTURA DE LOS SENSORES
void deepSleep(); //FUNCIÓN PARA MANDAR A DOMIR LA PLACA
void print_porque_desperte(); //FUNCIÓN QUE IMPRIME AL RAZÓN POR LA QUE SE DESPIERTA, QUE DEBE SER POR EMDIO DE UNA INTERRUPCIÓN EXTERNA DEL DS3231
bool enviarDatos = false;

void setup() {
  
  //1. Inicializar el puerto serie
  Serial.begin(115200);// open serial for debugging
  delay(10);
  pinMode(alarmPin, INPUT_PULLUP); // en condiciones normales, leerá un valor HIGH/ Cuando la alarma del RTC lo lleve a LOW (FALLING), se activará la interrupción.
  pinMode(Soil_PIN, INPUT);

  //2. Realizar un reset del módulo al iniciar
  pinMode(MODEM_PWKEY, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW); 
  pinMode(MODEM_RST, OUTPUT); 
  digitalWrite(MODEM_RST, HIGH);

  pinMode(MODEM_POWER_ON, OUTPUT); //Encender la placa
  digitalWrite(MODEM_POWER_ON, LOW);
  delay(1000);
  digitalWrite(MODEM_POWER_ON, HIGH);

  Serial2.begin(115200, SERIAL_8N1, TXD1, RXD1);//Es necesario invertir los pines para una correcta comunicacion UART
  delay(6000);
  Serial.println("Starting Thinger GSM conection o inicializando moden");
  delay(1000);

  // 3. Inicialización de sensores por medio de las direcciones
  Wire.begin(I2C_SDA, I2C_SCL);
  mySensor.setI2CAddress(BME280_I2C_ADDRESS);
  if (mySensor.beginI2C() == false) { //Begin communication over I2C para BME280
    Serial.println("The sensor did not respond. Please check wiring.");
    while (1); delay(10);
  } else {
    Serial.println("BME Estamos ok");
  }

  //Wire.beginTransmission(DS3231_I2C_ADDRESS);// no hace falta porque ya detecta el canal
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

  // 4. Configuramos la APN y enviamos los datos a la platafora de thinger.io
  Serial.println("Configurado APN y usauario");
  thing.setAPN(APN_NAME, APN_USER, APN_PSWD);// configurar APN, puede eliminar usuario y contraseña de la llamada si su apn no los requiere

  //thing["millis"] >> outputValue(millis());
  thing["Bme"] >> [](pson & out) { //pson es un objeto especial proporcionado por la biblioteca Thinger.h para construir la estructura de datos
    out["Humidity (%)"] = round_to_dp(humidity_SET, 2);
    out["Temperature (°C)"] = round_to_dp(temperature_SET, 2);
    out["Dewpoint"] = DewPoint;
    out["Pressure (Pa)"] = pressure;
    out["Altitude (m)"] = altitud;
    out["Soil moisture (%)"] = percent_Humididy;
    out["Battery level (%)"] = batteryLevel;
    out["Device_1"] = String(DEVICE_ID);
  };

  // 5. Desactivamos y limpiamos ambas alarmas del ds3231
  Serial.println("Alarmas reinicidas");
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  Serial.println("Pin SQW en modo de interrupción de alarma"); //Puedes hacer esto utilizando el método rtc.writeSqwPinMode() con diferentes opciones, como DS3231_SquareWave1Hz o DS3231_SquareWave1kHz, para ver esa diferencia de polaridad
  rtc.writeSqwPinMode(DS3231_OFF); // Coloca el pin SQW en modo de interrupción de alarma //
  delay(1000);
  Serial.println("");

  // 6. Imprimimos la razon pro la cual se despertó...
  print_porque_desperte();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); //1 = High, 0 = Low // en el momento que se dispare la alarma se depierta. //ext1 sería para varios pines
  Serial.println("");

  // 7. Imprimirmos algunos datos para verificar cuando nos levantamos y ajustamos las alarmas
  DateTime now = rtc.now();
  char buff[] = "Start time is hh:mm:ss DDD, DD MMM YYYY";
  Serial.println(now.toString(buff));
  Serial.println("");

  Serial.print("Alarma 1 : ");
  //if (!rtc.setAlarm1(DateTime(2024, 6, 25, 15, 55, 30), DS3231_A1_Minute)) { //DS3231_A1_Hour
  if (!rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 5, 20), DS3231_A1_Minute)) {// esto es util si quiero que se despierte cada hora por ejemplo.
    Serial.println(" Error, alarm 1 wasn't set!");
  } else {
    Serial.println(" Alarm 1 configurada correctamente!");//no matching function for call to 'RTC_DS3231::setAlarm2(DateTime, Ds3231Alarm1Mode)'
  }

  Serial.print("Alarma 2 : ");
  if (!rtc.setAlarm2(DateTime(2024, 6, 25, 11, 57, 30), DS3231_A2_Minute)) { //DS3231_A2_Hour
    //if (!rtc.setAlarm2(rtc.now() + TimeSpan(0, 0, 2, 0), DS3231_A2_Minute)) {
    Serial.println(" Error, alarm 2 wasn't set!");
  } else {
    Serial.println(" Alarm 2 configurada correctamente!");
  }
  delay(3000);
  Serial.println("");

  //8. Llamamos a la funcion que se ejecuta periodicamente
  timer1.register_callback(Sensor);
}


void loop() {
  thing.handle();//Maneja la conexión con Thinger.io.
  timer1.run(50000); //el tiempo en el que va a hacer cada lectura, si es mayor a 60s, refleja un 0 en el cubo de datos
    // Verifica si es el momento de enviar datos
  if (enviarDatos) {
    thing.stream("Bme"); // Utiliza tu lógica para determinar cuándo enviar los datos
    //thing.write_bucket("Station1", "Bme");
    enviarDatos = false; // Resetea la variable para que no se envíen datos continuamente
  }
  
  if (readCount > 3) { //sabemos que 12*5 =60s, timpo para subir a la plataforma
    deepSleep();  // Llama a deepSleep después de un cierto número de ciclos de la función void sensor
  }

}

void Sensor() { // Función que se ejecuta periodicamente
  Serial.println("");
  readCount++;
  Serial.print("Corrida numero: ");
  Serial.println(readCount);
  
  Serial.print("Humidity: ");
  humidity = mySensor.readFloatHumidity();
  Serial.print(humidity, 0);
  Serial.println(" %");
  Serial.print("Humidity SET: ");
  humidity_SET = (0.94 * humidity) + 7.59; //0.8892*humidity+11.267 //ecuacion de calibración mediante regresión
  Serial.print(humidity_SET, 0);
  Serial.println(" %");


  Serial.print("Pressure : ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  pressure = mySensor.readFloatPressure();
  Serial.print(pressure, 1);
  Serial.println(" (Pa)");

  Serial.print("Alt : ");//Para el resto no hace falta la calibración pues el RMSE es muy bajo
  altitud = mySensor.readFloatAltitudeMeters();
  Serial.print(altitud, 1);
  Serial.println(" (m)");

  Serial.print("Temp : "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature = mySensor.readTempC();
  Serial.print(temperature, 2);
  Serial.println(" °C");
  Serial.print("Temp SET: "); //23.281*log(x)-49.289 //ecuacion de calibración mediante regresión
  temperature_SET = (0.88 * temperature) + 3.52; //Nose si log solo acepta double
  Serial.print(temperature_SET, 2);
  Serial.println(" °C");

  Serial.print("Dewpoint Programa: ");
  DewPoint = mySensor.dewPointC();
  Serial.print(DewPoint, 2);
  Serial.println("°C");

  soil_value = analogRead(Soil_PIN);
  Serial.print("Soil Moisture Voltage (Tensión): "); // Por medio de una simple regla de 3
  Voltage2 = soil_value /4095*3.3; //Calculo el voltaje basado en la lectura del ADC del ESP32, teniendo en cuenta que trabaja a 12 bits por defecto. 
  Serial.print(Voltage2);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095
  Serial.println(" V");// lo ocupo en voltage

  //Formulas es θv=(a/V)+b //lectura de tensión tomada de nuestro sensor electromagnético puede relacionarse con el contenido volumétrico de agua mediante un ajuste lineal
  Contenido_Vol_real = (pendiente/ Voltage2) + intercepcion; //Calculo de contenido volumètrico de agua corregido por regresión para m (endiente) y b (intercepción)
  Serial.print("Contenido volumetrico de agua en suelo: ");
  Serial.print(Contenido_Vol_real);  //bits leidos del sensor/los 12 bits *3.3Vol //12bits con tecnologpia ttl sería 0-4095
  Serial.println(" cm^3/cm^3");// proporción de volumen de agua con respecto al volumen total del suelo
  //1 cm³/cm³ (o 1 m³/m³) = 100% de humedad: Significa que todo el volumen del suelo está lleno de agua
  //0 cm³/cm³ = 0% de humedad: Significa que no hay agua en el volumen del suelo. El suelo está completamente seco.

  //En esta secciòn puedo calcular el porcentaje de humedad segun 
  Serial.print("Soil Moisture bits: "); 
  Serial.println(soil_value); 

  //LECTURA DE LOS VALORES DEL SENSOR DE HUMEDAD EN PORCENNTAJES
  percent_Humididy = map(Voltage2, dry, wet, 0, 100);//esto cambia según la calibración mX + b
  if (percent_Humididy >= 100) {
    percent_Humididy = 100;
  }
  else if (percent_Humididy <= 0) {
    percent_Humididy = 0;
  }
  Serial.print("Soil moisture: ");
  Serial.print(percent_Humididy);
  Serial.println(" %");

  //LECTURA DE LOS VALORES DE LA BATERIA EN UNO DE SUS PINES
  Serial.print("Battery level: ");
  batteryLevel = getBatteryLevel();
  Serial.print(batteryLevel, 1);
  Serial.println("%");

}

float round_to_dp( float in_value, int decimal_place ) {
  float multiplier = powf( 10.0f, decimal_place );
  in_value = roundf( in_value * multiplier ) / multiplier;
  return in_value;
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
  readCount = 0;
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
