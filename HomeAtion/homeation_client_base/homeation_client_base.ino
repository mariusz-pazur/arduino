#include <SPI.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(7,8);
const uint64_t myAddress = 0xF0F0F0F0D2LL;
const uint64_t mainAddress = 0xF0F0F0F0E1LL;

const int led_pin = 9;

void setup(void)
{
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Client\n\r");

  setup_RF();
  
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
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
      int command = 0;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &command, sizeof(int) );
      }
      
      // First, stop listening so we can talk
      radio.stopListening();

      if (command == 0)
        digitalWrite(led_pin, LOW);
      else
        digitalWrite(led_pin, HIGH);  
      // Send the final one back. This way, we don't delay
      // the reply while we wait on serial i/o.
      radio.write( &command, sizeof(int) );
      printf("Sent response %d\n\r", command);

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
