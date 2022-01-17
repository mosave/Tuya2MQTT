#ifndef mcu_h
#define mcu_h

enum MotorState {
    Idle,
    Opening,
    Closing,
    Moving
};

enum MotorLimit {
    Up,
    Down,
    Middle
};

enum MotorMode {
    Linkage,
    Inching
};


//MCU_DEBUG only!!!
void mcuSendMessage( const char* data);

bool mcuIsAlive();
MotorState mcuGetMotorState();
int mcuGetPosition();
bool mcuNetworkResetRequested();

void mcuReverse( bool reversed );
void mcuSetLimit( MotorLimit limit);
void mcuClearLimit(MotorLimit limit);
void mcuSetMotorMode(MotorMode mode);
void mcuPairing();

void mcuSetPosition( int position );
void mcuStop();
void mcuChangeDirection();
void mcuContinue();
void mcuOpenKey();
void mcuCloseKey();
void mcuSingleKey();

void mcuInit();
#endif
