#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include "printf.h"
#include "hardware.h"

static uint8_t myAddress[] =  {0xF0, 0xF0, 0xF0, 0xF0, 0xD2};
static uint8_t mainAddress[] = {0xF0, 0xF0, 0xF0, 0xF0, 0xE1 };
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;

int socketPins[] = { 5, 6, 7, 8 };
uint8_t socketPinsState[] = { HIGH, HIGH, HIGH, HIGH};

#define HA_REMOTE_POWER_DEBUG 1

void setup(void)
{
#if HA_REMOTE_POWER_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Power Strip\n\r");
#endif
  setupRF();
  setupRelay(); 
#if HA_REMOTE_POWER_DEBUG
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
  Mirf.payload = 4;
  Mirf.channel = 76;
  Mirf.config();
}

void setupRelay()
{
  for (int i = 0; i < 4; i++)
  {
    pinMode(socketPins[i], OUTPUT);
    digitalWrite(socketPins[i], socketPinsState[i]);
  }
}

void loop(void)
{  
  // if there is data ready
  if(!Mirf.isSending() && Mirf.dataReady())
  {
    // Dump the payloads until we've gotten everything
    byte command[4];      
    bool done = false;
    Mirf.getData(command);     
#if HA_REMOTE_POWER_DEBUG
    printf("Read command from radio {%d,%d,%d}\n\r", command[1],command[2],command[3]);
#endif    
    if (command[1] == 1)//RemotePower
    {
      if (command[2] == 0) //enable 
      {          
        digitalWrite(socketPins[command[3]], LOW);
        socketPinsState[command[3]] = LOW;
      }
      else if (command[2] == 1) //disable
      {          
        digitalWrite(socketPins[command[3]], HIGH);
        socketPinsState[command[3]] = HIGH;
      }
      else if (command[2] == 2) //switch
      {
        if (socketPinsState[command[3]] == LOW)
        {
          digitalWrite(socketPins[command[3]], HIGH);
          socketPinsState[command[3]] = HIGH;
        }
        else
        {
          digitalWrite(socketPins[command[3]], LOW); 
          socketPinsState[command[3]] = LOW;         
        }
      } 
      else if (command[1] == 3)//read
      {          
        //do nothing          
      }
      else if (command[2] == 4) //enable all
      {          
        for (int i = 0; i < 4; i++)
        {
          digitalWrite(socketPins[i], LOW);
          socketPinsState[i] = LOW;          
        }
      }
      else if (command[2] == 5) //disable all
      {          
        for (int i = 0; i < 4; i++)
        {
          digitalWrite(socketPins[i], HIGH);
          socketPinsState[i] = HIGH;          
        }          
      }
    }            
    Mirf.send(socketPinsState);
#if HA_REMOTE_POWER_DEBUG
    printf("Sent state response {%d,%d,%d,%d}\n\r", socketPinsState[0],socketPinsState[1],socketPinsState[2],socketPinsState[3]);
#endif        
  } 
}

