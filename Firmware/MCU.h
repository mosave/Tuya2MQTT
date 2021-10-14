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

void mcuReverse();
void mcuStop();
void mcuSetPosition( int position );

// One-key curtain operation. Cycle thru open-close states according to last direction.
void mcuContinue();

void mcuInit();
#endif
