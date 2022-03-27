#ifndef config_h
#define config_h
#include <Arduino.h>

#define VERSION "1.0"


// Device host name and MQTT ClientId, "%s" is to be replaced with device' MAC address
// Disables SetName MQTT command
#define WIFI_HostName "Curtain_%s"

// Define this to enable SendCommand topic and extra debug output
//#define MCU_DEBUG

//#define WIFI_SSID "SSID"
//#define WIFI_Password  "Password"
//#define MQTT_Address "MQTT Broker Address"
//#define MQTT_Port 1883

#ifndef WIFI_SSID
#include "Config.AE.h"
#endif

// Override default MCU motor direction 
// Define this if Open command erroneously move curtains to position 0 and Close to 100 
// Possible values are:
// 0 Open/Close behave natively
// 1 Open/Close switched
// Not defined: Autosense motor direction
//#define MCU_MOTOR_INVERTED 1


// Define to use BH1750 light meter
#define I2C_SDA 4
#define I2C_SCL 5

// SDA/SCL PIO for ESP-1S controller:
#define I2C_1S_SDA 0
#define I2C_1S_SCL 2

// Define this to autosynchronize time if NTP server is available.
// Check "tz.h" for timezone constants
#define TIMEZONE TZ_Europe_Moscow

// If light meter should be used to track sunset
#define USE_SUNSET_TRACKER true

//#define USE_SOFT_SERIAL

#ifdef USE_SOFT_SERIAL
  #define aePrintf( ... ) Serial.printf( __VA_ARGS__ )
  #define aePrint( ... ) Serial.print( __VA_ARGS__ )
  #define aePrintln( ... ) Serial.println( __VA_ARGS__ )

  #define MCU_RX 13 // D7
  #define MCU_TX 12 // D6
#else
  #define aePrintf( ... )
  #define aePrint( ... )
  #define aePrintln( ... )
#endif

#define LOOP std::function<void()>

void registerLoop( LOOP loop );
void Loop();

#endif
