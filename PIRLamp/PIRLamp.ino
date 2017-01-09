#include <RCSwitch.h>
#include "printf.h"
#include "hardware.h"

const byte pirPin = 2;  
int currentMove = LOW;
int previousMove = LOW; 
uint32_t pirThreadLastRun = 0;
uint32_t pirThreadDelayInMillis = 200;

const byte photoPin = A7;
const uint32_t photoThreadDelayInMillis = 200;
uint32_t photoThreadLastRun = 0;
int photoValue;
int photoThresholdValue = 100;

RCSwitch rf433 = RCSwitch();
const byte rf433Pin = 3;

bool isLampTurnedOn = false;

#define LOOP_DEBUG 0
#define PIR_DEBUG 0
#define PHOTO_DEBUG 0
#define RF433_DEBUG 0

void setup()
{
#if LOOP_DEBUG || PIR_DEBUG || PHOTO_DEBUG || RF433_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("PIRLamp. Free RAM: %d B\n\r", freeRam()); 
#endif
  pinMode(pirPin, INPUT);   
  rf433.enableTransmit(rf433Pin);
  rf433.setRepeatTransmit(4);
  rf433.setPulseLength(147);
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
#if PHOTO_DEBUG
  printf("PhotoV: %d\n\r", photoValue);
#endif
}

void pirCallback()
{
  previousMove = currentMove;
  currentMove = digitalRead(pirPin);
  if (currentMove == HIGH)
  {
    if (previousMove == LOW)
    {
#if PIR_DEBUG
      printf("Pir: new move\n\r");
#endif
      if (photoValue < photoThresholdValue)
      {
#if RF433_DEBUG
        printf("RF433: On\n\r");
#endif
        rf433.send(13980756, 24);        
        isLampTurnedOn = true;
      }
    }
    else
    {
#if PIR_DEBUG
      printf("Pir: move in progress\n\r");
#endif
    }
  }
  else
  {
    if (previousMove == HIGH)
    {
#if PIR_DEBUG
      printf("Pir: end of move\n\r");
#endif
      if (isLampTurnedOn == true)
      {
#if RF433_DEBUG
        printf("RF433: Off\n\r");
#endif
        rf433.send(13980753, 24);          
        isLampTurnedOn = false;
      }
    }
    else
    {
#if PIR_DEBUG
      printf("Pir: lack of move\n\r");
#endif
    }
  }
}

bool hasIntervalGone(uint32_t lastRead, uint32_t millisBetweenReads)
{
  return (lastRead + millisBetweenReads) <= millis();
}

bool hasPirIntervalGone()
{
  return hasIntervalGone(pirThreadLastRun, pirThreadDelayInMillis);
}

bool hasPhotoIntervalGone()
{
  return hasIntervalGone(photoThreadLastRun, photoThreadDelayInMillis);
}
