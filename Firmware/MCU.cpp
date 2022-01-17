#include <Arduino.h>
#include "Config.h"
#include "MCU.h"
#include "Comms.h"

MotorState mcuMotorState = Idle;
MotorState mcuDirection = Closing;
bool mcuCalibrated = false;
int mcuPosition = false;
char mcuData[128];
int mcuLen = 0;
bool mcuTriggeredByHand = false;
bool mcuIsNetworkResetRequested = false;
int mcuCommandSet = 0;

unsigned long mcuLastRead = 0;
unsigned long mcuHeartBeat = 0;
unsigned long mcuLastAlive = 0;
int mcuPhase = 0;

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
  char s1[48];
  strcpy( s, "< ");
  if( (prefix != NULL) && (strlen(prefix)>0) ) strcat(strcat( s, prefix)," ");

  for(int i=0; i<mcuLen; i++ ) {
    sprintf( s1, "%02x", mcuData[i]);
    if( i == mcuLen-1 ) strcat(s, ":");
    strcat( s, s1);
  }
  sprintf(s1, " phs=%d,pos=%d,state=%d,dir=%d,cs=%d", mcuPhase, mcuPosition, mcuMotorState, mcuDirection, mcuCommandSet);
  strcat(s, s1);
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
  sprintf(s, "> %s:%02x phs=%d,pos=%d,state=%d,dir=%d,cs=%d", data, sum, mcuPhase, mcuPosition, mcuMotorState, mcuDirection, mcuCommandSet);
  mqttPublish("Log",s,false);
  aePrintln(s);
#endif
  mcuLastRead = millis() + (unsigned long)500;
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

bool mcuNetworkResetRequested() {
    bool b = mcuIsNetworkResetRequested;
    mcuIsNetworkResetRequested = false;
    return b;
}

void mcuReverse( bool reversed ) {
  if( reversed ) { // Enable reversing
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056700000101") );
    } else {
      mcuSendMessage( F("55aa000600050500000101") );
    }
  } else { // Disable reversing
    if( mcuCommandSet==0 ) {
      mcuSendMessage( F("55aa000600056700000100") );
    } else {
      mcuSendMessage( F("55aa000600050500000100") );
    }
  }
  delay(300);
}

void mcuSetLimit(MotorLimit limit) {
    switch (limit) {
    case MotorLimit::Up:
        //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056901000101"));
        //} else {
        //    mcuSendMessage(F("55aa000600056701000101"));
        //}
        break;
    case MotorLimit::Down:
        //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056701000101"));
        //} else {
        //    mcuSendMessage(F("55aa000600056901000101"));
        //}
        break;
    case MotorLimit::Middle:
        //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056801000101"));
        //} else {
        //    mcuSendMessage(F("55aa000600056801000101"));
        //}
        break;
    }
    delay(300);
}
void mcuClearLimit(MotorLimit limit) {
    //mcu.write(0xFF);

    switch (limit) {
    case MotorLimit::Up:
        //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056901000100"));
        //} else {
        //    mcuSendMessage(F("55aa000600056701000100"));
        //}
        break;
    case MotorLimit::Down:
        //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056701000100"));
        //} else {
        //    mcuSendMessage(F("55aa000600056901000100"));
        //}
        break;
    case MotorLimit::Middle:
        //if (mcuCommandSet == 0) {
            mcuSendMessage(F("55aa000600056801000100"));
        //} else {
        //    mcuSendMessage(F("55aa000600056801000100"));
        //}
        break;
    }
    delay(300);
}
void mcuSetMotorMode(MotorMode mode) {
    switch (mode) {
    case MotorMode::Inching:
        //if (mcuCommandSet == 0) {
            mcuSendMessage(F("55aa000600056a04000101"));
        //} else {
        //    mcuSendMessage(F("55aa000600056a04000101"));
        //}
        break;
    default:
        //if (mcuCommandSet == 0) {
            mcuSendMessage(F("55aa000600056a04000100"));
        //} else {
        //    mcuSendMessage(F("55aa000600056a04000100"));
        //}
        break;
    }
    delay(300);
}
void mcuPairing() {
    //if (mcuCommandSet == 0) {
        mcuSendMessage(F("55aa000600056501000101"));
    //} else {
    //    mcuSendMessage(F("55aa000600056501000101"));
    //}
    delay(300);
}


void mcuSetPosition( int position ) {
  mcuTriggeredByHand = false;
  if( position < mcuPosition ) {
    mcuDirection = Opening;
  } else if( position > mcuPosition ) {
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
  delay(300);
}

void mcuStop() {
  mcuTriggeredByHand = false;
  if( mcuCommandSet==0 ) {
    mcuSendMessage( F("55aa000600056504000101") );
  } else {
    mcuSendMessage( F("55aa000600050104000101") );
  }
  delay(300);
}

void mcuChangeDirection() {
  bool isMoving = (mcuMotorState!=MotorState::Idle);
  if( isMoving) {
    mcuStop();
  }
  mcuDirection = (mcuDirection == MotorState::Opening) ? MotorState::Closing : MotorState::Opening;
  if( isMoving ) {
    delay(500);
    mcuSetPosition( (mcuDirection == Opening) ? 100 : 0 );
  }
  delay(300);
}

void mcuContinue() {
  if ( (mcuMotorState==MotorState::Idle) && (!mcuCalibrated) && (mcuPosition==50)) {
    mcuDirection = (mcuDirection == MotorState::Opening) ? MotorState::Closing : MotorState::Opening;
  }
  
  if( (mcuDirection == Opening) && (mcuPosition>97) ) {
    mcuDirection = Closing;
  } else if( (mcuDirection == Closing) && (mcuPosition<3) ) {
    mcuDirection = Opening;
  }
  mcuSetPosition( (mcuDirection == Opening) ? 100 : 0 );
  delay(300);
}

void mcuOpenKey(){
  if( mcuPosition<99) {
    if (mcuMotorState == MotorState::Idle) {
      mcuSetPosition(100);
    } else {
      mcuStop();
    }
  }
  delay(300);
}

void mcuCloseKey(){
  if( mcuPosition>0) {
    if (mcuMotorState == MotorState::Idle) {
      mcuSetPosition(0);
    } else {
      mcuStop();
    }
  }
  delay(300);
}

void mcuSingleKey(){
  if (mcuMotorState == MotorState::Idle) {
      mcuContinue();
  } else {
      mcuStop();
  }
  delay(300);
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
    case 0x04: { // Check if network reset was requested by pressing motor button *3:
        //< 55 aa 03 04 00 00 06
        if( (mcuData[4]==0x00) && (mcuData[5] == 0x00) && (mcuData[6] == 0x06) ){
            mcuIsNetworkResetRequested = true;
        }
        break;
    }
    
    case 0x07: {
      switch (mcuData[6]) { // <=== DP ID
        case 0x67: 
        case 0x05: { // Motor reversing state - NOT RELIABLE :(
          //mcuReversed = !!mcuData[10];
          break;
        }
        case 0x65: 
        case 0x01: { // Operation mode state
          if( mcuData[7]==4 ) { // <=== Data type = ENUM
            int cs = mcuCommandSet;
            mcuCommandSet = ( mcuData[6]>0x10 ) ? 0 : 1;
            if( cs != mcuCommandSet) {
                aePrint("MCU command set "); aePrintln(mcuCommandSet);
            }
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
          }
          break;
        }
        // Position reports during operation
        case 0x66: 
        case 0x07:  {
          if( (mcuCommandSet) && (mcuLen>10) ) {
            if( mcuData[10] == 1) {
              mcuDirection = Opening;
            } else {
              mcuDirection = Closing;
            }
          }
          mcuMotorState = mcuDirection;
          if( mcuLen>13 ) {
            mcuPosition = mcuData[13];
            if (mcuPosition >= 95) mcuPosition = 100;
            if (mcuPosition <= 5) mcuPosition = 0;
            if (mcuPosition != 50) mcuCalibrated = true;
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
          mcuMotorState = MotorState::Idle;
          if( mcuLen>13 ) {
            mcuPosition = mcuData[13];
            if (mcuPosition >= 95) mcuPosition = 100;
            if (mcuPosition <= 5) mcuPosition = 0;
            if (mcuPosition != 50) mcuCalibrated = true;
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
  if (mcuLastRead==0) mcuLastRead = t;

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
      mcuData[0]=mcuData[1]=0;
    }

    mcuLastRead = t;
    mcuData[mcuLen++] = mcu.read(); 
    // Invalid packet header (0x55AA)
    if( (mcuData[0]!=0x55) || (mcuLen>1) && (mcuData[1]!=0xAA) ) {
      mcuLogData("Invalid header");
      mcuData[0]=mcuData[1]=0;
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
  
#ifdef MCU_DEBUG
if (!mqttConnected() ) return;
#endif

  if( (unsigned long)(t - mcuLastRead) > (unsigned long)500 ) {
    if( mcuPhase == 0 ) { // Just Started Up
      // Send heartbeat
      mcuSendMessage( F("55aa00000000"));
      delay(100);
      mcuPhase++;
    } else if( mcuPhase == 1) { // Get Device Info
      mcuSendMessage(F("55aa00010000"));
      delay(100);
      mcuPhase++;
    } else if( mcuPhase == 2) { // Query work mode
      mcuSendMessage(F("55aa00020000"));
      delay(100);
      mcuPhase++;
    } else if( mcuPhase == 3) { // Query device status
      mcuSendMessage(F("55aa00080000"));
      delay(100);
      mcuPhase++;
    } else if( mcuPhase == 4) { // Send "Stop" command using command set 0 to test reaction
      mcuSendMessage( F("55aa000600056504000101") );
      delay(200);
      mcuPhase++;
    } else if( mcuPhase == 5) { // Send "Stop" command using command set 1 to test reaction
      mcuSendMessage( F("55aa000600050104000101") );
      delay(200);
      mcuPhase++;
    } else { // Normal operation mode
      if( (unsigned long)(t - mcuHeartBeat) > (unsigned long)10000 ) {
        // Send heartbeat
        mcuSendMessage( F("55aa00000000"));
        mcuHeartBeat = t;
      } else {
        static char _wifiStatus = -1;
        if( _wifiStatus != (char) mqttConnected() ) {
          char b[32];
          // Report WiFi status
          sprintf(b, "55aa00030001%02X", mqttConnected() ? 4 : 0);
          mcuSendMessage(  b );
          _wifiStatus = (char) mqttConnected();
        }
      }
    }
  }
}

void mcuInit() {
	mcu.begin(9600);
	registerLoop(mcuLoop);
}
