#include <EtherCard.h>
#include <SPI.h>
#include <RF24.h>
#include "printf.h"

#define HOME_ATION_DEBUG 1

RF24 radio(7,8);
const uint64_t remoteAddress = 0xF0F0F0F0D2LL;
const uint64_t myAddress = 0xF0F0F0F0E1LL;
byte commandToSend[3];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte localIP = { 192, 168, 0, 6 };
byte Ethernet::buffer[500];
BufferFiller bfill;
#if HOME_ATION_DEBUG
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

#include <EtherCard.h>

#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
// ethernet interface ip address
static byte myip[] = { 
  192,168,1,200 };
// gateway ip address
static byte gwip[] = { 
  192,168,1,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 
  0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;


#define numberOfLeds 5
int LED[numberOfLeds] = { 2,3,4,5,6}; //This would use digital2 to digital6, but you could choose any 
char LedURL[numberOfLeds][numberOfLeds+1] ={ "10000", "01000", "00100", "00010", "00001"}; //All leds begin off, so the LEDs in the array are off except for the one which a given hyperlinks would turn on.

void setup(){
  for(byte i = 0; i < numberOfLeds; i++){
    pinMode(LED[i],OUTPUT); //LEDs are outputs
    digitalWrite(LED[i],LOW); //Off to begin with
  }

  Serial.begin(57600);
  Serial.println("\n[backSoon]");

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");
#endif

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
}
const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_Found[] PROGMEM =
"HTTP/1.0 302 Found\r\n"
"Location: /\r\n\r\n";

const char http_Unauthorized[] PROGMEM =
"HTTP/1.0 401 Unauthorized\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>401 Unauthorized</h1>";

void homePage(){
  //$F represents p string, $D represents word or byte, $S represents string. 
  //There are 5 leds, so 5 hyperlinks.
  bfill.emit_p(PSTR("$F"
    "<meta http-equiv='refresh' content='5'/>"
    "<title>Ethercard LED</title>" 
    "Turn LED$D <a href=\"?led=$S\">$S</a>" 
    "Turn LED$D <a href=\"?led=$S\">$S</a>" 
    "Turn LED$D <a href=\"?led=$S\">$S</a>" 
    "Turn LED$D <a href=\"?led=$S\">$S</a>" 
    "Turn LED$D <a href=\"?led=$S\">$S</a>"),
  http_OK,
  (word)1,LedURL[0],(LedURL[0][0]=='1')?"ON":"OFF",
  (word)2,LedURL[1],(LedURL[1][1]=='1')?"ON":"OFF",
  (word)3,LedURL[2],(LedURL[2][2]=='1')?"ON":"OFF",
  (word)4,LedURL[3],(LedURL[3][3]=='1')?"ON":"OFF",
  (word)5,LedURL[4],(LedURL[4][4]=='1')?"ON":"OFF"); 
  //I forgot in these that we are checking for '1' and '0', not 1 and 0. I also got the indecies wrong 
  //(I was using MATLAB prior to writing this and they use 1 indexing not 0 indexing so I got confused :S
}

void loop(){
  // DHCP expiration is a bit brutal, because all other ethernet activity and
  // incoming packets will be ignored until a new lease has been acquired
  if (!STATIC &&  ether.dhcpExpired()) {
    Serial.println("Acquiring DHCP lease again");
    ether.dhcpSetup();
  }
  // wait for an incoming TCP packet, but ignore its contents
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len); 
  if (pos) {
    // write to LED digital output

    delay(1);   // necessary for my system
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /", data, 5) != 0) { //I also accidentally copied this bit in twice (though that shouldn't be an issue)
      // Unsupported HTTP request
      // 304 or 501 response would be more appropriate
      bfill.emit_p(http_Unauthorized);
    } else {
        data += 5;
        if (data[0] == ' ') { //Check if the home page, i.e. no URL
          homePage();
        } else if (!strncmp("?led=",data,5)){ //Check if a url which changes the leds has been recieved
          data += strcspn (data,"01"); //Move to the start of the URL
          char tempURL[numberOfLeds+1] = {0};
          strncpy(tempURL, data, numberOfLeds); //Extract the recieved URL to the temporary string 
          Serial.print("temp = ");
          Serial.println(tempURL); //Just some quick logging
          for(byte i = 0; i < numberOfLeds ; i++){ //Check through each of the LEDs URLs
            if(!strcmp(tempURL,LedURL[i])){
              //The recieved URL matches the one required to turn this LED on, so we will turn it on.
              digitalWrite(LED[i],(tempURL[i] == '1')?HIGH:LOW); //Set the led state to match the URL.
              //Now we need to toggle the state of this LED in all of the URLs, so that all the hyperlinks can be corrected to match this state.
              for(byte j = 0; j < numberOfLeds ; j++){
                if(j==i){
                  LedURL[j][i] = (tempURL[i] == '1')?'0':'1'; //Notice how we toggle the 'i'th led in each url.
                  Serial.print("led = ");
                  Serial.println(LedURL[j]); 
                  continue;
                }
                LedURL[j][i] = tempURL[i]; //Notice how we toggle the 'i'th led in each url.
                Serial.print("led = ");
                Serial.println(LedURL[j]);     
              }
              //The URL was found, so we don't need to check any others.
              break; //Exit the for loop.
            }          
          }
          bfill.emit_p(http_Found);
        } else { //Otherwise, page isn't found
          // Page not found
          bfill.emit_p(http_Unauthorized);
        }
      }

      ether.httpServerReply(bfill.position());    // send http response
    
  }
}


