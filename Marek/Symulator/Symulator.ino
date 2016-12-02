#include "printf.h"
#include "hardware.h"
#include <Adafruit_NeoPixel.h>
#include <dht_nonblocking.h>

typedef struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColor;

typedef struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
} HsvColor;

int pirPin = 8;  
int currentMove = LOW;
int previousMove = LOW; 
uint32_t pirThreadLastRun = 0;
uint32_t pirThreadDelayInMillis = 200;

byte ledsPin = 2;
byte modePin = 4;
byte ledsNumber = 10;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(ledsNumber, ledsPin, NEO_GRB + NEO_KHZ800);
uint8_t rainbow_j = 0;
uint32_t countingStartTimeInMillis;
uint32_t ledsThreadLastRun = 0;
uint32_t ledsThreadDelayInMillis = 50;
bool isLedsChanging = false;
byte brightness = 255;
RgbColor colorForLeds;
HsvColor colorFromTempAndLight;

byte dhtPin = 3;
DHT_nonblocking dht(dhtPin, DHT_TYPE_22);
const uint32_t dhtThreadDelayInMillis = 5000;
uint32_t dhtThreadLastRun = 0;
float temp;
float humid;


byte photoPin = A1;
const uint32_t photoThreadDelayInMillis = 200;
uint32_t photoThreadLastRun = 0;
int photoValue;

#define LOOP_DEBUG 0
#define PIR_DEBUG 0
#define LEDS_DEBUG 0
#define DHT_DEBUG 0
#define PHOTO_DEBUG 0

void setupLeds()
{
  strip.begin();
  strip.show();
  colorFromTempAndLight.h = 255;
  colorFromTempAndLight.s = 127;
  colorFromTempAndLight.v = 255;
}

void setup()
{
#if LOOP_DEBUG || PIR_DEBUG || LEDS_DEBUG || DHT_DEBUG || PHOTO_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("Symulator. Free RAM: %d B\n\r", freeRam()); 
#endif
  pinMode(pirPin, INPUT);
  pinMode(modePin, INPUT);
  setupLeds();   
}
 
void loop()
{
  if (hasPirIntervalGone())
  {
    pirCallback();
    pirThreadLastRun = millis();
#if LOOP_DEBUG && PIR_DEBUG
    printf("Pir: %ld \n\r", pirThreadLastRun); 
#endif
  }
  if (hasDhtIntervalGone())
  {
    if (dhtCallback())
    {
      dhtThreadLastRun = millis();
#if LOOP_DEBUG && DHT_DEBUG
      printf("DHT: %ld \n\r", dhtThreadLastRun); 
#endif
    }
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsThreadLastRun = millis();
#if LOOP_DEBUG && LEDS_DEBUG
    printf("LEDs: %ld \n\r", ledsThreadLastRun); 
#endif
  }  
  if (hasPhotoIntervalGone())
  {
    photoCallback();
    photoThreadLastRun = millis();
#if LOOP_DEBUG && PHOTO_DEBUG
    printf("Photo: %ld \n\r", photoThreadLastRun); 
#endif
  } 
}

void photoCallback()
{
  photoValue = analogRead(photoPin);
  brightness = map(photoValue, 0, 1024, 255, 0);
  colorFromTempAndLight.v = brightness;
#if PHOTO_DEBUG
  printf("PhotoV: %d;Bright:%d\n\r", photoValue, brightness);
#endif
}

void pirCallback()
{
  previousMove = currentMove;
  currentMove = digitalRead(pirPin);
#if PIR_DEBUG
  if (currentMove == HIGH)
  {
    if (previousMove == LOW)
    {
      printf("Pir: new move\n\r");
    }
    else
    {
      printf("Pir: move in progress\n\r");
    }
  }
  else
  {
    if (previousMove == HIGH)
    {
      printf("Pir: end of move\n\r");
    }
    else
    {
      printf("Pir: lack of move\n\r");
    }
  }
#endif
}

bool dhtCallback()
{
  bool measureResult = dht.measure(&temp, &humid);
  if(measureResult)
  { 
    int t = (int)temp;
    colorFromTempAndLight.h = map(t, 20, 30, 255, 0);       
  #if DHT_DEBUG
    char str_temp[6];
    dtostrf(temp, 4, 2, str_temp);
    char str_humid[6];
    dtostrf(humid, 4, 2, str_humid);
    printf("TempForCol: %d; Hue: %d", t, colorFromTempAndLight.h);
    printf("Temp: %s C\n\r", str_temp);
    printf("Humid: %s %%\n\r", str_humid);
  #endif  
  }  
  else
  {
#if DHT_DEBUG
    printf("DHT measure error\n\r");
#endif
  }
  return measureResult;
}

void ledsCallback()
{  
  if (currentMove == HIGH)
  {
    isLedsChanging = true;
    if (digitalRead(modePin) == LOW)
    {
#if LEDS_DEBUG
    printf("Rainbow\n\r");
#endif
      rainbowLeds();
    }
    else
    {
      colorForLeds = HsvToRgb(colorFromTempAndLight);      
      colorWipe(colorForLeds.r, colorForLeds.g, colorForLeds.b);
#if LEDS_DEBUG
    printf("Color: %d, %d, %d\n\r", colorForLeds.r, colorForLeds.g, colorForLeds.b);
#endif
    }
  }
  else if (isLedsChanging && currentMove == LOW)
  {
#if LEDS_DEBUG
    printf("Clear rainbow or color\n\r");
#endif
    colorWipe(0, 0, 0);
    isLedsChanging = false;
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
void colorWipe(byte r, byte g, byte b) 
{
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      strip.setPixelColor(i, r, g, b);           
  }
  strip.show();
}



RgbColor HsvToRgb(HsvColor hsv)
{
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;

    if (hsv.s == 0)
    {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

HsvColor RgbToHsv(RgbColor rgb)
{
    HsvColor hsv;
    unsigned char rgbMin, rgbMax;

    rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

    hsv.v = rgbMax;
    if (hsv.v == 0)
    {
        hsv.h = 0;
        hsv.s = 0;
        return hsv;
    }

    hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
    if (hsv.s == 0)
    {
        hsv.h = 0;
        return hsv;
    }

    if (rgbMax == rgb.r)
        hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
    else if (rgbMax == rgb.g)
        hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
    else
        hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

    return hsv;
}

bool hasIntervalGone(uint32_t lastRead, uint32_t millisBetweenReads)
{
  return (lastRead + millisBetweenReads) <= millis();
}

bool hasPirIntervalGone()
{
  return hasIntervalGone(pirThreadLastRun, pirThreadDelayInMillis);
}

bool hasDhtIntervalGone()
{
  return hasIntervalGone(dhtThreadLastRun, dhtThreadDelayInMillis);
}

bool hasLedsIntervalGone()
{
  return hasIntervalGone(ledsThreadLastRun, ledsThreadDelayInMillis);
}

bool hasPhotoIntervalGone()
{
  return hasIntervalGone(photoThreadLastRun, photoThreadDelayInMillis);
}
