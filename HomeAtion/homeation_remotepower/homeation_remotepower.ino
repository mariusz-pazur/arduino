#include <SPI.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(7,8);
const uint64_t myAddress = 0xF0F0F0F0D2LL;
const uint64_t mainAddress = 0xF0F0F0F0E1LL;

int socketPins[] = {2, 3, 4, 5};


void setup(void)
{
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Remote Power Strip\n\r");

  setup_RF();
  
  for (int i = 0; i < 4; i++)
  {
    pinMode(socketPins[i], OUTPUT);
    digitalWrite(socketPins[i], HIGH);
  }
  
}

void setup_RF(void)
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
  radio.printDetails();
}

void loop(void)
{
  
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      byte command[3];
      int response = 0;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( command, 3 );
      }      
      
      // First, stop listening so we can talk
      radio.stopListening();

      if (command[0] == 1)//RemotePower
      {
        if (command[1] == 0) //enable
        {
          pinMode(socketPins[command[2]], OUTPUT);
          digitalWrite(socketPins[command[2]], LOW);
        }
        else if (command[1] == 1)
        {
          pinMode(socketPins[command[2]], OUTPUT);
          digitalWrite(socketPins[command[2]], HIGH);
        }
        else if (command[1] == 2)
        {
          pinMode(socketPins[command[2]], INPUT);
          delay(20);
          response = digitalRead(socketPins[command[2]]);
          pinMode(socketPins[command[2]], OUTPUT);
          if (response == LOW)
            digitalWrite(socketPins[command[2]], HIGH);
          else
            digitalWrite(socketPins[command[2]], LOW);          
        }
        else if (command[1] == 3)
        {
          pinMode(socketPins[command[2]], INPUT);
          delay(20);
          response = digitalRead(socketPins[command[2]]);          
        }
      }  
      // Send the final one back. This way, we don't delay
      // the reply while we wait on serial i/o.
      radio.write( &response, sizeof(int) );
      printf("Sent response %d\n\r", command);

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
