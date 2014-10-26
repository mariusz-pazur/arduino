#include "FastLED.h"

#define NUM_LEDS 25
#define DATA_PIN 7
#define CLOCK_PIN 9

CRGB leds[NUM_LEDS];
const int heartLength = 24;
int heartIndex[heartLength];
const int lLength = 5;
int lIndex[lLength];
const int oLength = 6;
int oIndex[oLength];
const int vLength = 7;
int vIndex[vLength];
const int eLength = 7;
int eIndex[eLength];


// This function sets up the ledsand tells the controller about them
void setup() {	
   	delay(2000);
        FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);    
        int i; 
        int current;
        for (i = 0; i < heartLength; i++)
        {
          heartIndex[i] = NUM_LEDS - heartLength -1 + i;
        }
        current = heartLength;
        for (i = current; i > current - lLength; i--) {
          lIndex[current-i] = i;
        }
        current = current + lLength;
        for (i = current; i < current + oLength; i++) {
          oIndex[i-current] = i;
        }
        current = current + oLength;
        for (i = current; i < current + vLength; i++) {
          vIndex[i-current] = i;
        }
        current = current + vLength;
        for (; i < current + eLength; i++) {
          eIndex[i-current] = i;
        }
}

void roundTheHeart(CRGB color) {
  for(int led = 0; led < heartLength; led++) {
      
      leds[heartIndex[led]] = color;
      FastLED.show();
      delay(100);
      leds[heartIndex[led]] = CRGB::Black;
   }   
}

void roundTheHeartFill(CRGB color) {
  for(int led = 0; led < heartLength; led++) {
      
      leds[heartIndex[led]] = color;
      FastLED.show();
      delay(100);      
   }   
}

void heartBlink(CRGB color)
{
  for(int led = 0; led < heartLength; led++) {      
      leds[heartIndex[led]] = CRGB::Black;           
   }
   FastLED.show();
   delay(100);
   for(int led = 0; led < heartLength; led++) {      
      leds[heartIndex[led]] = color;           
   }
   FastLED.show();
   delay(300);    
}

void lShow(CRGB color) {
  //for(int led = 0; led < lLength; led++) {      
   //   leds[lIndex[led]] = color;           
   //}
      leds[24] = color;
   //leds[25] = color;
   leds[23] = color;
   leds[22] = color;
   FastLED.show();
}
void lHide() {
  CRGB color = CRGB::Black;
  //for(int led = 0; led < lLength; led++) {      
   //   leds[lIndex[led]] = CRGB::Black;           
   //}
   leds[24] = color;
   //leds[25] = color;
   leds[23] = color;
   leds[22] = color;
   
   FastLED.show();
}


void loop() {
   //roundTheHeart(CRGB( 128, 0, 0));
   //roundTheHeart(CRGB( 128, 0, 0));
   //roundTheHeartFill(CRGB( 128, 0, 0));
   //heartBlink(CRGB( 128, 0, 0));
   //heartBlink(CRGB( 128, 0, 0));
   lShow(CRGB(128,0,128));
   delay(1000);
   lHide();
   delay(1000);
}
