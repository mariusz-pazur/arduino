#include <SPI.h>
#include <RF24.h>
#include "printf.h"

//
// Hardware configuration
//
// Set up nRF24L01 radio on SPI bus plus pins 7 & 8
RF24 radio(7,8);
const uint64_t remoteAddress = 0xF0F0F0F0D2LL;
const uint64_t myAddress = 0xF0F0F0F0E1LL;

const int command_pin = 9;
int commands[] = {0, 1};
int commandToSend = 0;

void setup(void)
{
  Serial.begin(57600);
  printf_begin();
  printf("HomeAtion Main\n\r");

  setup_RF();
}

void setup_RF(void)
{
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(0x4c);
  radio.openWritingPipe(myAddress);
  radio.openReadingPipe(1,remoteAddress);
  radio.enableDynamicPayloads() ;
  radio.setAutoAck( true ) ;
  radio.powerUp() ;
  radio.startListening();
  radio.printDetails();
}

void loop(void)
{
    // First, stop listening so we can talk.
    radio.stopListening();

    //choose command
    pinMode(command_pin, INPUT);
    digitalWrite(command_pin,HIGH);
    delay(20); // Just to get a solid reading on the role pin

    // read the address pin, establish our role
    if (digitalRead(command_pin))
      commandToSend = 0;
    else
      commandToSend = 1;
      
    printf("Now sending %d...",commands[commandToSend]);
    uint8_t commandLength = sizeof(int);
    bool ok = radio.write( commands[commandToSend], commandLength );
    
    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");

    // Now, continue listening
    radio.startListening();

    // Wait here until we get a response, or timeout (250ms)
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > 1+(radio.getMaxTimeout()/1000) )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
      printf("Failed, response timed out.\n\r");
      printf("Timeout duration: %d\n\r", (1+radio.getMaxTimeout()/1000) ) ;
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      //unsigned long got_time;
      int response;
      radio.read( &response, commandLength );

      // Spew it
      printf("Got response %d\n\r",response);
    }

    // Try again 1s later
    delay(1000);
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
