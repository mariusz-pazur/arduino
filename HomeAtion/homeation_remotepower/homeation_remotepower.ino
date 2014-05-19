#include <SPI.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(7,8);
const uint64_t myAddress = 0xF0F0F0F0D2LL;
const uint64_t mainAddress = 0xF0F0F0F0E1LL;

int socketPins[] = {
  2, 3, 4, 5};
uint8_t socketPinsState[] = {
  HIGH, HIGH, HIGH, HIGH};

#define HA_REMOTE_POWER_DEBUG 1

void setup(void)
{
#ifdef HA_REMOTE_POWER_DEBUG
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Power Strip\n\r");
#endif
  setupRF();
  setupRelay();  
}

void setupRF(void)
{
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(0x4c);
  radio.openWritingPipe(myAddress);
  radio.openReadingPipe(1,mainAddress);
  radio.enableDynamicPayloads() ;
  radio.setAutoAck( true ) ;
  radio.powerUp() ;
  radio.startListening();
#ifdef HA_REMOTE_POWER_DEBUG
  radio.printDetails();
#endif
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
  if ( radio.available() )
  {
    // Dump the payloads until we've gotten everything
    byte command[3];      
    bool done = false;
    while (!done)
    {
      // Fetch the payload, and see if this was the last one.
      done = radio.read( command, 3 );
    }      
#ifdef HA_REMOTE_POWER_DEBUG
    printf("Read command from radio {%d,%d,%d}\n\r", command[0],command[1],command[2]);
#endif
    // First, stop listening so we can talk
    radio.stopListening();

    if (command[0] == 1)//RemotePower
    {
      if (command[1] == 0) //enable 
      {          
        digitalWrite(socketPins[command[2]], LOW);
        socketPinsState[command[2]] = LOW;
      }
      else if (command[1] == 1) //disable
      {          
        digitalWrite(socketPins[command[2]], HIGH);
        socketPinsState[command[2]] = HIGH;
      }
      else if (command[1] == 2) //switch
      {
        if (socketPinsState[command[2]] == LOW)
        {
          digitalWrite(socketPins[command[2]], HIGH);
          socketPinsState[command[2]] = HIGH;
        }
        else
        {
          digitalWrite(socketPins[command[2]], LOW); 
          socketPinsState[command[2]] = HIGH;         
        }
      } 
      else if (command[1] == 3)//read
      {          
        //do nothing          
      }
      else if (command[1] == 4) //enable all
      {          
        for (int i = 0; i < 4; i++)
        {
          digitalWrite(socketPins[i], LOW);
          socketPinsState[i] = LOW;          
        }
      }
      else if (command[1] == 5) //disable all
      {          
        for (int i = 0; i < 4; i++)
        {
          digitalWrite(socketPins[i], HIGH);
          socketPinsState[i] = HIGH;          
        }          
      }
    }        
    radio.write(socketPinsState, 4*sizeof(uint8_t));
#ifdef HA_REMOTE_POWER_DEBUG
    printf("Sent state response {%d,%d,%d,%d}\n\r", socketPinsState[0],socketPinsState[1],socketPinsState[2],socketPinsState[3]);
#endif
    // Now, resume listening so we catch the next packets.
    radio.startListening();
  } 
}

