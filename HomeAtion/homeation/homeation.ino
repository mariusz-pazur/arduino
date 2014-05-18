#include <SPI.h>
#include <UIPEthernet.h>
#include <RF24.h>
#include "printf.h"

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };


RF24 radio(7,8);
const uint64_t remoteAddress = 0xF0F0F0F0D2LL;
const uint64_t myAddress = 0xF0F0F0F0E1LL;
byte commandToSend[3];

EthernetServer server(80);
IPAddress localIP(192,168,0,6);
char requestLine[50];
int requestLinePos = 0;

void setup() {
  Serial.begin(57600);  
  Ethernet.begin(mac,localIP);
  server.begin();
  Serial.print("Server is at ");
  Serial.println(localIP);
  printf_begin();
  printf("HomeAtion Main\n\r");
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

//commandArray[0] - id - 1 - RemotePower
//commandArray[1] - cmd - 0 - enable - commandArray[2] - param - 0-3 (numer portu)
//        - 1 - disable - j.w.
//        - 2 - switch - j.w.
//        - 3 - read - j.w.
int sendRFCommand(byte* commandArray)
{
    int response;
    // First, stop listening so we can talk.
    radio.stopListening(); 
    printf("Now sending (%d-%d-%d)", commandArray[0], commandArray[1], commandArray[2]);
    bool ok = radio.write(commandArray, 3);
    
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
      radio.read( &response, sizeof(int) );
      // Spew it
      printf("Got response %d\n\r",response);
    }
    return response;
}

byte* getCommandFromQuery(char* requestLine, int requestLineLength)
{    
    byte commandArray[3];
    int parameterNumber = 0;    
    for (int i = 0; i < requestLineLength; i++)
    {
      char ch = requestLine[i];
      if (ch == '=')
      {
        commandArray[parameterNumber] = (byte)atoi(&(requestLine[i+1]));
        parameterNumber++;
      }
    }
        
    /*int qsParamStart = requestLine.indexOf("id=");
    int qsParamEnd = 0;
    Serial.println("RequestLine " + requestLine);
    //printf("start=%d;end=%d;id=%d", qsParamStart, qsParamEnd, commandArray[0]);
    if (qsParamStart != -1)
    {
      qsParamEnd = requestLine.indexOf("&", qsParamStart);    
      if (qsParamEnd == -1)
        commandArray[0] = (byte)(requestLine.substring(qsParamStart+3).toInt());
      else
        commandArray[0] = (byte)(requestLine.substring(qsParamStart+3, qsParamEnd).toInt());  
    }  
    else
     return NULL; 
    
    qsParamStart = requestLine.indexOf("cmd=");
    if (qsParamStart != -1)
    {
      qsParamEnd = requestLine.indexOf("&", qsParamStart);
      if (qsParamEnd == -1)
        commandArray[1] = (byte)(requestLine.substring(qsParamStart+4).toInt());
      else
        commandArray[1] = (byte)(requestLine.substring(qsParamStart+4, qsParamEnd).toInt());     
    }
    else
      return NULL;
    //printf("qsParamStart=%d;qsParamEnd=%d;command=%d", qsParamStart, qsParamEnd, commandArray[1]); 
    qsParamStart = requestLine.indexOf("param=");
    if (qsParamStart != -1)
    {
      qsParamEnd = requestLine.indexOf("&", qsParamStart);
      if (qsParamEnd == -1)
        commandArray[2] = (byte)(requestLine.substring(qsParamStart+6).toInt());
      else
        commandArray[2] = (byte)(requestLine.substring(qsParamStart+6, qsParamEnd).toInt());     
    }
    else
      return NULL;
    //printf("qsParamStart=%d;qsParamEnd=%d;parameter=%d", qsParamStart, qsParamEnd, commandArray[2]);*/
    return commandArray;
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  int response = -1;
  if (client) {
    Serial.println("new client");
    
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();        
        requestLine[requestLinePos] = c;
        requestLinePos++;
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {         
          requestLine[requestLinePos] = '\0';
           printf(requestLine);
          // send a standard http response header
          byte* command = getCommandFromQuery(requestLine, requestLinePos);
          //if (command)
            //response = sendRFCommand(command);          
          //Serial.println(requestLine);        
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response	  
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html><body>"); 
          for (int j = 1; j <= 4; j++)
          { 
            client.print(j);
            for (int i = 0; i < 4; i++)
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
            if (command && command[2] == j)
            {
              client.print("Status ");
              client.print(j);
              client.println(" - " + response);
            }
            client.println("<br/>");
          }
          
          client.println("</body></html>");
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
  }
}


