#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include "DHT.h"
#include "printf.h"
#include "hardware.h"
#include "aes256.h"
#include "crypto.h"

static uint8_t myAddress[] =  {0xF0, 0xF0, 0xF0, 0xF0, 0xD3};
static uint8_t mainAddress[] = {0xF0, 0xF0, 0xF0, 0xF0, 0xE1 };
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;
static uint8_t commandAndResponseLength = 16;
static byte command[] = {0,0,0,0,
                         0,0,0,0,
                         0,0,0,0,
                         0,0,0,0};
uint32_t mainThreadDelayInMillis = 5;
uint32_t mainThreadLastRun = 0;

byte ledsPin = 3;
uint8_t stateLength = 4;
uint8_t state[] = { 0, 0, 0, 0};
uint8_t ledState[] = { 0, 0 };
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, ledsPin, NEO_GRB + NEO_KHZ800);
static uint8_t brightness = 255;
static uint8_t nightModePin = 7;
static uint8_t offlineEnablePin = 8;
byte isInNightModeState = 0; 
byte enableState = 0;
byte previousEnableState = 0;
uint8_t rainbow_j = 0;
uint32_t ledsLastControl = 0;
uint32_t ledsControlDelayInMillis = 50;

aes256_context ctxt;
//uint8_t cryptoKey[] = { // set this in crypto.h
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
//    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
//    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
//    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
//  };  

byte dhtPin = 4;
DHT dht(dhtPin, DHT11);
uint32_t dhtSensorLastRead = 0;
uint32_t dhtSensorReadsDelayInMillis = 30000;

const uint16_t noiseReadsBufferLength = 60;
uint16_t noiseBufferIndex = 0;
uint8_t noiseReadsBuffer[noiseReadsBufferLength];
uint32_t noiseSensorReadsDelayInMillis = 60000 / noiseReadsBufferLength;
uint32_t noiseSensorLastRead = 0;

#define HA_REMOTE_LEDENV_DEBUG 0

void setup(void)
{
  pinMode(nightModePin, INPUT);
  pinMode(offlineEnablePin, INPUT);
#if HA_REMOTE_LEDENV_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led Environment (leds & temp & humidity & noise)\n\r");
#endif
  setupRF(); 
  setupEncryption();
  setupLeds();
  setupNoiseSensor();
#if HA_REMOTE_LEDENV_DEBUG
  printf("Free RAM: %d B\n\r", freeRam()); 
#endif
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

void setupNoiseSensor()
{
  for (uint16_t i = 0; i < noiseReadsBufferLength; i++)
  {
    noiseReadsBuffer[i] = 0;
  }  
}

void loop() 
{
  if (hasMainIntervalGone())
  {
    mainCallback();
    mainThreadLastRun = millis();
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsLastControl = millis();
  }
  if (hasMainIntervalGone())
  {
    mainCallback();
    mainThreadLastRun = millis();
  }
  if (hasDHTIntervalGone())
  {
    dhtCallback();
    dhtSensorLastRead = millis();
  }
  if (hasMainIntervalGone())
  {
    mainCallback();
    mainThreadLastRun = millis();
  }
  if (hasNoiseIntervalGone())
  {
    noiseCallback();
    noiseSensorLastRead = millis();
  }
}


void mainCallback()
{
  checkForBrightnessChange();
  checkForEnableLeds();
  checkForCommandArrived();  
}

void checkForBrightnessChange()
{
  isInNightModeState = digitalRead(nightModePin);
    if (isInNightModeState == HIGH)
      brightness = 64;
    else
      brightness = 255;
}

void checkForEnableLeds()
{
  previousEnableState = enableState;
  enableState = digitalRead(offlineEnablePin);
  if (previousEnableState == HIGH && enableState == LOW)
  {      
    ledState[0] = 1;
    ledState[1] = 0;
  }
  else if (previousEnableState == LOW && enableState == HIGH)
  {
    ledState[0] = 2;
    ledState[1] = 0;
  }
}

void checkForCommandArrived()
{
  if(!Mirf.isSending() && Mirf.dataReady())
  {
    byte response[commandAndResponseLength];    
    bool done = false;
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      command[i] = 0;
    }
    Mirf.getData(command);  
    aes256_decrypt_ecb(&ctxt, command);   
#if HA_REMOTE_LEDENV_DEBUG
    printf("Read command from radio {%d,%d,%d}\n\r", command[1],command[2],command[3]);
#endif 
    if (command[1] == 2)//RemoteLedEnv
    {
      if (command[2] == 1) //LEDs Off
      {
        ledState[0] =  command[2];
        ledState[1] = state[3] = command[3];        
      }
      else if (command[2] == 2) //LEDs Effect
      {
        ledState[0] = command[2];
        ledState[1] = command[3];
        state[3] = 1;
      }
      else if (command[2] == 3) //Leds set color
      {
        ledState[0] = command[2];
        ledState[1] = command[3];
        state[3] = 2;
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
  }
}     

void dhtCallback()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
#if HA_REMOTE_LEDENV_DEBUG
  char str_temp[6];
  dtostrf(t, 4, 2, str_temp);
  char str_humid[6];
  dtostrf(h, 4, 2, str_humid);
  printf("Temp: %s C\n\r", str_temp);
  printf("Humid: %s %\n\r", str_humid);
#endif         
  state[0] = byte(t);
  state[1] = byte(h);         
}

void noiseCallback()
{
  int sensorValue = analogRead(A1);
#if HA_REMOTE_LEDENV_DEBUG
  printf("Noise:%d\n\r", sensorValue);
#endif
  byte noise = map(sensorValue, 0, 1023, 255, 0);
  noiseReadsBuffer[noiseBufferIndex] = noise;
#if HA_REMOTE_LEDENV_DEBUG
   printf("Noise_M[%d]:%d\n\r", noiseBufferIndex, noiseReadsBuffer[noiseBufferIndex]);
#endif
   noiseBufferIndex++;
   if(noiseBufferIndex == noiseReadsBufferLength)
   {
     noiseBufferIndex = 0;
     state[2] = calculateMeanNoiseSensorValue();
   }
}

byte calculateMeanNoiseSensorValue()
{
  uint32_t sum = 0;
  byte mean;
  for (uint16_t i = 0; i < noiseReadsBufferLength; i++)
  {
    sum += noiseReadsBuffer[i];
  }
  mean = sum / noiseReadsBufferLength;
#if HA_REMOTE_LEDENV_DEBUG
  printf("Sum: %ld, Mean: %d\n\r", sum, mean);
#endif
  return mean;
}

void ledsCallback()
{
  if (ledState[0] == 1 && ledState[1] == 0) //OFF
  {
    colorWipe(0);
    state[3] = 0;
  }
  else if (ledState[0] == 2) //Effects
  {
    if (ledState[1] == 0) //RainbowWheel
    {
      rainbowLeds();
      state[3] = 1;
    }
  }
  else if (ledState[0] == 3) //Set Color
  {
    uint32_t colorToSet;
    if (ledState[1] == 255)
      colorToSet = strip.Color((brightness*255)/255,(brightness*255)/255,(brightness*255)/255);
    else
      colorToSet = Wheel(ledState[1]);
    colorWipe(colorToSet);
    state[3] = 2;
  }
}

void rainbowLeds() 
{
  rainbow_j++;
  for(int i = 0; i< strip.numPixels(); i++) 
  {
    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + rainbow_j) & 255));            
  }
  strip.show();    
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) 
{
  if(WheelPos < 85) 
  {
    return strip.Color((brightness * WheelPos * 3)/255, (brightness * (255 - WheelPos * 3))/255, 0);
  } 
  else if(WheelPos < 170) 
  {
    WheelPos -= 85;
    return strip.Color((brightness * (255 - WheelPos * 3))/255, 0, (brightness * WheelPos * 3)/255);
  } 
  else 
  {
    WheelPos -= 170;
    return strip.Color(0, (brightness * WheelPos * 3)/255, (brightness * (255 - WheelPos * 3))/255);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c) 
{
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      strip.setPixelColor(i, c);           
  }
  strip.show();
}

bool hasIntervalGone(uint32_t lastRead, uint32_t millisBetweenReads)
{
  return (lastRead + millisBetweenReads) <= millis();
}

bool hasMainIntervalGone()
{
  return hasIntervalGone(mainThreadLastRun, mainThreadDelayInMillis);
}

bool hasLedsIntervalGone()
{
  return hasIntervalGone(ledsLastControl, ledsControlDelayInMillis);
}

bool hasDHTIntervalGone()
{
  return hasIntervalGone(dhtSensorLastRead, dhtSensorReadsDelayInMillis);
}

bool hasNoiseIntervalGone()
{
  return hasIntervalGone(noiseSensorLastRead, noiseSensorReadsDelayInMillis);
}


