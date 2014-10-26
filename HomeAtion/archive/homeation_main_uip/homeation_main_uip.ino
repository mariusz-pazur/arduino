#include <SPI.h>
#include <UIPEthernet.h>
#include <RF24.h>
#include "printf.h"

#define HOME_ATION_DEBUG 1

RF24 radio(7,8);
const uint64_t remoteAddress = 0xF0F0F0F0D2LL;
const uint64_t myAddress = 0xF0F0F0F0E1LL;
byte commandToSend[3];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetServer server(80);
#ifdef HOME_ATION_DEBUG
IPAddress localIP(192,168,0,6);
#else
IPAddress localIP;
#endif
char requestLine[100];
int requestLinePos = 0;

void setup() 
{
#ifdef HOME_ATION_DEBUG
  Serial.begin(57600);  
  printf_begin();
#endif
  setupEthernet();
  setupRF();
#ifdef HOME_ATION_DEBUG
  printf("HomeAtion Main\n\r");
  printf("Server is at %d.%d.%d.%d\n\r", localIP[0], localIP[1], localIP[2], localIP[3]);  
#endif
}

void setupRF()
{
	radio.begin();
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(0x4c);
	radio.openWritingPipe(myAddress);
	radio.openReadingPipe(1,remoteAddress);
	radio.enableDynamicPayloads() ;
	radio.setAutoAck(true) ;
	radio.powerUp() ; 
#ifdef HOME_ATION_DEBUG
	radio.printDetails();
#endif
}

void setupEthernet()
{
#ifdef HOME_ATION_DEBUG
  Ethernet.begin(mac, localIP);
#else
  Ethernet.begin(mac);
  localIP = Ethernet.localIP();
#endif
  server.begin();
}

//commandArray[0] - id - 1 - RemotePower
//commandArray[1] - cmd - 0 - enable - commandArray[2] - param - 0-3 (numer portu)
//        - 1 - disable - j.w.
//        - 2 - switch - j.w.
//        - 3 - read all
//		  - 4 - enable all
//		  - 5 - disable all
boolean sendRFCommand(byte* commandArray, uint8_t* response)
{    
    // First, stop listening so we can talk.    
#ifdef HOME_ATION_DEBUG
    printf("Now sending (%d-%d-%d)", commandArray[0], commandArray[1], commandArray[2]);
#endif
    bool ok = radio.write(commandArray, 3);
    radio.startListening();
    if (ok)
	{
		// Wait here until we get a response, or timeout (250ms)
		unsigned long started_waiting_at = millis();
		bool timeout = false;
		while ( ! radio.available() && ! timeout )
		  if (millis() - started_waiting_at > 1+(radio.getMaxTimeout()/1000) )
			timeout = true;

		// Describe the results
		if (!timeout)		
		{
		  if (commandArray[0] == 1)//Remote Power Strip
		  {
			radio.read(response, 4*sizeof(uint8_t) );
			// Spew it
#ifdef HOME_ATION_DEBUG
			printf("Got response {%d,%d,%d,%d}\n\r",response[0],response[1],response[2],response[3]);
#endif
		  }
		  return true;
		}  
	}
	radio.stopListening();
	return false;
}

boolean getCommandFromQuery(char* requestLine, int requestLineLength, byte* commands)
{        
    int parameterNumber = 0;    
    for (int i = 0; i < requestLineLength; i++)
    {
      char ch = requestLine[i];
      if (ch == '=')
      {
        commands[parameterNumber] = (byte)atoi(&(requestLine[i+1]));
        parameterNumber++;
      }
    }
    if (parameterNumber == 3)
    {
#ifdef HOME_ATION_DEBUG
      printf("id=%d;cmd=%d;param=%d\n\r", commands[0], commands[1], commands[2]);
#endif
      return true;      
    }
    else
    {
#ifdef HOME_ATION_DEBUG
      printf("no params\n\r");
#endif
      return false;
    }                    
}

void loop() 
{
  // listen for incoming clients
  EthernetClient client = server.available();
  uint8_t response[4];
  boolean hasCommandSend = false;
  if (client) {
#ifdef HOME_ATION_DEBUG
    printf("new client\n\r");
#endif
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    boolean isFirstLine = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();    
        if (isFirstLine && requestLinePos < 99)
        {    
          requestLine[requestLinePos] = c;
          requestLinePos++;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {         
          requestLine[requestLinePos] = '\0';
#ifdef HOME_ATION_DEBUG
          printf(requestLine);
#endif
          byte commands[3];
          boolean hasParameters = getCommandFromQuery(requestLine, requestLinePos, commands);
          int numberOfRetries = 3;
          while(hasParameters && !hasCommandSend && numberOfRetries > 0)
          {
            numberOfRetries--;
            hasCommandSend = sendRFCommand(commands, response);    
          }
		  // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response	  
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html><body>"); 		  
          for (int j = 0; j < 4; j++)
          { 
            client.print(j+1);
            for (int i = 0; i < 6; i++)
            {              
              client.print("- <a href=\"http://");
              client.print(localIP);
              client.print("/?id=1&cmd=");
              client.print(i);
              client.print("&param=");
              client.print(j);
              client.print("\">Cmd "); 
              client.print(i);              
              client.println("</a> ");                        
            }
            client.print("State - ");  
			if (response[j] == LOW)
				client.println("ON");            
			else
				client.println("OFF");
            client.println("<br/>");
          }
		  if (hasParameters && !hasCommandSend)
			  client.println("Error during radio send");
          client.println("</body></html>");          
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          isFirstLine = false;
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
    requestLinePos = 0;   
#ifdef HOME_ATION_DEBUG
    printf("client disconnected\n\r");        
#endif
  }
}


