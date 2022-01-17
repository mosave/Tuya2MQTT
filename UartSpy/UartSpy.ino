// WeMos D1 Mini (Clone)

#include <stdarg.h>
#include <SoftwareSerial.h>

// TX (OUT)
#define OUT_RX 12  // D6: к TX оригинального контроллера
#define OUT_TX 14  // D5: не используется но нужен для инициализации SoftwareSerial

// RX (IN)
#define IN_RX 13   // D7:  к RX оригинального контроллера 
#define IN_TX 02   // D4: не используется но нужен для инициализации SoftwareSerial

#define BAUD 9600

enum Mode{ Idle, In, Out };

SoftwareSerial serialIn( IN_RX, IN_TX);
SoftwareSerial serialOut( OUT_RX, OUT_TX);

unsigned long lastRead = 0;

Mode mode = Idle;

void setMode( Mode newMode ) {
  if( mode != newMode) {
    if( mode != Idle) Serial.println();
    if( newMode != Idle) {
      Serial.printf( "%6lu%s ",((unsigned long)(millis() - lastRead)), (newMode==In)?"<" : ">" );
    }
  }
  mode = newMode;
}

void setup() {
  Serial.begin(115200);
  serialIn.begin(BAUD);
  serialOut.begin(BAUD);
  delay(500); 
  Serial.println();  Serial.println("Initializing");
  lastRead = millis();
}

void loop() {

  if( ((unsigned long)(millis() - lastRead) > (unsigned long)20)) {
    setMode(Idle);
    if( serialIn.available() ) {
      if( !serialOut.available()) setMode( In );
    } else {
      if( serialOut.available() ) setMode( Out );
    }
  }

  while( (mode==In) && serialIn.available()) {
    char data = serialIn.read();
    Serial.printf( " %02x", data);
    lastRead = millis();
  }
  while( (mode==Out) && serialOut.available()) {
    char data = serialOut.read();
    Serial.printf( " %02x", data);
    lastRead = millis();
  }
  delay(1);
}
