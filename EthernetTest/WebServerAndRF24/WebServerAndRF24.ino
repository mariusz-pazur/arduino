/*
  Web Server
 
 A simple web server that shows the value of the analog input pins.
 using an Arduino Wiznet Ethernet shield. 
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Analog inputs attached to pins A0 through A5 (optional)
 
 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 
 */

#include <SPI.h>
#include <UIPEthernet.h>
#include <RF24.h>
#include "printf.h"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,0,6);

RF24 radio(7,8);
const uint64_t remoteAddress = 0xF0F0F0F0D2LL;
const uint64_t myAddress = 0xF0F0F0F0E1LL;

const int command_pin = 9;
int commands[] = {0, 1};
int commandToSend = 0;

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup() {
 // Open serial communications and wait for port to open:
  Serial.begin(57600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
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

void sendRFCommand()
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
    bool ok = radio.write( &commands[commandToSend], commandLength );
    
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
    //delay(1000);
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
	  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");       
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
    sendRFCommand();    
  }
}

