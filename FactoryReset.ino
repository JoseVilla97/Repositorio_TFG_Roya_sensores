/**************************************************************

   To run this tool you need StreamDebugger library:
     https://github.com/vshymanskyy/StreamDebugger
     or from http://librarymanager/all#StreamDebugger

   TinyGSM Getting Started guide:
     https://tiny.cc/tinygsm-readme

 **************************************************************/

// Select your modem:
#define TINY_GSM_MODEM_SIM800

#include <TinyGsmClient.h>
#include <SPI.h>//contiene las funciones necesarias para controlar el hardware integrado de SPI.
#include <Wire.h>//libreria para comunicarse con dispositivos I2C/TWI por vas

// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#ifndef __AVR_ATmega328P__
#define SerialAT Serial1

// or Software Serial on Uno, Nano
#else
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX
#endif

#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);

//#define RXD2 27
//#define TXD2 26
//#define BOOT_PIN 0//boton

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  pinMode(23, OUTPUT); //ENCENDER EL ESP32, ver el Pin Diagram del fabricante
  digitalWrite(23, LOW);
  delay(1000);
  digitalWrite(23, HIGH);
  
  SerialAT.begin(115200, SERIAL_8N1, 26, 27);//los coloco al inversa para que pueda conectar perfectamente
  delay(6000);

  if (!modem.init()) {
    SerialMon.println(F("***********************************************************"));
    SerialMon.println(F(" Cannot initialize modem!"));
    SerialMon.println(F("   Use File -> Examples -> TinyGSM -> tools -> AT_Debug"));
    SerialMon.println(F("   to find correct configuration"));
    SerialMon.println(F("***********************************************************"));
    return;
  }

  bool ret = modem.factoryDefault();

  SerialMon.println(F("***********************************************************"));
  SerialMon.print  (F(" Return settings to Factory Defaults: "));
  SerialMon.println((ret) ? "OK" : "FAIL");
  SerialMon.println(F("***********************************************************"));
}

void loop() {

}
