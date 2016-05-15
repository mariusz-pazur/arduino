#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include <dht_nonblocking.h>
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
static byte response[] = {0,0,0,0,//ID,TYPE,COMMAND,0 - 0-3
                         0,0,0,0,0,0,//LEDS - 4-8,BRIGHTNESS-9
                         0,0,0,0,//DHT22 - 10-13
                         0,0};//NOISE_VALUE,NOISE_BRIGHTNESS - 14-15
const uint32_t mainThreadDelayInMillis = 2;
uint32_t mainThreadLastRun = 0;

byte ledsPin = 3;
byte ledsNumber = 7;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(ledsNumber, ledsPin, NEO_GRB + NEO_KHZ800);
static uint8_t nightModePin = 7;
static uint8_t offlineEnablePin = 8;
byte previousBrightness = 0;
byte currentNightModeState = 0; 
byte enableState = 0;
byte previousEnableState = 0;
uint8_t rainbow_j = 0;
uint32_t ledsThreadLastRun = 0;
uint32_t ledsThreadDelayInMillis = 50;
bool isLedsChanging = false;

byte dhtPin = 4;
DHT_nonblocking dht(dhtPin, DHT_TYPE_22);
const uint32_t dhtThreadDelayInMillis = 30000;
uint32_t dhtThreadLastRun = 0;

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

#define HA_REMOTE_LEDENV_LOOP_DEBUG 0
#define HA_REMOTE_LEDENV_NRF_DEBUG 1
#define HA_REMOTE_LEDENV_LEDS_DEBUG 0
#define HA_REMOTE_LEDENV_DHT_DEBUG 0
#define HA_REMOTE_LEDENV_NOISE_DEBUG 1

void setup(void)
{
  pinMode(nightModePin, INPUT);
  pinMode(offlineEnablePin, INPUT);
#if HA_REMOTE_LEDENV_LOOP_DEBUG || HA_REMOTE_LEDENV_NRF_DEBUG || HA_REMOTE_LEDENV_LEDS_DEBUG || HA_REMOTE_LEDENV_DHT_DEBUG || HA_REMOTE_LEDENV_NOISE_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led Environment (leds & temp & humidity & noise)\n\r");
#endif
  setupRF();   
  setupLeds();
  setupNoiseSensor();
#if HA_REMOTE_LEDENV_LOOP_DEBUG || HA_REMOTE_LEDENV_NRF_DEBUG || HA_REMOTE_LEDENV_LEDS_DEBUG || HA_REMOTE_LEDENV_DHT_DEBUG || HA_REMOTE_LEDENV_NOISE_DEBUG
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
  previousBrightness = 255;
  response[9] = 255;//brightness
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
#if HA_REMOTE_LEDENV_LOOP_DEBUG && HA_REMOTE_LEDENV_NRF_DEBUG
    printf("Main: %ld \n\r", mainThreadLastRun); 
#endif
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsThreadLastRun = millis();
#if HA_REMOTE_LEDENV_LOOP_DEBUG && HA_REMOTE_LEDENV_LEDS_DEBUG
    printf("LEDs: %ld \n\r", ledsThreadLastRun); 
#endif
  }   
  if (hasDhtIntervalGone())
  {
    if (dhtCallback())
    {
      dhtThreadLastRun = millis();
#if HA_REMOTE_LEDENV_LOOP_DEBUG && HA_REMOTE_LEDENV_DHT_DEBUG
      printf("DHT: %ld \n\r", dhtThreadLastRun); 
#endif
    }
  }    
  if (hasNoiseForLedsIntervalGone())
  {
    noiseCallback();
    noiseForLedsLastRead = millis();
#if HA_REMOTE_LEDENV_LOOP_DEBUG && HA_REMOTE_LEDENV_NOISE_DEBUG
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
  if (response[15] == 0)
  {
    previousBrightness = response[9];
    currentNightModeState = digitalRead(nightModePin);          
    if (currentNightModeState == HIGH)
      response[9] = 64;
    else
      response[9] = 255;
    if (response[9] != previousBrightness)
      isLedsChanging = true;
  }
}

void checkForEnableLeds()
{
  previousEnableState = enableState;
  enableState = digitalRead(offlineEnablePin);
  if (previousEnableState == HIGH && enableState == LOW)
  {      
    response[4] = 0;
    isLedsChanging = true;
  }
  else if (previousEnableState == LOW && enableState == HIGH)
  {
    response[4] = 1;
    response[5] = 0;
    isLedsChanging = true;
  }
}

void checkForCommandArrived()
{
  if(!Mirf.isSending() && Mirf.dataReady())
  {       
    bool done = false;
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      command[i] = 0;
    }
    Mirf.getData(command);       
#if HA_REMOTE_LEDENV_NRF_DEBUG
    printf("Read command from radio {");
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      printf("%d,", command[i]);
    }
    printf("}\n\r");
#endif 
    if (command[1] == 2)//TYPE-RemoteLedEnv
    {
      for (byte i = 0; i < 4; i++)
      {
        response[i] = command[i];
      }
      if (command[2] == 0) //read state
      {
        //4,5,6,7,8,9-Leds
        //10,11,12,13-DHT
        //14-Noise
      }
      else if (command[2] == 1) //LEDS
      {         
        isLedsChanging = true;              
        for (byte i = 4; i < 9; i++)
        {
          response[i] = command[i];
        }
      }
      else if (command[2] == 2) //Noise
      {
        response[15] = command[15];
      }           
      Mirf.send(response);
#if HA_REMOTE_LEDENV_NRF_DEBUG
      printf("Sent state response {");
      for (byte i = 0; i < commandAndResponseLength; i++)
      {
        printf("%d,", response[i]);
      }
      printf("}\n\r");
#endif  
    }    
  }
}     

bool dhtCallback()
{
  float t;  
  float h;  
  bool measureResult = dht.measure(&t, &h);
  if(measureResult)
  {    
    int16_t temp = (int16_t)(t*100);
    int16_t humid = (int16_t)(h*100);
    response[10] = (temp & 0xFF00) >> 8;  
    response[11] = (temp & 0x00FF);
    response[12] = (humid & 0xFF00) >> 8;  
    response[13] = (humid & 0x00FF);
  #if HA_REMOTE_LEDENV_DHT_DEBUG
    char str_temp[6];
    dtostrf(t, 4, 2, str_temp);
    char str_humid[6];
    dtostrf(h, 4, 2, str_humid);
    printf("Temp: %s C\n\r", str_temp);
    printf("Humid: %s %%\n\r", str_humid);
  #endif  
  }  
  else
  {
#if HA_REMOTE_LEDENV_DHT_DEBUG
    printf("DHT measure error\n\r");
#endif
  }
  return measureResult;
}

void noiseCallback()
{
  int sensorValue = analogRead(A1);
#if HA_REMOTE_LEDENV_NOISE_DEBUG
  printf("Noise:%d\n\r", sensorValue);
#endif
  byte noise = map(sensorValue, 0, 1023, 255, 0);
  if (hasNoiseIntervalGone())
  {
    noiseReadsBuffer[noiseBufferIndex] = noise;
#if HA_REMOTE_LEDENV_NOISE_DEBUG
    printf("Noise_M[%d]:%d\n\r", noiseBufferIndex, noiseReadsBuffer[noiseBufferIndex]);
#endif
    noiseBufferIndex++;
    if(noiseBufferIndex == noiseReadsBufferLength)
    {
      noiseBufferIndex = 0;
      response[14] = calculateMeanNoiseSensorValue();
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
#if HA_REMOTE_LEDENV_NOISE_DEBUG
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
  if (response[15] == 1)
  {
    response[9] = calculateMeanNoiseForLedsValue();
    isLedsChanging = true;
  }  
  if (isLedsChanging)
  {
    if (response[4] == 0) //OFF
    {      
#if HA_REMOTE_LEDENV_LEDS_DEBUG
      printf("Leds Off\n\r");
#endif
      colorWipe(0);  
      isLedsChanging = false;  
    }
    else if (response[4] == 1) //Effects
    {      
      if (response[5] == 0) //RainbowWheel
      {        
#if HA_REMOTE_LEDENV_LEDS_DEBUG
      printf("Rainbow Leds\n\r");
#endif
        rainbowLeds();        
      }
      else if (response[5] == 1) //NoiseColor
      {
        colorWipe(Wheel(calculateMeanNoiseForLedsValue()));        
      }
    }
    else if (response[4] == 2) //Set Color
    {      
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Set Color Leds\n\r");
#endif
      uint32_t colorToSet = strip.Color((response[9]*response[5])/255,(response[9]*response[6])/255,(response[9]*response[7])/255);
      colorWipe(colorToSet);      
      isLedsChanging = false;      
    } 
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
    return strip.Color((response[9] * WheelPos * 3)/255, (response[9] * (255 - WheelPos * 3))/255, 0);
  } 
  else if(WheelPos < 170) 
  {
    WheelPos -= 85;
    return strip.Color((response[9] * (255 - WheelPos * 3))/255, 0, (response[9] * WheelPos * 3)/255);
  } 
  else 
  {
    WheelPos -= 170;
    return strip.Color(0, (response[9] * WheelPos * 3)/255, (response[9] * (255 - WheelPos * 3))/255);
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
  return hasIntervalGone(ledsThreadLastRun, ledsThreadDelayInMillis);
}

bool hasDhtIntervalGone()
{
  return hasIntervalGone(dhtThreadLastRun, dhtThreadDelayInMillis);
}

bool hasNoiseIntervalGone()
{
  return hasIntervalGone(noiseSensorLastRead, noiseSensorReadsDelayInMillis);
}

bool hasNoiseForLedsIntervalGone()
{
  return hasIntervalGone(noiseForLedsLastRead, noiseForLedsReadsDelayInMillis);
}


