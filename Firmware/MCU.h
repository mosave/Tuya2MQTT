#ifndef mcu_h
#define mcu_h

enum MotorState {
    Idle,
    Opening,
    Closing,
    Moving
};

//MCU_DEBUG only!!!
void mcuSendMessage( const char* data);

bool mcuIsAlive();
MotorState mcuGetMotorState();
int mcuGetPosition();

void mcuReverse( bool reversed );
void mcuSetPosition( int position );
void mcuStop();
void mcuChangeDirection();
void mcuContinue();
void mcuOpenKey();
void mcuCloseKey();
void mcuSingleKey();

void mcuInit();
#endif
