#include <Arduino.h>
#include "Config.h"
#include "MCU.h"
#include "Comms.h"

enum MCUPhase {
  HeartBeat,
  DeviceInfo,
  WorkingMode,
  WorkingStatus,
  Normal
};

bool mcuReversed = false;
MotorState mcuMotorState = Idle;
MotorState mcuDirection = Closing;
int mcuPosition = false;
char mcuData[128];
int mcuLen = 0;
bool mcuTriggeredByHand = true;
int mcuCommandSet = 0;

unsigned long mcuLastRead = 0;
unsigned long mcuHeartBeat = 0;
unsigned long mcuLastAlive = 0;
MCUPhase mcuPhase = (MCUPhase)0;

#ifdef USE_SOFT_SERIAL
	#include <SoftwareSerial.h>
	SoftwareSerial mcu(MCU_RX, MCU_TX);
#else
	#define mcu Serial
#endif


void mcuLogData( const char* prefix ) {
#ifdef MCU_DEBUG
  if( mcuLen==0 ) return;
  char s[255];
  char hex[4];
  strcpy( s, "< ");
  if( (prefix != NULL) && (strlen(prefix)>0) ) strcat(strcat( s, prefix)," ");

  for(int i=0; i<mcuLen; i++ ) {
    sprintf( hex, "%02x", mcuData[i]);
    if( i == mcuLen-1 ) strcat(s, ":");
    strcat( s, hex);
  }
  aePrintln(s);
  mqttPublish("Log",s,false);
#endif
}


#pragma region Message Sending
void mcuSendMessage( const char* data) {
  char hex[3] = {0,0,0};
  char* p = (char*)data;
  uint8_t sum = 0x00;
  while ( *p>'\0' ) {
    hex[0] = *p; p++;
    hex[1] = *p; p++;
    uint8_t d = strtoul( hex, NULL, 16);
    mcu.write(d);
    sum += d;
  }
  mcu.write(sum);

#ifdef MCU_DEBUG
  char s[256];
  sprintf(s, "> %s:%02x", data,sum);
  mqttPublish("Log",s,false);
  aePrintln(s);
#endif
}

void mcuSendMessage( const __FlashStringHelper* data) {
  PGM_P p = reinterpret_cast<PGM_P>(data);
  char s[255] __attribute__ ((aligned(4)));
  size_t n = std::min(sizeof(s)-1, strlen_P(p) );
  memcpy_P(s, p, n);
  s[n] = 0;
  mcuSendMessage( s );
}
#pragma endregion

#pragma region Curtains commands
bool mcuIsAlive() {
  return ( (unsigned long)(millis() - mcuLastAlive) < (unsigned long)90000 );
}

int mcuGetPosition() {
  return mcuPosition;
}

MotorState mcuGetMotorState() {
  return mcuMotorState;
}

void mcuReverse() {
  if( mcuReversed ) { // Disable reversing
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056700000100") );
    } else {
      mcuSendMessage( F("55aa000600050500000100") );
    }
  } else { // Enable reversing
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056700000101") );
    } else {
      mcuSendMessage( F("55aa000600050500000101") );
    }
  }
}

void mcuStop() {
  mcuTriggeredByHand = false;
  if( mcuCommandSet==0 ) {
    mcuSendMessage( F("55aa000600056504000101") );
  } else {
    mcuSendMessage( F("55aa000600050104000101") );
  }
}

void mcuSetPosition( int position ) {
  mcuTriggeredByHand = false;
  if( position+3 < mcuPosition ) {
    mcuMotorState = Opening;
    mcuDirection = Opening;
  } else if( position-3 > mcuPosition ) {
    mcuMotorState = Closing;  
    mcuDirection = Closing;
  }

  if( position <= 1 ) {
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056504000100") );
    } else {
      mcuSendMessage( F("55aa000600050104000100") );
    }
  } else if( position >= 99 ) {
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056504000102") );
    } else {
      mcuSendMessage( F("55aa000600050104000102") );
    }
  } else {
    char cmd[31];
    if( mcuCommandSet==0 ) {
      sprintf( cmd, (const char*)F("55aa0006000868020004000000%02x"), position );
    } else {
      sprintf( cmd, (const char*)F("55aa0006000802020004000000%02x"), position );
    }
    mcuSendMessage( cmd );
  }
}

void mcuContinue() {
  if( (mcuDirection == Opening) && (mcuPosition>97) ) {
    mcuDirection = Closing;
  } else if( (mcuDirection == Closing) && (mcuPosition<3) ) {
    mcuDirection = Opening;
  }
  mcuSetPosition( (mcuDirection == Opening) ? 100 : 0 );
}

#pragma endregion
void mcuProcessData() {
  //int size = mcuData[5];
  switch (mcuData[3]) { // Command
    case 0x00: { // Heartbeat answer
      break;
    }
    case 0x01: { // Device model & soft version
      break;
    }
    case 0x02: { // Module working mode
      break;
    }
    case 0x07: {
      switch (mcuData[6]) { // <=== DP ID
        case 0x67: 
        case 0x05: { // Motor reversing state
          mcuCommandSet = ( mcuData[6]>0x10 ) ? 0 : 1;
          mcuReversed = !!mcuData[10];
          break;
        }
        case 0x65: 
        case 0x01: { // Operation mode state
          mcuCommandSet = ( mcuData[6]>0x10 ) ? 0 : 1;
          switch ( mcuData[10]) {
            case 0x00:
              mcuMotorState = Closing;
              mcuDirection = Closing;
              break;
            case 0x01:
              mcuMotorState = Idle;
              break;
            case 0x02:
              mcuMotorState = Opening;
              mcuDirection = Opening;
              break;
          }
          break;
        }
        // Position reports during operation
        case 0x66: 
        case 0x07:  {
          mcuCommandSet = ( mcuData[6]>0x10 ) ? 0 : 1;
          if( (mcuCommandSet) && (mcuLen>10) ) {
            if( mcuData[10] == 1) {
              mcuMotorState = Opening;
              mcuDirection = Opening;
            } else {
              mcuMotorState = Closing;
              mcuDirection = Closing;
            }
          } else if( mcuMotorState == Idle ) {
            mcuMotorState = Moving;
          }
          if( mcuLen>13 ) {
            mcuPosition = mcuData[13];
            if (mcuPosition >= 95) mcuPosition = 100;
            if (mcuPosition <= 5) mcuPosition = 0;
          }
          if( mcuTriggeredByHand ) {
            triggerActivity();
            aePrintln(F("Triggered by hand"));
          }
          break;
        }
        // Position reports after operation
        case 0x68:
        case 0x03: {
          mcuCommandSet = ( mcuData[6]>0x10 ) ? 0 : 1;
          mcuMotorState = MotorState::Idle;
          if( mcuLen>13 ) {
            mcuPosition = mcuData[13];
            if (mcuPosition >= 95) mcuPosition = 100;
            if (mcuPosition <= 5) mcuPosition = 0;
          }
          mcuTriggeredByHand = true;
          break;
        }
      }
      break;
    }
  }
}

void mcuLoop() {
	unsigned long t = millis();

  while (mcu.available()>0) {
    // Packet read timeout
    if( (unsigned long)(t - mcuLastRead) > (unsigned long)500 ) {
      if( mcuLen>1 ) mcuLogData("Timed Out");
      mcuLen = 0;
    }
    // Packet is too long
    if( mcuLen >= sizeof(mcuData)-1 ) {
      mcuLogData("Too long");
      mcuLen=0;
    }

    mcuLastRead = t;
    int d = mcu.read();
    //mcuData[mcuLen++] = mcu.read(); 
    mcuData[mcuLen++] = d; 
    // Invalid packet header (0x55AA)
    if( (mcuData[0]!=0x55) || (mcuLen>1) && (mcuData[1]!=0xAA) ) {
      mcuLogData("Invalid header");
      mcuLen = 0;
    } else {
      // Check if packet received correctly.
      // 1. Packet length is received already:
      if( mcuLen > 6) {
        int len = mcuData[4] << 8 | mcuData[5];
        // 2. All packet data received
        if( mcuLen >= 6 + len + 1 ) {
          // 3. Calculate CRC
          uint8_t sum = 0;
          for (int i = 0; i < 6 + len; i++) sum += mcuData[i];
          // 4. CRC Check
          if( sum == mcuData[ 6 + len ]) {
            // 5. Process buffer...
            mcuLastAlive = t;
            mcuHeartBeat = t;
            mcuLogData("");
            mcuProcessData();
          } else {
            mcuLogData("Bad CRC");
          }
          mcuLen = 0;
        }
      }
    }
  }

  if( (unsigned long)(t - mcuLastRead) > (unsigned long)1000 ) {

    (unsigned long)(t + (unsigned long)300);
    if( (unsigned long)(t - mcuHeartBeat) > (unsigned long)15000 ) {
      // Send heartbeat
      mcuSendMessage( F("55aa00000000"));
      if( mcuPhase <= HeartBeat ) mcuPhase = DeviceInfo;
      mcuHeartBeat = t;
      mcuLastRead = t;
    } else {
      if( mcuPhase == MCUPhase::DeviceInfo) {
        mcuSendMessage(F("55aa00010000"));
        mcuPhase = MCUPhase::WorkingMode;
        mcuLastRead = t;
      } else if( mcuPhase == MCUPhase::WorkingMode) {
        mcuSendMessage(F("55aa00020000"));
        mcuPhase = MCUPhase::WorkingStatus;
        mcuLastRead = t;
      } else if( mcuPhase == MCUPhase::WorkingStatus) {
        mcuSendMessage(F("55aa00080000"));
        mcuPhase = MCUPhase::Normal;
        mcuLastRead = t;
      } else if( mcuPhase == MCUPhase::Normal) {
        static char _wifiStatus = -1;
        if( _wifiStatus != (char) mqttConnected() ) {
          char b[32];
          // Report WiFi status
          sprintf(b, "55aa00030001%02X", mqttConnected() ? 4 : 0);
          mcuSendMessage(  b );
          _wifiStatus = (char) mqttConnected();
        }
        mcuLastRead = t;
      }
    }
  }

}

void mcuInit() {
	mcu.begin(9600);
	registerLoop(mcuLoop);
}
