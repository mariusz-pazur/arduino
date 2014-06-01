#include <EtherCard.h>
#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
//#include <LiquidCrystal.h>
#include "printf.h"
#include "hardware.h"

#define STATIC 1
#define HOME_ATION_DEBUG 1

static uint8_t remoteAddress[] =  {
  0xF0, 0xF0, 0xF0, 0xF0, 0xD2};
static uint8_t myAddress[] = {
  0xF0, 0xF0, 0xF0, 0xF0, 0xE1 };
byte commandToSend[4];

#if STATIC
// ethernet interface ip address
static byte myip[] = { 
  192,168,0,6 };
// gateway ip address
static byte gwip[] = { 
  192,168,0,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 
  0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;

const char http_OK[] PROGMEM =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n"
"Pragma: no-cache\r\n\r\n";

//LiquidCrystal lcd(12,11,5,4,3,2);

void setup() 
{
#if HOME_ATION_DEBUG
  Serial.begin(57600);  
  printf_begin();
#endif
  setupEthernet();  
  setupRF();  
#if HOME_ATION_DEBUG
  printf("HomeAtion Main\n\r");
  printf("Server is at %d.%d.%d.%d\n\r", ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3]);  
#endif
  /*lcd.begin(16, 2);
   lcd.clear();
   lcd.print(ether.myip[0]);
   lcd.print('.');
   lcd.print(ether.myip[1]);
   lcd.print('.');
   lcd.print(ether.myip[2]);
   lcd.print('.');
   lcd.print(ether.myip[3]);*/  
#if HOME_ATION_DEBUG 
   printf("Free RAM: %d B\n\r", freeRam());     
#endif
}

void setupRF()
{
  Mirf.cePin = 7;
  Mirf.csnPin = 8;
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.setRADDR(myAddress);
  Mirf.payload = 4;
  Mirf.channel = 76;
  Mirf.config(); 
}

void setupEthernet()
{
  if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) 
  {
#if HOME_ATION_DEBUG
    printf("Failed to access Ethernet controller\n\r");
#endif
  }
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if (!ether.dhcpSetup())
  {
#if HOME_ATION_DEBUG
    printf("DHCP failed\n\r");
#endif
  }
#endif
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
  Mirf.setTADDR(remoteAddress);
  Mirf.send(commandArray);
  while(Mirf.isSending())
  {
  }
  delay(10);
  unsigned long started_waiting_at = millis();
  while(!Mirf.dataReady())
  {    
    if ( ( millis() - started_waiting_at ) > 5000 ) 
    {
#if HOME_ATION_DEBUG
      printf("Timeout on response from server!");
#endif
      return false;
    }
  }
  if (commandArray[0] == 1)//Remote Power Strip
  {
    Mirf.getData(response);				
#if HOME_ATION_DEBUG
    printf("Got response {%d,%d,%d,%d}\n\r",response[0],response[1],response[2],response[3]);
#endif
  }
  return true;		 		
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
    if (ch == '\n')
      break;
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

void homePage(uint8_t* response) 
{
#ifdef HOME_ATION_DEBUG
  printf("home page before emit\n\r");
#endif
  /*bfill.emit_p(PSTR("$F"    
    "1 - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=0&param=0\">Enable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=1&param=0\">Disable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=2&param=0\">Switch</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=3&param=0\">Read</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=4&param=0\">Enable all</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=5&param=0\">Disable all</a> State - $S" 
    "2 - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=0&param=1\">Enable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=1&param=1\">Disable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=2&param=1\">Switch</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=3&param=1\">Read</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=4&param=1\">Enable all</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=5&param=1\">Disable all</a> State - $S"  
    "3 - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=0&param=2\">Enable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=1&param=2\">Disable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=2&param=2\">Switch</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=3&param=2\">Read</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=4&param=2\">Enable all</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=5&param=2\">Disable all</a> State - $S"  
    "4 - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=0&param=3\">Enable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=1&param=3\">Disable</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=2&param=3\">Switch</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=3&param=3\">Read</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=4&param=3\">Enable all</a> - <a href=\"http://$D.$D.$D.$D/?id=1&cmd=5&param=3\">Disable all</a> State - $S" ),
  http_OK,
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3], response[0] == 0 ? "OFF" : "ON",
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3], response[1] == 0 ? "OFF" : "ON",
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3], response[2] == 0 ? "OFF" : "ON",
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],
  ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3],ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3], response[3] == 0 ? "OFF" : "ON"); */
  bfill.emit_p(http_OK);
#ifdef HOME_ATION_DEBUG
  printf("home page after emit\n\r");
#endif
}

void loop() 
{
  uint8_t response[] = {0, 0, 0, 0};
  boolean hasCommandSend = false;
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len); 
  if (pos) 
  {
    delay(1);
#ifdef HOME_ATION_DEBUG
    printf("new client start\n\r");
#endif
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    printf(data);				
    byte commands[3];
    boolean hasParameters = getCommandFromQuery(data, len, commands);
    int numberOfRetries = 3;
    while(hasParameters && !hasCommandSend && numberOfRetries > 0)
    {
      numberOfRetries--;
      hasCommandSend = sendRFCommand(commands, response);    
    }
    homePage(response);	
    ether.httpServerReply(bfill.position());
#ifdef HOME_ATION_DEBUG
    printf("new client end\n\r");
#endif
  }
}


