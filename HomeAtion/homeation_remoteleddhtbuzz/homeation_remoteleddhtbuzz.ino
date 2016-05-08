#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <Adafruit_NeoPixel.h>
#include <dht_nonblocking.h>
#include "printf.h"
#include "hardware.h"

static uint8_t myAddress[] =  {0x4C, 0x44, 0x42, 0x5A, 0x31}; //LDBZ1
static uint8_t mainAddress[] = {0x52, 0x50, 0x49, 0x32, 0x34 }; //RPI24
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;
static uint8_t commandAndResponseLength = 16;
static byte command[] = {0,0,0,0,
                         0,0,0,0,
                         0,0,0,0,
                         0,0,0,0};
static byte response[] = {0,0,0,0,//ID,TYPE,COMMAND,0 - 0-3
                         0,0,0,0,0,0,//LEDS - 4-9
                         0,0,0,0,//DHT22 - 10-13
                         0,0};//BUZZER - 14-15
const uint32_t radioThreadDelayInMillis = 2;
uint32_t radioThreadLastRun = 0;

byte ledsPin = 2;
byte ledsNumber = 33;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(ledsNumber, ledsPin, NEO_GRB + NEO_KHZ800);
uint8_t rainbow_j = 0;
uint32_t countingStartTimeInMillis;
uint32_t ledsThreadLastRun = 0;
uint32_t ledsThreadDelayInMillis = 50;
bool isLedsChanging = false;
byte brightness = 255;

byte dhtPin = 7;
DHT_nonblocking dht(dhtPin, DHT_TYPE_22);
const uint32_t dhtThreadDelayInMillis = 30000;
uint32_t dhtThreadLastRun = 0;

byte buzzerPin = 6;
const uint32_t buzzerThreadDelayInMillis = 25;
uint32_t buzzerThreadLastRun = 0;
uint32_t lastNoteCall = 0;
byte lastNoteIndex = 0;
bool hasPlayStarted = false;
struct Song {  
  char notes[50];// = "cdfda ag cdfdg gf ";//X-kończy utwór
  uint8_t beats[50];// = ;
  uint8_t songLength;// = 18;
  uint8_t tempo;// = 100;
};
static Song songs[] = 
{    
    { { 'c', 'd', 'f', 'd', 'a', ' ', 'a', 'g', ' ', 'c', 'd', 'f', 'd', 'g', ' ', 'g', 'f', ' ', 'X' }, {1, 1, 1, 1, 1, 1, 4, 4, 2, 1, 1, 1, 1, 1, 1, 4, 4, 2}, 18, 100 }      
};

#define HA_REMOTE_LEDDHTBUZZ_LOOP_DEBUG 0
#define HA_REMOTE_LEDDHTBUZZ_NRF_DEBUG 1
#define HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG 1
#define HA_REMOTE_LEDDHTBUZZ_DHT_DEBUG 1
#define HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG 1

void setup(void)
{  
#if HA_REMOTE_LEDDHTBUZZ_NRF_DEBUG || HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG || HA_REMOTE_LEDDHTBUZZ_DHT_DEBUG || HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Led DHT & Sound (leds & temp & humidity & buzzer)\n\r");
#endif
  setupRF();   
  setupLeds();  
#if HA_REMOTE_LEDDHTBUZZ_NRF_DEBUG || HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG || HA_REMOTE_LEDDHTBUZZ_DHT_DEBUG || HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
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

void loop() 
{  
  if (hasRadioIntervalGone())
  {
    radioCallback();
    radioThreadLastRun = millis();
#if HA_REMOTE_LEDDHTBUZZ_LOOP_DEBUG
    printf("Radio: %ld \n\r", radioThreadLastRun); 
#endif
  }
  if (hasLedsIntervalGone())
  {
    ledsCallback();
    ledsThreadLastRun = millis();
#if HA_REMOTE_LEDDHTBUZZ_LOOP_DEBUG
    printf("LEDs: %ld \n\r", ledsThreadLastRun); 
#endif
  }  
  if (hasDhtIntervalGone())
  {
    if (dhtCallback())
    {
      dhtThreadLastRun = millis();
#if HA_REMOTE_LEDDHTBUZZ_LOOP_DEBUG
      printf("DHT: %ld \n\r", dhtThreadLastRun); 
#endif
    }
  }    
  if (hasBuzzerIntervalGone())
  {
    buzzerCallback();
    buzzerThreadLastRun = millis();
#if HA_REMOTE_LEDDHTBUZZ_LOOP_DEBUG
    printf("Buzzer: %ld \n\r", buzzerThreadLastRun); 
#endif
  }   
}

void radioCallback()
{
  if(!Mirf.isSending() && Mirf.dataReady())
  {
    bool done = false;
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      command[i] = 0;
    }
    Mirf.getData(command);       
#if HA_REMOTE_LEDDHTBUZZ_NRF_DEBUG
    printf("Read command from radio {");
    for (byte i = 0; i < commandAndResponseLength; i++)
    {
      printf("%d,", command[i]);
    }
    printf("}\n\r");
#endif 
    if (command[1] == 3)//TYPE-RemoteLedDhtBuzz
    {
      if (command[2] == 0) //DHT
      {
        
      }
      else if (command[2] == 1) //LEDS
      { 
        isLedsChanging = true; 
        countingStartTimeInMillis = millis();      
        for (byte i = 4; i < 10; i++)
        {
          response[i] = command[i];
        }
      }
      else if (command[2] == 2) //BUZZER
      {  
        lastNoteCall = 0;
        lastNoteIndex = 0;      
        for (byte i = 14; i < 16; i++)
        {
          response[i] = command[i];
        }
      }               
      Mirf.send(response);
#if HA_REMOTE_LEDDHTBUZZ_NRF_DEBUG
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
  #if HA_REMOTE_LEDDHTBUZZ_DHT_DEBUG
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
#if HA_REMOTE_LEDDHTBUZZ_DHT_DEBUG
    printf("DHT measure error\n\r");
#endif
  }
  return measureResult;
}

void ledsCallback()
{  
  if (isLedsChanging)
  {    
    if (response[4] == 0) //OFF
    {
      brightness = 255;
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Leds Off\n\r");
#endif
      colorWipe(0);  
      isLedsChanging = false;  
    }
    else if (response[4] == 1) //Effects
    {      
      if (response[5] == 0) //RainbowWheel
      {
        brightness = 100;
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Rainbow Leds\n\r");
#endif
        rainbowLeds();        
      }    
      else if (response[5] == 1) //Counting
      {
        brightness = 255;
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Counting Leds\n\r");
#endif
        countingLeds(response[6], response[7], response[8], response[9]);
      }
    }
    else if (response[4] == 2) //Set Color
    {
      brightness = 255;
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Set Color Leds\n\r");
#endif
      uint32_t colorToSet = strip.Color(response[5],response[6],response[7]);
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

void countingLeds(byte timeInMinutes, byte colorR, byte colorG, byte colorB) 
{
  uint16_t millisForLed = (uint16_t)((float)timeInMinutes*60*1000 /(float)ledsNumber);
  int16_t numberOfPixelsToShow = ledsNumber - (int16_t)((float)(millis()-countingStartTimeInMillis)/(float)millisForLed);
  uint32_t color = strip.Color(colorR, colorG, colorB);
  if (numberOfPixelsToShow < 0)
    numberOfPixelsToShow = 0;
#if HA_REMOTE_LEDDHTBUZZ_LEDS_DEBUG
      printf("Counting Leds - st:%ld,tm:%d,cr:%ld,ml:%d,np:%d\n\r", countingStartTimeInMillis, timeInMinutes, color, millisForLed, numberOfPixelsToShow);
#endif
  for(int i = 0; i< numberOfPixelsToShow; i++) 
  {
    strip.setPixelColor(i, color);            
  }
  for (int j = numberOfPixelsToShow; j < ledsNumber; j++)
  {
    strip.setPixelColor(j, 0);
  }
  strip.show(); 
  if (numberOfPixelsToShow == 0)
  {
    //response[14] = 1;
    response[4] = 0; 
  }
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

void buzzerCallback()
{
  int songIndex = response[14] - 1;
  if (response[14] > 0)
  {
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer On\n\r");
#endif
    int duration = songs[songIndex].beats[lastNoteIndex] * songs[songIndex].tempo;  // length of note/rest in ms
    if (!hasPlayStarted)
    {
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer Start Playing\n\r");
#endif
      hasPlayStarted = true;
      lastNoteIndex = 0;
      lastNoteCall = millis();
      tone(buzzerPin, frequency(songs[songIndex].notes[lastNoteIndex]), songs[songIndex].beats[lastNoteIndex] * songs[songIndex].tempo); 
    }
    else if (hasPlayStarted && (lastNoteCall + duration > millis()))
    {
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer Continue Playing\n\r");
#endif
      lastNoteIndex++;
      if (lastNoteIndex < songs[songIndex].songLength)
      {
        lastNoteCall = millis();
        if (songs[songIndex].notes[lastNoteIndex] == ' ')
        {
          //rest
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer Continue Playing - Rest\n\r");
#endif
        }
        else if (songs[songIndex].notes[lastNoteIndex] == 'X')
        {
          //end of song
          hasPlayStarted = false;
          response[14] = 0;
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer Continue Playing - End\n\r");
#endif
        }
        else
        {
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer Continue Playing - New Note\n\r");
#endif
          //new note
          tone(buzzerPin, frequency(songs[songIndex].notes[lastNoteIndex]), songs[songIndex].beats[lastNoteIndex] * songs[songIndex].tempo);
        }
      }
      else
      {
#if HA_REMOTE_LEDDHTBUZZ_BUZZER_DEBUG
    printf("Buzzer End Playing\n\r");
#endif
        //end of song
        hasPlayStarted = false;
        response[14] = 0;
      }
    }          
  }
}

int frequency(char note) 
{
  // This function takes a note character (a-g), and returns the
  // corresponding frequency in Hz for the tone() function.
  int i;
  const int numNotes = 8;  // number of notes we're storing

  // The following arrays hold the note characters and their
  // corresponding frequencies. The last "C" note is uppercase
  // to separate it from the first lowercase "c". If you want to
  // add more notes, you'll need to use unique characters.

  // For the "char" (character) type, we put single characters
  // in single quotes.

  char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
  int frequencies[] = {262, 294, 330, 349, 392, 440, 494, 523};

  // Now we'll search through the letters in the array, and if
  // we find it, we'll return the frequency for that note.

  for (i = 0; i < numNotes; i++)  // Step through the notes
  {
    if (names[i] == note)         // Is this the one?
    {
      return(frequencies[i]);     // Yes! Return the frequency
    }
  }
  return(0);  // We looked through everything and didn't find it,
              // but we still need to return a value, so return 0.
}

bool hasIntervalGone(uint32_t lastRead, uint32_t millisBetweenReads)
{
  return (lastRead + millisBetweenReads) <= millis();
}

bool hasRadioIntervalGone()
{
  return hasIntervalGone(radioThreadLastRun, radioThreadDelayInMillis);
}

bool hasLedsIntervalGone()
{
  return hasIntervalGone(ledsThreadLastRun, ledsThreadDelayInMillis);
}

bool hasDhtIntervalGone()
{
  return hasIntervalGone(dhtThreadLastRun, dhtThreadDelayInMillis);
}

bool hasBuzzerIntervalGone()
{
  return hasIntervalGone(buzzerThreadLastRun, buzzerThreadDelayInMillis);
}
