/*
Run this sketch for reading noise levels

How to use it: 
  - Set-up your hardware as described by the Adafruit tutorial https://learn.adafruit.com/piccolo/wiring
  - Upload code
  - Open Serial Monitor 
  - Keep quite :) Any  
  - Wait until the Serial monitor stopped printing
  - Copy the values of the last line and replace the old noise array values with the new ones in your own sketch
*/

#include <avr/pgmspace.h>
#include <ffft.h>
#include <math.h>
#include <Wire.h>

#ifdef __AVR_ATmega32U4__
 #define ADC_CHANNEL 7
#else
 #define ADC_CHANNEL 0
#endif

int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[FFT_N/2]; // Spectrum output buffer
volatile byte samplePos = 0;     // Buffer position counter
uint8_t       noiseTest[FFT_N/2];// Array for setting max spectrum values for testing max noise

void setup() {
  Serial.begin(9600);
  memset(noiseTest, 0, sizeof(noiseTest));

  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  ADMUX  = ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
  ADCSRA = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE)  | // Interrupt enable
           _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
  ADCSRB = 0;                // Free run mode, no high MUX bit
  DIDR0  = 1 << ADC_CHANNEL; // Turn off digital input for ADC pin
  TIMSK0 = 0;                // Timer0 off
}

ISR(ADC_vect) 
{ // Audio-sampling interrupt
  static const int16_t noiseThreshold = 4;
  int16_t              sample         = ADC; // 0-1023

  capture[samplePos] =
    ((sample > (512-noiseThreshold)) &&
     (sample < (512+noiseThreshold))) ? 0 :
    sample - 512; // Sign-convert for FFT; -512 to +511

  if(++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off
}

void loop() {
  while(ADCSRA & _BV(ADIE)); // Wait for audio sampling to finish
  fft_input(capture, bfly_buff);   // Samples -> complex #s
  samplePos = 0;                   // Reset sample counter
  ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
  fft_execute(bfly_buff);          // Process complex data
  fft_output(bfly_buff, spectrum); // Complex -> spectrum

  testNoise();
}

// Get the spectrum value, compare it with the value in noiseTest and update when a higher value is found
void testNoise() {
  uint8_t x;
  boolean change = false;
  for(x=0; x<FFT_N/2; x++) {
    uint8_t LSB = byte(spectrum[x]);
    if(LSB > noiseTest[x]) {
      noiseTest[x] = LSB;
      change = true;
    }
  }
  
  if(change)
    printNoiseLevels();
}

// Prints the array of max levels
void printNoiseLevels(){
  uint8_t x;
  String arrayString = "";
  for(x = 0; x < FFT_N/2; x++) {
    arrayString += noiseTest[x];
    if(x + 1 < sizeof(noiseTest)) {
      arrayString += ", ";      
    }
  }
  
  Serial.println(arrayString);
}
