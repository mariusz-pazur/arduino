#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include "printf.h"
#include "hardware.h"
#include "aes256.h"
#include "crypto.h"

static uint8_t myAddress[] =  {0xF0, 0xF0, 0xF0, 0xF0, 0xD3};
static uint8_t mainAddress[] = {0xF0, 0xF0, 0xF0, 0xF0, 0xE1 };
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;
static uint8_t commandAndResponseLength = 16;

int ledsPin = 6;
boolean hasToTurnOff = false;
uint8_t stateLength = 4;
uint8_t state[] = { 0, 0, 0, 0};
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, ledsPin, NEO_GRB + NEO_KHZ800);

aes256_context ctxt;
//uint8_t cryptoKey[] = { // set this in crypto.h
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
//    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
//    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
//    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
//  };  

#define HA_REMOTE_LEDENV_DEBUG 1

void setup(void)
{
#if HA_REMOTE_LEDENV_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led Environment (leds & temp & humidity & noise)\n\r");
#endif
  setupRF(); 
#if HA_REMOTE_LEDENV_DEBUG
  printf("Free RAM: %d B\n\r", freeRam()); 
#endif
  setupEncryption();
  setupLeds();
}

void setupRF(void)
{
  Mirf.cePin = rf24cePin;
  Mirf.csnPin = rf24csnPin;
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.setRADDR(myAddress);
  Mirf.setTADDR(mainAddress);
  Mirf.payload = commandAndResponseLength;
  Mirf.channel = 90;
  Mirf.configRegister( RF_SETUP, ( 1<<2 | 1<<1 | 1<<5 ) );
  Mirf.config();
}

void setupEncryption()
{  
  aes256_init(&ctxt, cryptoKey);          
}

void setupLeds()
{
  strip.begin();
  strip.show();
}

void loop(void)
{ 
  byte command[commandAndResponseLength]; 
  // if there is data ready
  if(!Mirf.isSending() && Mirf.dataReady())
  {
    // Dump the payloads until we've gotten everything    
    byte response[commandAndResponseLength];    
    bool done = false;
    Mirf.getData(command);  
    aes256_decrypt_ecb(&ctxt, command);   
#if HA_REMOTE_LEDENV_DEBUG
    printf("Read command from radio {%d,%d,%d}\n\r", command[1],command[2],command[3]);
#endif    
    if (command[1] == 2)//RemoteLED
    {
      if (command[2] == 0) //turn off 
      { 
        for (int i = 0; i < stateLength; i++)
          state[i] = 0;                
      }
      else if (command[2] == 1) //enableEffect
      {    
        state[0] = 1;
        state[1] = command[3];      
        if (command[3] == 0) //rainbowWheel
        { 
          state[2] = 0;
          state[3] = 0;         
        }
      }
      else if (command[2] == 2) //read state
      {        
      } 
    } 
    for (int i = 0; i < commandAndResponseLength; i++)
    {
      if (i < stateLength)
        response[i] = state[i];    
      else
        response[i] = 0;
    }
    aes256_encrypt_ecb(&ctxt, response);
    Mirf.send(response);
#if HA_REMOTE_LEDENV_DEBUG
    printf("Sent state response {%d,%d,%d,%d}\n\r", state[0],state[1],state[2],state[3]);
#endif         
  } 
  if (command[1] == 2)//RemoteLED
  {
    if (state[0] == 0) //turn off
    {
      colorWipe(0,10); 
    }
    else if (state[0] == 1) //enable effect
    {
      if (state[1] == 0) //rainbowWheel
      {
        rainbowCycle(50);
      }
    }
  }
}

void rainbowCycle(uint8_t wait) 
{
  uint16_t i, j, k;

  for(j=0; j<256*5; j++) 
  { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) 
    {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));            
    }
    strip.show();
    for (k=0; k < 5; k++ )
    {
      delay(wait/5);
      if (!Mirf.isSending() && Mirf.dataReady())
        return;
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) 
{
  if(WheelPos < 85) 
  {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) 
  {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else 
  {
    WheelPos -= 170;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) 
{
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

