/*
  Emon.h - Library for openenergymonitor
  Created by Trystan Lea, April 27 2010
  GNU GPL
  modified to use up to 12 bits ADC resolution (ex. Arduino Due)
  by boredman@boredomprojects.net 26.12.2013
  Low Pass filter for offset removal replaces HP filter 1/1/2015 - RW
*/

#ifndef EmonLib_h
#define EmonLib_h

#if defined(ARDUINO) && ARDUINO >= 100

#include "Arduino.h"

#else

#include "WProgram.h"

#endif

// define theoretical vref calibration constant for use in readvcc()
// 1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
// override in your code with value for your specific AVR chip
// determined by procedure described under "Calibrating the internal reference voltage" at
// http://openenergymonitor.org/emon/buildingblocks/calibration
#ifndef READVCC_CALIBRATION_CONST
#define READVCC_CALIBRATION_CONST 1126400L
#endif

// to enable 12-bit ADC resolution on Arduino Due,
// include the following line in main sketch inside setup() function:
//  analogReadResolution(ADC_BITS);
// otherwise will default to 10 bits, as in regular Arduino-based boards.
#if defined(__arm__)
#define ADC_BITS    12
#else
#define ADC_BITS    10
#endif

#define ADC_COUNTS  (1<<ADC_BITS)


class EnergyMonitor
{
  public:

    void voltage(int16_t _inADCV, double _VCAL, double _PHASECAL);
    void current(unsigned int _inPinI, double _ICAL);

    void voltageTX(double _VCAL, double _PHASECAL);
    void currentTX(unsigned int _channel, double _ICAL);

    void calcVI(unsigned int crossings, unsigned int timeout);
    double calcIrms(unsigned int NUMBER_OF_SAMPLES);
    void serialprint();

    long readVcc();
    //Useful value variables
    double realPower,
      apparentPower,
      powerFactor,
      Vrms,
      Irms;

  private:

    //Set Voltage and current input pins
    int16_t inADCV;
    unsigned int inPinI;
    //Calibration coefficients
    //These need to be set in order to obtain accurate results
    double VCAL;
    double ICAL;
    double PHASECAL;

    //--------------------------------------------------------------------------------------
    // Variable declaration for emon_calc procedure
    //--------------------------------------------------------------------------------------
    int sampleV;                        //sample_ holds the raw analog read value
    int sampleI;

    double lastFilteredV,filteredV;          //Filtered_ is the raw analog value minus the DC offset
    double filteredI;
    double offsetV;                          //Low-pass filter output
    double offsetI;                          //Low-pass filter output

    double phaseShiftedV;                             //Holds the calibrated phase shifted voltage.

    double sqV,sumV,sqI,sumI,instP,sumP;              //sq = squared, sum = Sum, inst = instantaneous

    int startV;                                       //Instantaneous voltage at start of sample window.

    boolean lastVCross, checkVCross;                  //Used to measure number of times threshold is crossed.


};


//--------------------------------------------------------------------------------------
// Sets the pins to be used for voltage and current sensors
//--------------------------------------------------------------------------------------
void EnergyMonitor::voltage(int16_t _inADCV, double _VCAL, double _PHASECAL)
{
  inADCV = _inADCV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetV = ADC_COUNTS>>1;
}

void EnergyMonitor::current(unsigned int _inPinI, double _ICAL)
{
  inPinI = _inPinI;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS>>1;
}

//--------------------------------------------------------------------------------------
// Sets the pins to be used for voltage and current sensors based on emontx pin map
//--------------------------------------------------------------------------------------
// void EnergyMonitor::voltageTX(double _VCAL, double _PHASECAL)
// {
//   inPinV = 2;
//   VCAL = _VCAL;
//   PHASECAL = _PHASECAL;
//   offsetV = ADC_COUNTS>>1;
// }

void EnergyMonitor::currentTX(unsigned int _channel, double _ICAL)
{
  if (_channel == 1) inPinI = 3;
  if (_channel == 2) inPinI = 0;
  if (_channel == 3) inPinI = 1;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS>>1;
}

//--------------------------------------------------------------------------------------
// emon_calc procedure
// Calculates realPower,apparentPower,powerFactor,Vrms,Irms,kWh increment
// From a sample window of the mains AC voltage and current.
// The Sample window length is defined by the number of half wavelengths or crossings we choose to measure.
//--------------------------------------------------------------------------------------
void EnergyMonitor::calcVI(unsigned int crossings, unsigned int timeout)
{
  #if defined emonTxV3
  int SupplyVoltage=3300;
  #else
  int SupplyVoltage = readVcc();
  #endif

  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  unsigned long start = millis();    //millis()-start makes sure it doesnt get stuck in the loop if there is an error.

  while(1)                                   //the while loop...
  {
    startV = inADCV;                    //using the voltage waveform
    if ((startV < (ADC_COUNTS*0.55)) && (startV > (ADC_COUNTS*0.45))) break;  //check its within range
    if ((millis()-start)>timeout) break;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < crossings) && ((millis()-start)<timeout))
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------
    sampleV = inADCV;                 //Read in raw voltage signal
    sampleI = analogRead(inPinI);                 //Read in raw current signal

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    offsetV = offsetV + ((sampleV-offsetV)/1024);
    filteredV = sampleV - offsetV;
    offsetI = offsetI + ((sampleI-offsetI)/1024);
    filteredI = sampleI - offsetI;

    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------
    sqI = filteredI * filteredI;                //1) square current values
    sumI += sqI;                                //2) sum

    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV);

    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------
    instP = phaseShiftedV * filteredI;          //Instantaneous Power
    sumP +=instP;                               //Sum

    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied.

  double V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Vrms = V_RATIO * sqrt(sumV / numberOfSamples);

  double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / numberOfSamples);

  //Calculation power values
  realPower = V_RATIO * I_RATIO * sumP / numberOfSamples;
  apparentPower = Vrms * Irms;
  powerFactor=realPower / apparentPower;

  //Reset accumulators
  sumV = 0;
  sumI = 0;
  sumP = 0;
//--------------------------------------------------------------------------------------
}

//--------------------------------------------------------------------------------------
double EnergyMonitor::calcIrms(unsigned int Number_of_Samples)
{

  #if defined emonTxV3
    int SupplyVoltage=3300;
  #else
    int SupplyVoltage = readVcc();
  #endif


  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sampleI = analogRead(inPinI);

    // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset,
    //  then subtract this - signal is now centered on 0 counts.
    offsetI = (offsetI + (sampleI-offsetI)/1024);
    filteredI = sampleI - offsetI;

    // Root-mean-square method current
    // 1) square current values
    sqI = filteredI * filteredI;
    // 2) sum
    sumI += sqI;
  }

  double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / Number_of_Samples);

  //Reset accumulators
  sumI = 0;
  //--------------------------------------------------------------------------------------

  return Irms;
}

void EnergyMonitor::serialprint()
{
  Serial.print(realPower);
  Serial.print(' ');
  Serial.print(apparentPower);
  Serial.print(' ');
  Serial.print(Vrms);
  Serial.print(' ');
  Serial.print(Irms);
  Serial.print(' ');
  Serial.print(powerFactor);
  Serial.println(' ');
  delay(100);
}

//thanks to http://hacking.majenko.co.uk/making-accurate-adc-readings-on-arduino
//and Jérôme who alerted us to http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/

long EnergyMonitor::readVcc() {
  long result;

  //not used on emonTx V3 - as Vcc is always 3.3V - eliminates bandgap error and need for calibration http://harizanov.com/2013/09/thoughts-on-avr-adc-accuracy/

  #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);

  #endif


  #if defined(__AVR__)
  delay(2);                                        // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);                             // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = READVCC_CALIBRATION_CONST / result;  //1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
  return result;
  #elif defined(__arm__)
  return (3300);                                  //Arduino Due
  #else
  return (3300);                                  //Guess that other un-supported architectures will be running a 3.3V!
  #endif
}




#endif
