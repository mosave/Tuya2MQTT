#include <errno.h>
#include "Config.h"
#include "Comms.h"
#include "Storage.h"
#include "BH1750.h" // Christopher Laws BH1750: https://github.com/claws/BH1750

static char* TOPIC_LMLevel PROGMEM = "Sensors/LightLevel";
static char* TOPIC_LMValid PROGMEM = "Sensors/LightLevelValid";

static char* TOPIC_LMFilteredLevel PROGMEM = "Sensors/LightLevelFiltered";
static char* TOPIC_LMSunriseLevel  PROGMEM = "Sensors/SunriseLevel";
static char* TOPIC_LMSetSunriseLevel  PROGMEM = "Sensors/SetSunriseLevel";
static char* TOPIC_LMSunsetLevel  PROGMEM = "Sensors/SunsetLevel";
static char* TOPIC_LMSetSunsetLevel  PROGMEM = "Sensors/SetSunsetLevel";

static char* TOPIC_LMSunrise PROGMEM = "Sensors/Sunrise";
static char* TOPIC_LMSunset  PROGMEM = "Sensors/Sunset";
static char* TOPIC_LMPhase  PROGMEM = "Sensors/DayPhase";


// debug mode
//#define Debug

#define lmValidityTimeout ((unsigned long)(30*1000))
// Normal sunset detection timeframe (minutes)
#define LMSS_TIMEFRAME 45
// Minimum sunset detection timeframe (minutes) (wait after device start)
#define LMSS_TIMEFRAME_MIN 10

// Default sunrise/sunset trigger light levels:
#define LMSS_SUNRISE_LEVEL 10.0
#define LMSS_SUNSET_LEVEL 5.0

#define LMSS_UNKNOWN 0
#define LMSS_DAY 1
#define LMSS_NIGHT 2

struct LmConfig {
    float sunriseLevel;
    float sunsetLevel;
} lmConfig;

struct LmssDataPoint {
    float l;
    unsigned long t;
};


BH1750 lightMeter;

float lmLevel;
float lmFilteredLevel=-1;
bool lmDetected;

bool lmssEnabled;
LmssDataPoint lmssData[LMSS_TIMEFRAME];
float lmssLevelSum;
int lmssLevelCnt;
int lmssDayPhase = LMSS_UNKNOWN;
char lmssSunrise[8];
char lmssSunset[8];
bool lmssConfirmed;


byte lmMTReg = BH1750_DEFAULT_MTREG;
unsigned long lmUpdatedOn = 0;

float lmAccuracy(float lightLevel) {
    if (lightLevel < 10) {
        return 0.6;
    } else if (lightLevel < 50) {
        return 3;
    } else if (lightLevel < 100) {
        return 5;
    } else {
        return lightLevel / 20;
    }
}

bool lmValid() {
    return (lmDetected && ((unsigned long)(millis() - lmUpdatedOn) < lmValidityTimeout));
}

#pragma region MQTT publishing

void lmPublishSettings() {
  char b[31];
  dtostrf( lmConfig.sunriseLevel, 0, 1, b);
  mqttPublish(TOPIC_LMSunriseLevel, b, true);
  dtostrf( lmConfig.sunsetLevel, 0, 1, b);
  mqttPublish(TOPIC_LMSunsetLevel, b, true);
}

void lmPublishStatus() {
    if (!mqttConnected()) return;
    static int _valid = -1;
    int valid = lmValid() ? 1 : 0;
    if (valid != _valid) {
        if (mqttPublish(TOPIC_LMValid, valid, true)) _valid = valid;
    }
    if (valid == 0) return;

    char b[31];
    bool hindex = false;
    float delta;

    if( lmLevel >= 0 ) {
      static float _lmLevel = -1000;
      delta = lmLevel - _lmLevel;  if (delta < 0) delta = -delta;
      if (delta > lmAccuracy(lmLevel)) {
          unsigned long t = millis();
          static unsigned long publishedOn;
          if ((delta > lmLevel / 3) || ((unsigned long)(t - publishedOn) > (unsigned long)60000)) {
              dtostrf(lmLevel, 0, 1, b);
              if (mqttPublish(TOPIC_LMLevel, b, true)) {
                  _lmLevel = lmLevel;
                  publishedOn = t;
              }
          }
      }
    }
    if( lmssEnabled ) {
        static float _lmFilteredLevel = -1;
        if( (lmFilteredLevel>0) && (lmFilteredLevel != _lmFilteredLevel) ) {
            dtostrf(lmFilteredLevel, 0, 1, b);
            if (mqttPublish(TOPIC_LMFilteredLevel, b, true)) {
                _lmFilteredLevel = lmFilteredLevel;
            }
        }
      
        static int _dayPhase = -1;
        if (lmssDayPhase != _dayPhase) {
            if (lmssDayPhase == LMSS_DAY) {
                mqttPublish(TOPIC_LMPhase, "Day", true);
            } else if (lmssDayPhase == LMSS_NIGHT) {
                mqttPublish(TOPIC_LMPhase, "Night", true);
            } else {
                mqttPublish(TOPIC_LMPhase, "Unknown", true);
            }
            _dayPhase = lmssDayPhase;
        }
        
        static char _lmssSunrise[8] = {0};
        if( (strcmp(_lmssSunrise, lmssSunrise)!=0) && mqttPublish(TOPIC_LMSunrise, lmssSunrise, true) ) {
            strcpy(_lmssSunrise, lmssSunrise);
        }

        static char _lmssSunset[8] = {0};
        if( (strcmp(_lmssSunset, lmssSunset)!=0) && mqttPublish(TOPIC_LMSunset, lmssSunset, true) ) {
            strcpy(_lmssSunset, lmssSunset);
        }
        
    }
}
#pragma endregion

#pragma region Sunset/Sunrise detection code

void lmMqttConnect() {
  mqttSubscribeTopic( TOPIC_LMSetSunriseLevel );
  mqttSubscribeTopic( TOPIC_LMSetSunsetLevel );
  lmPublishSettings();
}

bool lmMqttCallback(char* topic, byte* payload, unsigned int length) {
    //aePrintf("mqttCallback(\"%s\", %u, %u )\r\n", topic, payload, length);
        
    int cmd = 0;
    if( lmssEnabled && mqttIsTopic( topic, TOPIC_LMSetSunriseLevel ) ) {
      cmd = 1;
    } else if( lmssEnabled && mqttIsTopic( topic, TOPIC_LMSetSunsetLevel ) ) {
      cmd = 2;
    }
    
    if( cmd>0 ) {
      if( (payload != NULL) && (length > 0) && (length<31) ) {
        char s[31];
        memset( s, 0, sizeof(s) );
        strncpy( s, ((char*)payload), length );
        
        errno = 0;
        float v = ((int)(strtof(s,NULL) * 10.0)) / 10.0 ;
        
        if ( (errno==0) && (v>=0.1) && (v<1000) ) {
          if( cmd==1 ) {
            lmConfig.sunriseLevel = v;
            mqttPublish(TOPIC_LMSetSunriseLevel,(char*)NULL, false);
          } else {
            lmConfig.sunsetLevel = v;
            mqttPublish(TOPIC_LMSetSunsetLevel,(char*)NULL, false);
          }
          storageSave();
          lmPublishSettings();
        }
      }
      return true;
    }
    return false;
}


bool lmssApproximate(double* a, double* b, bool filter) {
    unsigned long t0 = millis();
    double sL = 0;
    double sT = 0;
    double sT2 = 0;
    double sTL = 0;
    double dFilter = 1000000;
    int n = 0;

#ifdef Debug
  char dbgs[255];
#endif  
    if( filter ) {
#ifdef Debug
  aePrint("Calculating quartile: ");
#endif  
      double delta[LMSS_TIMEFRAME/4+4];
      memset(delta,0,sizeof(delta));
      dFilter = 0;
      for (int i = 0; i < LMSS_TIMEFRAME; i++) {
        if( lmssData[i].t ) {
          double t = (t0 - lmssData[i].t)/1000.0;
          double d = abs( (*a) * t + (*b) - lmssData[i].l);

#ifdef Debug
  aePrint(d); aePrint(" ");
#endif
          n++;
          for(int j=0;j<=LMSS_TIMEFRAME/4; j++ ) {
            if(d>delta[j]) {
              memmove( &delta[j+1], &delta[j], sizeof(double)*(LMSS_TIMEFRAME/4-j+1) );
              delta[j] = d;
              j = 100000;
            }
          }
        }
      }
#ifdef Debug
      aePrintln();
      aePrint("delta: ");
      for(int i=0;i<LMSS_TIMEFRAME/4+4;i++) {
        aePrint(delta[i]); 
        if(i==n/4) aePrint("|");
        aePrint(" ");
      }
      aePrintln();
    sprintf( dbgs, "n/4=%d, dFilter= %f ",n/4,delta[n/4]);
    mqttPublish("Sensors/dbg1", dbgs, false);
#endif
      
      if (n < 8) return false;
      dFilter = delta[n/4];
    }

#ifdef Debug
  aePrint("Approximating, dFilter="); aePrint(dFilter); aePrintln();
#endif
    n = 0;
    for (int i = 0; i < LMSS_TIMEFRAME; i++) {
      if(lmssData[i].t) {
        double l = lmssData[i].l;
        double t = (t0 - lmssData[i].t)/1000.0;
        if ( t <= LMSS_TIMEFRAME*60+5 ) {
            double l2 = l * l;
            // Take this point if filtering is off or point is "close" to graph
            if( !filter || ( abs((*a) * t + (*b) - l) <= dFilter ) ) {
                sT += t;
                sL += l;
                sT2 += t * t;
                sTL += t * l;
                n++;
            }
        }
      }
    }
    if (n < 5) return false;

    
    *a = ((n * sTL) - (sT * sL)) / (n * sT2 - sT * sT);
    *b = (sL - (*a) * sT) / n;
#ifdef Debug
    sprintf(dbgs,"L0=%f n=%d, sT=%f sT2=%f, sL=%f, sTL=%f, a=%f, b=%f, dFilter=%f",
              lmssData[0].l, n, sT, sT2,    sL,    sTL,    (*a), (*b), dFilter ); 
    aePrintln(dbgs);
    mqttPublish("Sensors/dbg2", dbgs, false);
#endif
    return true;
}

void lmssUpdateStatus() {
    double a, b;
    if (lmssApproximate(&a, &b, false) && lmssApproximate(&a, &b, true) ) {
      double lMin = 100000;
      double lMax = 0;
      unsigned long t0 = millis();
      for(int i=0; i<LMSS_TIMEFRAME; i++) {
        if( lmssData[i].t>0 ) {
          double t = (t0 - lmssData[i].t)/1000.0;
          if ( t <= LMSS_TIMEFRAME*60+5 ) {
            if( lMin > lmssData[i].l ) lMin = lmssData[i].l;
            if( lMax < lmssData[i].l ) lMax = lmssData[i].l;
          }
        }
      }
      
      // lB = current light level approximation
      double lB = b;
      // lA = "(TIMEFRAME-1) minutes ago" light level approximation
      double lA = a * ((double)(LMSS_TIMEFRAME-1)*60.0) + b;
      if( lA<=lMin ) lA = lMin; else if( lA>lMax ) lA = lMax;
      if( lB<=lMin ) lB = lMin; else if( lB>lMax ) lB = lMax;
      lmFilteredLevel = lB;

      if ( (lmssDayPhase == LMSS_NIGHT) && (lA < lmConfig.sunriseLevel) && (lB > lmConfig.sunriseLevel) ) {
        // Sunrise detected!
          lmssDayPhase = LMSS_DAY;
          if( lmssConfirmed ) {
              tm* lt = commsGetTime();
              if( lt ) sprintf(lmssSunrise, "%02d:%02d", lt->tm_hour, lt->tm_min);
          }
          lmssConfirmed = false;
      } else if ( (lmssDayPhase == LMSS_DAY) && (lA > lmConfig.sunsetLevel) && (lB < lmConfig.sunsetLevel) ) {
        // Sunset detected!
          lmssDayPhase = LMSS_NIGHT;
          if( lmssConfirmed ) {
            tm* lt = commsGetTime();
            if( lt ) sprintf(lmssSunset, "%02d:%02d", lt->tm_hour, lt->tm_min);
          }
          lmssConfirmed = false;
      } else if ( lmssDayPhase == LMSS_UNKNOWN ) {
        // startup: initialize current day phase
          if( (lB > lmConfig.sunriseLevel) && (lB > lmConfig.sunsetLevel) ) {
            lmssDayPhase = LMSS_DAY;
            lmssConfirmed = true;
          } else if( (lB < lmConfig.sunriseLevel) && (lB < lmConfig.sunsetLevel) ) {
            lmssDayPhase = LMSS_NIGHT;
            lmssConfirmed = true;
          }
      }
      if ( (lmssDayPhase == LMSS_DAY) && (lB > lmConfig.sunriseLevel) && (lB > lmConfig.sunsetLevel) ) {
        lmssConfirmed = true;
      }
      if ( (lmssDayPhase == LMSS_NIGHT) && (lB < lmConfig.sunriseLevel) && (lB < lmConfig.sunsetLevel) ) {
        lmssConfirmed = true;
      }
    }
}

#pragma endregion

void lmLoop() {
    if (!lmDetected) return;

    unsigned long t = millis();
    static unsigned long checkedOn;

    if ((unsigned long)(t - checkedOn) > (unsigned long)1000) {
        checkedOn = t;
        lmLevel = lightMeter.readLightLevel();

        if (lmLevel >= 0) {
            byte mtreg = BH1750_DEFAULT_MTREG;
            if (lmLevel <= 10.0) { //very low light environment
                mtreg = 138;
            } else  if (lmLevel > 30000.0) { // reduce measurement time - needed in direct sun light
                mtreg = 32;
            }
            if (mtreg != lmMTReg) {
                lmMTReg = mtreg;
                lightMeter.setMTreg(mtreg);
                lmLevel = -1;
                checkedOn -= (unsigned long)800;
            }
        }

        if (lmLevel >= 0) {
            lmUpdatedOn = t;
            if (lmssEnabled) {
                lmssLevelSum = lmssLevelSum + lmLevel;
                lmssLevelCnt++;
                if ((lmssData[0].t == 0) && (lmssLevelCnt > 10) ||
                    (lmssData[0].t > 0) && ((unsigned long)(t - lmssData[0].t) >= (unsigned long)60000)) {
                    memmove( &lmssData[1], &lmssData[0], sizeof(lmssData[0])*(LMSS_TIMEFRAME-1) );
                    lmssData[0].t = t;
                    lmssData[0].l = lmssLevelSum / lmssLevelCnt;
                    if( lmssData[0].l>10000 ) lmssData[0].l = 10000;
                    lmssUpdateStatus();
                    lmssLevelSum = 0; lmssLevelCnt = 0;
                }
            }
        }
        lmPublishStatus();
    }
}
bool lightMeterValid() {
    return lmValid();
}
float lightMeterLevel() {
    return lmLevel;
}

void lightMeterInit(bool sunTracker) {
    lmDetected = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    lmssEnabled = lmDetected && sunTracker;
    if (lmDetected) {
        aePrintln(F("BH1750 found"));
        if (lmssEnabled) {
            memset(lmssSunrise, 0, sizeof(lmssSunrise));
            memset(lmssSunset, 0, sizeof(lmssSunset));
            memset(lmssData, 0, sizeof(lmssData));
            
            lmssLevelSum = 0;
            lmssLevelCnt = 0;
            storageRegisterBlock('L', &lmConfig, sizeof(lmConfig));
            if ((lmConfig.sunriseLevel == 0) || (lmConfig.sunsetLevel == 0)) {
                lmConfig.sunriseLevel = LMSS_SUNRISE_LEVEL;
                lmConfig.sunsetLevel = LMSS_SUNSET_LEVEL;
            }
#ifdef Debug
            randomSeed(micros());
            for (int i = /*LMSS_TIMEFRAME*/ 10; i>=0; i--) {
                lmssData[i].t = millis() - ((unsigned long)i) * (unsigned long)60000L;
                lmssData[i].l = /*80 - */(i / (float)LMSS_TIMEFRAME)*80 + random(50);
                if (lmssData[i].l < 0) lmssData[i].l = 0;
                aePrint((int)(lmssData[i].l * 100) / 100.0); aePrint(" ");
            }
            aePrintln();
            double a, b;
            aePrintln(lmssApproximate(&a, &b, false));
            aePrintln(lmssApproximate(&a, &b, true));
    
            aePrintln("");
#endif
            mqttRegisterCallbacks( lmMqttCallback, lmMqttConnect );
        }

        registerLoop(lmLoop);
    } else {
        aePrintln(F("Error initialising BH1750"));
    }
}

void lightMeterInit() {
    lightMeterInit(false);
}
