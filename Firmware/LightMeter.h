#ifndef lightmeter_h
#define lightmeter_h


bool lightMeterValid();
float lightMeterLevel();

/// <summary>Initialize Light Meter hardware and (optionally) allow sunrise/sunset detection functionality</summary>
/// <param name="sunTracker">Enable sunrise/sunset detection</param>
void lightMeterInit( bool sunTracker );

/// <summary>Initialize Light Meter hardware with sunset detector disabled</summary>
void lightMeterInit();

#endif
