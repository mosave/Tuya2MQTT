// Tuya module configuration to reflash:
// Board: ESP8266 generic (Chip is ESP8266EX)
// Crystal is 26MHz
// Flash size: 2MB / FS:None
// Erase Flash: ALL content

#include <stdarg.h>
#include "Config.h"
#include "Storage.h"
#include "Comms.h"
#include "MCU.h"

#ifdef I2C_SDA
  #include <Wire.h>
  #include "LightMeter.h"
#endif


static char* TOPIC_State PROGMEM = "State";
static char* TOPIC_SetPosition PROGMEM = "SetPosition";

static char* TOPIC_Command PROGMEM = "Command";
static char* CMD_Open PROGMEM = "Open";
static char* CMD_Close PROGMEM = "Close";
static char* CMD_Stop PROGMEM = "Stop";
static char* CMD_Continue PROGMEM = "Continue";
static char* CMD_Reverse PROGMEM = "Reverse";

#ifdef MCU_DEBUG  
static char* TOPIC_MCUCommand PROGMEM = "MCUCommand";
#endif
static char* S_Opening PROGMEM = "Opening";
static char* S_Closing PROGMEM = "Closing";
static char* S_Moving PROGMEM = "Moving";
static char* S_Idle PROGMEM = "Idle";
static char* S_Failed PROGMEM = "Failed";

#define P3(str) ((char*)(((uint)str)+3))


//*****************************************************************************************
// MQTT support
//*****************************************************************************************
void mqttConnect() {
  mqttSubscribeTopic( TOPIC_SetPosition );
#ifdef MCU_DEBUG  
  mqttSubscribeTopic( TOPIC_MCUCommand );
#endif  
  mqttSubscribeTopic( TOPIC_Command );
}

bool mqttCallback(char* topic, byte* payload, unsigned int length) {
  //printf("mqttCallback(\"%s\", %u, %u )\r\n", topic, payload, length);
  //char s[63];
  //print("Payload=");
  //if( payload != NULL ) { println( (char*)payload ); } else { println( "empty" );}
  
  if( mqttIsTopic( topic, TOPIC_SetPosition ) ) {
    if( (payload != NULL) && (length > 0) && (length<5) ) {
      char b[16];
      memset( b, 0, sizeof(b) );
      strncpy( b, ((char*)payload), length );
      int p = atoi(b);
      if(p<0) p=0; else if (p>100) p=100;
      mcuSetPosition(p);
    }
    return true;
  }
  if( mqttIsTopic( topic, TOPIC_Command ) ) {
    if( (payload != NULL) && (length > 0) && (length<250) ) {
      char cmd[16];
      memset( cmd, 0, sizeof(cmd) );
      strncpy( cmd, ((char*)payload), length );

      if( strcmp( cmd, CMD_Open )==0 ) {
        mcuSetPosition(100);
      } else if( strcmp( cmd, CMD_Close )==0 ) {
        mcuSetPosition(0);
      } else if( strcmp( cmd, CMD_Stop )==0 ) {
        mcuStop();
      } else if( strcmp( cmd, CMD_Continue )==0 ) {
        mcuContinue();
      } else if( strcmp( cmd, CMD_Reverse )==0 ) {
        mcuReverse();
        
        commsClearTopicAndRestart(TOPIC_Command);
        return true;
      }
      mqttPublish( TOPIC_Command, (char *)NULL, false );
    }
    return true;
  }
#ifdef MCU_DEBUG  
  if( mqttIsTopic( topic, TOPIC_MCUCommand ) ) {
    if( (payload != NULL) && (length > 0) && (length<250) ) {
      char b[255];
      memset( b, 0, sizeof(b) );
      strncpy( b, ((char*)payload), length );
      mcuSendMessage(b);
    }
    return true;
  }
#endif  
  return false;
}

void publishState() {
  if( !mqttConnected() ) return;

  unsigned long t=millis();
  char s[127];

  static int _state = -1;
  int state = 0;
  if( mcuIsAlive() ) {
    switch( mcuGetMotorState() ) {
      case Opening: {
        state = 1;
        strcpy( s, S_Opening);
        break;
      }
      case Closing: {
        state = 2;
        strcpy( s, S_Closing);
        break;
      }
      case Moving: {
        state = 3;
        strcpy( s, S_Moving);
        break;
      }
      default: {
        state = 0;
        strcpy( s, S_Idle);
        break;
      }
    }
  } else {
    state = 99;
    strcpy( s, S_Failed);
  }

  if( ( state != _state) && mqttPublish( TOPIC_State, s, true ) ) {
    _state = state;
  }

  static int _pos = -1;
  int pos = mcuGetPosition();
  if( (_pos != pos) && mqttPublish( P3(TOPIC_SetPosition), pos, true ) ) {
    _pos = pos;
  }

}


//*****************************************************************************************
// Setup
//*****************************************************************************************
void setup() {
#ifdef USE_SOFT_SERIAL
  Serial.begin(115200);
  delay(500); 
#endif  
  aePrintln();  aePrintln("Initializing");
  storageInit();
  commsInit();

#ifdef I2C_SDA
  Wire.begin( I2C_SDA, I2C_SCL );
  lightMeterInit(USE_SUNSET_TRACKER);
#endif

  mqttRegisterCallbacks( mqttCallback, mqttConnect );

  mcuInit();
  //commsEnableOTA();
}

void loop() {
  Loop();
  publishState();
  delay(10);
}
