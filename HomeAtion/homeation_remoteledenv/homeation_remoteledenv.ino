#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include "DHT.h"
#include "printf.h"
#include "hardware.h"

static uint8_t myAddress[] =  {0x4C, 0x44, 0x45, 0x56, 0x31}; //LDEV1
static uint8_t mainAddress[] = {0x52, 0x50, 0x49, 0x32, 0x34 }; //RPI24
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;
static uint8_t commandAndResponseLength = 16;
static byte command[] = {0,0,0,0,
                         0,0,0,0,
                         0,0,0,0,
                         0,0,0,0};
const uint32_t mainThreadDelayInMillis = 2;
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

byte dhtPin = 4;
DHT dht(dhtPin, DHT11);

const uint16_t noiseReadsBufferLength = 600;
uint16_t noiseBufferIndex = 0;
uint8_t noiseReadsBuffer[noiseReadsBufferLength];
uint32_t noiseSensorReadsDelayInMillis = 60000 / noiseReadsBufferLength;
uint32_t noiseSensorLastRead = 0;

const uint8_t noiseForLedsBufferLength = 200;
uint8_t noiseForLedsBufferIndex = 0;
uint8_t noiseForLedsBuffer[noiseForLedsBufferLength];
uint16_t noiseForLedsReadsDelayInMillis = 5;
uint32_t noiseForLedsLastRead = 0;
bool noiseChangeBrightness = false;

#define HA_REMOTE_LEDENV_DEBUG 0
#define HA_REMOTE_LEDENV_NRF_DEBUG 0

void setup(void)
{
  pinMode(nightModePin, INPUT);
  pinMode(offlineEnablePin, INPUT);
#if HA_REMOTE_LEDENV_NRF_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led Environment (leds & temp & humidity & noise)\n\r");
#endif
  setupRF();   
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
  Mirf.configRegister( SETUP_RETR, 0b11111);
  Mirf.config();
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
#if HA_REMOTE_LEDENV_DEBUG
    printf("Main: %ld \n\r", mainThreadLastRun); 
#endif
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsLastControl = millis();
#if HA_REMOTE_LEDENV_DEBUG
    printf("LEDs: %ld \n\r", ledsLastControl); 
#endif
  }    
  if (hasNoiseForLedsIntervalGone())
  {
    noiseCallback();
    noiseForLedsLastRead = millis();
#if HA_REMOTE_LEDENV_DEBUG
    printf("NoiseInt: %ld \n\r", noiseForLedsLastRead); 
#endif
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
#if HA_REMOTE_LEDENV_NRF_DEBUG
    printf("Read command from radio {%d,%d,%d}\n\r", command[1],command[2],command[3]);
#endif 
    if (command[1] == 2)//RemoteLedEnv
    {
      if (command[2] == 0)
      {
        readDht();
      }
      if (command[2] == 1) //LEDs Off
      {
        noiseChangeBrightness = false;
        ledState[0] = command[2];
        ledState[1] = state[3] = command[3];        
      }
      else if (command[2] == 2) //LEDs Effect
      {
        noiseChangeBrightness = false;
        ledState[0] = command[2];
        ledState[1] = command[3];
        state[3] = 1;
      }
      else if (command[2] == 3) //Leds set color
      {
        noiseChangeBrightness = false;
        ledState[0] = command[2];
        ledState[1] = command[3];
        state[3] = 2;
      }
      else if (command[2] == 4) //Noise Changes brightness
      {
        noiseChangeBrightness = true;
      }
      for (int i = 0; i < commandAndResponseLength; i++)
      {
        if (i < stateLength)
          response[i] = state[i];    
        else
          response[i] = 0;
      }      
      Mirf.send(response);
#if HA_REMOTE_LEDENV_NRF_DEBUG
      printf("Sent state response {%d,%d,%d,%d}\n\r", state[0],state[1],state[2],state[3]);
#endif  
    }    
  }
}     

void readDht()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
#if HA_REMOTE_LEDENV_DEBUG
  char str_temp[6];
  dtostrf(t, 4, 2, str_temp);
  char str_humid[6];
  dtostrf(h, 4, 2, str_humid);
  printf("Temp: %s C\n\r", str_temp);
  printf("Humid: %s %%\n\r", str_humid);
#endif         
  state[0] = byte(t);
  state[1] = byte(h);  
#if HA_REMOTE_LEDENV_DEBUG
  printf("Free RAM: %d B\n\r", freeRam()); 
#endif       
}

void noiseCallback()
{
  int sensorValue = analogRead(A1);
#if HA_REMOTE_LEDENV_DEBUG
  printf("Noise:%d\n\r", sensorValue);
#endif
  byte noise = map(sensorValue, 0, 1023, 255, 0);
  if (hasNoiseIntervalGone())
  {
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
  noiseForLedsBuffer[noiseForLedsBufferIndex] = noise;
  noiseForLedsBufferIndex++;
  if (noiseForLedsBufferIndex == noiseForLedsBufferLength)
  {
    noiseForLedsBufferIndex = 0;
  }
}

byte calculateMeanBufferValue(byte buf[], uint16_t bufLength)
{
  uint32_t sum = 0;
  byte mean;
  for (uint16_t i = 0; i < bufLength; i++)
  {
    sum += buf[i];
  }
  mean = sum / bufLength;
#if HA_REMOTE_LEDENV_DEBUG
  printf("Sum: %ld, Mean: %d\n\r", sum, mean);
#endif
  return mean;
}

byte calculateMeanNoiseForLedsValue()
{
  return calculateMeanBufferValue(noiseForLedsBuffer, noiseForLedsBufferLength);    
}

byte calculateMeanNoiseSensorValue()
{
  return calculateMeanBufferValue(noiseReadsBuffer, noiseReadsBufferLength);  
}

void ledsCallback()
{
  if (noiseChangeBrightness)
    brightness = calculateMeanNoiseForLedsValue();
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
    else if (ledState[1] == 1) //NoiseColor
    {
      colorWipe(Wheel(calculateMeanNoiseForLedsValue()));
      state[3] = 2;
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
    state[3] = 3;
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

bool hasNoiseIntervalGone()
{
  return hasIntervalGone(noiseSensorLastRead, noiseSensorReadsDelayInMillis);
}

bool hasNoiseForLedsIntervalGone()
{
  return hasIntervalGone(noiseForLedsLastRead, noiseForLedsReadsDelayInMillis);
}


