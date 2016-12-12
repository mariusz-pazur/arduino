#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include <dht_nonblocking.h>
#include "printf.h"
#include "hardware.h"

#define arr_len( x )  ( sizeof( x ) / sizeof( *x ) )

static uint8_t myAddress[] =  {0x4C, 0x44, 0x53, 0x5A, 0x31}; //LDSZ1
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
static byte currentDayLeds[][2] = { {0, 2}, {2, 4}, {4, 6},
                                   {7, 9}, {9, 11}, {11, 13},
                                   {14, 16}, {16, 18}, {18, 20},
                                   {21, 23}, {23, 25}, {25, 27},
                                   {53, 55}, {51, 53}, {49, 51},
                                   {46, 48}, {44, 46}, {42, 44},
                                   {39, 41}, {37, 39}, {35, 37},
                                   {32, 34}, {30, 32}, {28, 30}
                                  };
uint32_t currentColorBoxesFill[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
byte boxLightOrder[] = {0, 1, 2, 3, 4, 5, 6, 7};
static byte pixelsInOneBox = 7;
static byte numberOfBoxes = 8;
uint32_t colorBoxesLastRun = 0;
byte indexOfBoxToFill = 0;
uint32_t colorBoxesNextChangeInMillis = 1000;

const byte ledsPin = 6;
const byte ledsNumber = 56;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(ledsNumber, ledsPin, NEO_GRB + NEO_KHZ800);
uint8_t rainbow_j = 0;
uint32_t ledsThreadLastRun = 0;
const uint32_t ledsThreadDelayInMillis = 50;
bool isLedsChanging = true;
const uint8_t knightRiderWidth = 16;
uint32_t old_val[ledsNumber];
int8_t knightRiderCurrentPixel = 0;
int8_t knightRiderDirection = 1;

const byte dhtPin = 2;
DHT_nonblocking dht(dhtPin, DHT_TYPE_22);
const uint32_t dhtThreadDelayInMillis = 30000;
uint32_t dhtThreadLastRun = 0;

#define HA_REMOTE_LEDDST_LOOP_DEBUG 0
#define HA_REMOTE_LEDDST_NRF_DEBUG 0
#define HA_REMOTE_LEDDST_LEDS_DEBUG 0
#define HA_REMOTE_LEDDST_DHT_DEBUG 0

void setup(void)
{
#if HA_REMOTE_LEDDST_LOOP_DEBUG || HA_REMOTE_LEDDST_NRF_DEBUG || HA_REMOTE_LEDDST_LEDS_DEBUG || HA_REMOTE_LEDDST_DHT_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led Distance (leds & temp & humidity & distance sensor)\n\r");
#endif
  setupRF();   
  setupLeds();  
#if HA_REMOTE_LEDDST_LOOP_DEBUG || HA_REMOTE_LEDDST_NRF_DEBUG || HA_REMOTE_LEDDST_LEDS_DEBUG || HA_REMOTE_LEDDST_DHT_DEBUG
  printf("Free RAM: %d B\n\r", freeRam()); 
#endif  
  randomSeed(analogRead(0));
  response[4] = 2;
  response[5] = 0;
  response[6] = 44;
  response[7] = 44;
  response[8] = 5;
  response[9] = 180;
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

void loop() 
{
  if (hasMainIntervalGone())
  {
    mainCallback();
    mainThreadLastRun = millis();
#if HA_REMOTE_LEDDST_LOOP_DEBUG && HA_REMOTE_LEDDST_NRF_DEBUG
    printf("Main: %ld \n\r", mainThreadLastRun); 
#endif
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsThreadLastRun = millis();
#if HA_REMOTE_LEDDST_LOOP_DEBUG && HA_REMOTE_LEDDST_LEDS_DEBUG
    printf("LEDs: %ld \n\r", ledsThreadLastRun); 
#endif
  }   
  /*if (hasDhtIntervalGone())
  {
    if (dhtCallback())
    {
      dhtThreadLastRun = millis();
#if HA_REMOTE_LEDDST_LOOP_DEBUG && HA_REMOTE_LEDDST_DHT_DEBUG
      printf("DHT: %ld \n\r", dhtThreadLastRun); 
#endif
    }
  } */     
}


void mainCallback()
{  
  checkForCommandArrived();  
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
#if HA_REMOTE_LEDDST_NRF_DEBUG
    printf("Read command from radio {");
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      printf("%d,", command[i]);
    }
    printf("}\n\r");
#endif 
    if (command[1] == 4)//TYPE-RemoteLEDDST
    {
      for (byte i = 0; i < 4; i++)
      {
        response[i] = command[i];
      }
      if (command[2] == 0) //read state
      {
        //4,5,6,7,8,9-Leds
        //10,11,12,13-DHT        
      }
      else if (command[2] == 1) //LEDS
      {         
        isLedsChanging = true;              
        for (byte i = 4; i < 9; i++)
        {
          response[i] = command[i];
        }
      }              
      Mirf.send(response);
#if HA_REMOTE_LEDDST_NRF_DEBUG
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
  #if HA_REMOTE_LEDDST_DHT_DEBUG
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
#if HA_REMOTE_LEDDST_DHT_DEBUG
    printf("DHT measure error\n\r");
#endif
  }
  return measureResult;
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
#if HA_REMOTE_LEDDST_NOISE_DEBUG
  printf("Sum: %ld, Mean: %d\n\r", sum, mean);
#endif
  return mean;
}

void ledsCallback()
{  
  if (isLedsChanging)
  {
    if (response[4] == 0) //OFF
    {      
#if HA_REMOTE_LEDDST_LEDS_DEBUG
      printf("Leds Off\n\r");
#endif
      colorWipe(0, -1);  
      isLedsChanging = false;  
    }
    else if (response[4] == 1) //Effects
    {      
      if (response[5] == 0) //RainbowWheel
      {        
#if HA_REMOTE_LEDDST_LEDS_DEBUG
      printf("Rainbow Leds\n\r");
#endif
        rainbowLeds();        
      }      
      else if (response[5] == 2) //KnightRider
      {
        uint32_t colorToSet = strip.Color((response[9]*response[6])/255,(response[9]*response[7])/255,(response[9]*response[8])/255);
        knightRider(colorToSet);
      }
      else if (response[5] == 3) //Random Color Boxes
      {
#if HA_REMOTE_LEDDST_LEDS_DEBUG
      printf("Random Color Boxes\n\r");
#endif
        colorBoxes();
        
      }
    }
    else if (response[4] == 2) //Set Color
    {      
#if HA_REMOTE_LEDDST_LEDS_DEBUG
      printf("Set Color Leds\n\r");
#endif
      uint32_t colorToSet = strip.Color((response[9]*response[5])/255,(response[9]*response[6])/255,(response[9]*response[7])/255);
      colorWipe(colorToSet, response[8] - 1);      
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
void colorWipe(uint32_t c, int8_t currentDayIndex) 
{
  byte startCurrentDayIndex;
  byte endCurrentDayIndex;
  if (currentDayIndex >= 0 && currentDayIndex < arr_len(currentDayLeds))
  {
    startCurrentDayIndex = currentDayLeds[currentDayIndex][0];
    endCurrentDayIndex = currentDayLeds[currentDayIndex][1];
  }
  else
  {
    startCurrentDayIndex =-1;
    endCurrentDayIndex = -1;
  }
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      if (i >= startCurrentDayIndex && i <= endCurrentDayIndex)
        strip.setPixelColor(i, (response[9]*255)/255, 0, 0);
      else
        strip.setPixelColor(i, c);           
  }
  strip.show();
}

void knightRider(uint32_t color) 
{     
  if (knightRiderDirection > 0)
  {
    strip.setPixelColor(knightRiderCurrentPixel, color);
    old_val[knightRiderCurrentPixel] = color;
    for(int x = knightRiderCurrentPixel; x>0; x--) 
    {
      old_val[x-1] = dimColor(old_val[x-1], knightRiderWidth);
      strip.setPixelColor(x-1, old_val[x-1]);
    }
    strip.show(); 
    knightRiderCurrentPixel += knightRiderDirection;
    if (knightRiderCurrentPixel >= ledsNumber)
    {
      knightRiderCurrentPixel = ledsNumber-1;
      knightRiderDirection = -knightRiderDirection;
    }       
  }
  else
  { 
    strip.setPixelColor(knightRiderCurrentPixel, color);
    old_val[knightRiderCurrentPixel] = color;
    for(int x = knightRiderCurrentPixel; x<=ledsNumber ;x++) 
    {
      old_val[x-1] = dimColor(old_val[x-1], knightRiderWidth);
      strip.setPixelColor(x+1, old_val[x+1]);
    }
    strip.show(); 
    knightRiderCurrentPixel += knightRiderDirection;
    if (knightRiderCurrentPixel < 0)
    {
      knightRiderCurrentPixel = 0;
      knightRiderDirection = -knightRiderDirection;
    }           
  }
}

void colorBoxes()
{
  if (hasIntervalGone(colorBoxesLastRun, colorBoxesNextChangeInMillis))
  {
#if HA_REMOTE_LEDDST_LEDS_DEBUG
    printf("Index Of Box To Fill - %d\n\r", indexOfBoxToFill);
#endif
    if (indexOfBoxToFill == numberOfBoxes)
    {
      colorWipe(0, -1);
      indexOfBoxToFill = 0;
    }
    else
    {
      if (indexOfBoxToFill == 0)
      {
        for (int i=0; i < numberOfBoxes; i++)
        {
          long r = random(i, numberOfBoxes); 
          int temp = boxLightOrder[i];
          boxLightOrder[i] = boxLightOrder[r];
          boxLightOrder[r] = temp;
        }
        for (int i = 0; i < numberOfBoxes; i++)
        {
          currentColorBoxesFill[i] = 0;
        }      
      }
      currentColorBoxesFill[boxLightOrder[indexOfBoxToFill]] = Wheel(random(1, 255));    
  
      for(uint16_t i=0; i<strip.numPixels(); i++) 
      {
        int boxIndex = i / pixelsInOneBox;
        strip.setPixelColor(i, currentColorBoxesFill[boxIndex]);                 
      }
      strip.show();      
      indexOfBoxToFill++;
    }
    colorBoxesLastRun = millis();
  }
}

uint32_t dimColor(uint32_t color, uint8_t width) 
{
  return (((color&0xFF0000)/width)&0xFF0000) + (((color&0x00FF00)/width)&0x00FF00) + (((color&0x0000FF)/width)&0x0000FF);
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
