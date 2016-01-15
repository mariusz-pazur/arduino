#include <EtherCard.h>
#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
//#include <LiquidCrystal.h>
#include "printf.h"
#include "hardware.h"
#include "aes256.h"
#include "crypto.h"
#include "thingspeak.h"

#define STATIC 1
#define HOME_ATION_DEBUG 0

//add yours in "thingspeak.h"
//const char thingspeakApiKey[] = "beef1337beef1337" 
const char thingspeakApiUrl[] PROGMEM = "api.thingspeak.com";
bool checkThingspeakUrl = false;
static void thingspeak_callback (byte status, word off, word len) ;

struct RemoteDevice {
  uint8_t deviceAddress[5];
  uint8_t deviceType;
  uint8_t deviceReadStateCommand[4];
  uint8_t commandResponse[16];
};
static RemoteDevice remoteDevices[] = 
{    
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD2 }, 1, { 0, 1, 3, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } },
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD3 }, 2, { 1, 2, 2, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } },
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD3 }, 3, { 2, 3, 2, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } },
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD3 }, 4, { 3, 4, 0, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } } 
};
static uint8_t myAddress[] = { 0xF0, 0xF0, 0xF0, 0xF0, 0xE1 };
static uint8_t rf24cePin = 9;
static uint8_t rf24csnPin = 10;
static uint8_t commandAndResponseLength = 16;

#if STATIC
static byte myip[] = { 192,168,0,6 };
static byte gwip[] = { 192,168,0,1 };
static byte dnsip[] = { 62,179,1,60 };
static byte mask[] = { 255,255,255,0 };
#endif
static byte broadcastip[] = { 255,255,255,255 };
static byte mymac[] = {  0x74,0x69,0x69,0x2D,0x30,0x31 };
byte Ethernet::buffer[500]; 
BufferFiller bfill;
static uint8_t ethernetcsPin = 18;
unsigned int portMy = 40000; 
unsigned int portDestination = 40001; 

const char deviceJson[] PROGMEM = "{\"id\":$D,\"type\":$D,\"state\":[$D,$D,$D,$D]}";
const char devicesJsonStart[] PROGMEM = "["; 
const char devicesJsonSeparator[] PROGMEM = ","; 
const char devicesJsonEnd[] PROGMEM = "]";
const char httpOkHeaders[] PROGMEM =
"HTTP/1.1 200 OK\r\n"
"Content-Type: application/json\r\n"
"Connection: close\r\n"
"Pragma: no-cache\r\n\r\n";

const char httpErrorHeaders[] PROGMEM =
"HTTP/1.1 500 Error\r\n"
"Content-Type: application/json\r\n"
"Connection: close\r\n"
"Pragma: no-cache\r\n\r\n"
"{\"message\":\"$F\"}";

const char http404Headers[] PROGMEM = 
"HTTP/1.1 404 NotFound\r\n"
"Content-Type: application/json\r\n"
"Connection: close\r\n"
"Pragma: no-cache\r\n\r\n";

const char commandErrorInfo[] PROGMEM = 
"Error during command send";
const char echoRequest[] = "HomeAtionMainRequest";
const char homeAtionResponse[] = "HomeAtionMain";
const char echoText[] PROGMEM = "HomeAtionMain";

static uint8_t greenLedPin = 3;

//LiquidCrystal lcd(12,11,5,4,3,2);

aes256_context ctxt;
//add your own in crypto.h
//uint8_t cryptoKey[] = { 
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
//    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
//  }; 

void setupRF()
{
  Mirf.cePin = rf24cePin;
  Mirf.csnPin = rf24csnPin;
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.setRADDR(myAddress);  
  Mirf.payload = commandAndResponseLength;
  Mirf.channel = 90;
  Mirf.configRegister( RF_SETUP, ( 1<<2 | 1<<1 | 1<<5 ) );
  Mirf.config(); 
}

static void thingspeak_callback (byte status, word off, word len) 
{
  checkThingspeakUrl = false;
#if HOME_ATION_DEBUG
    printf("Callback: status-%d,off-%d,len-%d\n\r", status, off, len);
#endif
}

void udpSerialPrint(word port, byte ip[4], const char *data, word len) 
{
//#if HOME_ATION_DEBUG
//  printf("S:%d.%d.%d.%d\n\r", ip[0], ip[1], ip[2], ip[3]);  
//  printf("D:%d\n\r", port);
//  printf("Data:%s\n\r", data);
//  printf("L:%d\n\r", len);
//#endif
  if (strncmp(echoRequest, data, 20) == 0)
  {
//#if HOME_ATION_DEBUG
//    printf("Resp:%s\n\r", echoText);    
//#endif
    ether.sendUdp(homeAtionResponse, sizeof(homeAtionResponse), portMy, broadcastip, portDestination);    
//#if HOME_ATION_DEBUG
//    printf("Send\n\r");
//#endif    
  } 
}

void setupEthernet()
{
  
  if (ether.begin(sizeof Ethernet::buffer, mymac, ethernetcsPin) == 0) 
  {
#if HOME_ATION_DEBUG
    printf("Ether f\n\r");
#endif    
  }
  else
  {
#if STATIC
    ether.staticSetup(myip, gwip, dnsip, mask);
    digitalWrite(greenLedPin, LOW);
#else
    if (!ether.dhcpSetup())
    {
#if HOME_ATION_DEBUG
      printf("DHCP f\n\r");
#endif      
    }
    else
    {
      digitalWrite(greenLedPin, LOW);
    }
#endif    
    ether.udpServerListenOnPort(&udpSerialPrint, portMy);
  }
  if (!ether.dnsLookup(thingspeakApiUrl))
  {
#if HOME_ATION_DEBUG
    printf("DNS f\n\r");
#endif
  }
}

void setupEncryption()
{  
  aes256_init(&ctxt, cryptoKey);          
}

void setup() 
{
#if HOME_ATION_DEBUG
  Serial.begin(57600);  
  printf_begin();
#endif
  setupEthernet();  
  setupRF();  
#if HOME_ATION_DEBUG 
  printf("START:%d.%d.%d.%d\n\r", ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3]);  
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
  printf("RAM:%d B\n\r", freeRam());     
#endif
  pinMode(greenLedPin, OUTPUT); 
  setupEncryption(); 
}

//commandArray[0] - id - indeks w tablicy adresďż˝w
//commandArray[1] - type - 1 - RemotePower
//commandArray[2] - cmd - 0 - enable - commandArray[3] - param - 0-3 (numer portu)
//        - 1 - disable - j.w.
//        - 2 - switch - j.w.
//        - 3 - read all
//	      - 4 - enable all
//	      - 5 - disable all
//        - 6 - set state - commandArray[3] - param - flaga z bitami odpowiadającymi stanom portów

//commandArray[1] - type - 2 - RemoteLED
//commandArray[2] - cmd - 0 - turn off
//                      - 1 - enable effect - commandArray[3] - param - 0(rainbow wheel)
//                      - 2 - read state {X1, X2, X3, X4}, X1 == 0 --> turned off; X1 == 1 --> effect enabled, X2 --> effect number (0 - rainbow wheel)

//commandArray[1] - type - 3 - RemoteDHT
//commandArray[2] - cmd - 0 - read temp
//                      - 1 - read humidity
//                      - 2 - read all

//commandArray[1] - type - 4 - RemoteNoise
//commandArray[2] - cmd - 0 - read mean in time (default or set )
//                      - 1 - read current value
//                      - 2 - change mean reads delay (param - delay in seconds)


boolean sendRF24Command(byte* commandArray, uint8_t* response)
{    
  byte encryptedCommand[commandAndResponseLength];
  for (int i = 0; i< commandAndResponseLength; i++)
    encryptedCommand[i] = commandArray[i]; 
#ifdef HOME_ATION_DEBUG
  printf("Sending(%d-%d-%d)", commandArray[1], commandArray[2], commandArray[3]);
#endif 
  Mirf.setTADDR(remoteDevices[commandArray[0]].deviceAddress);
  aes256_encrypt_ecb(&ctxt, encryptedCommand);
  Mirf.send(encryptedCommand);
  
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
      printf("Timeout");
#endif
      return false;
    }
  }
  if (commandArray[1] == 1 || commandArray[1] == 2 || commandArray[1] == 3 || commandArray[1] == 4)//Remote Power Strip || Remote LED || DHT11 || NoiseSensor
  {
    Mirf.getData(response);
    aes256_decrypt_ecb(&ctxt, response);				
#if HOME_ATION_DEBUG
    printf("Resp{%d,%d,%d,%d}\n\r",response[0],response[1],response[2],response[3]);
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
  if (parameterNumber == 4)
  {
#ifdef HOME_ATION_DEBUG
    printf("id=%d;type=%d;cmd=%d;param=%d\n\r", commands[0], commands[1], commands[2], commands[3]);
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

void commandResponse(byte id, uint8_t* response) 
{   
  bfill.emit_p(deviceJson, id, remoteDevices[id].deviceType, response[0], response[1], response[2], response[3]);  
}

float bytes2Float(byte bytes_temp[4])
{ 
  union {
    float a;
    unsigned char bytes[4];
  } thing;
  for (int i = 0; i < 4; i++)
  {
     thing.bytes[i] = bytes_temp[i];
  }
  return thing.a;  
}

int bytes2Int(byte bytes_temp[2])
{ 
  union {
    int a;
    unsigned char bytes[2];
  } thing;
  for (int i = 0; i < 2; i++)
  {
     thing.bytes[i] = bytes_temp[i];
  }
  return thing.a;  
}

void loop() 
{  
  delay(10);  
  boolean hasCommandSend = false;
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len); 
  if (pos) 
  {
    delay(1);
#ifdef HOME_ATION_DEBUG
    printf("http start\n\r");
#endif
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
#ifdef HOME_ATION_DEBUG
    printf(data);		
#endif
    if (strncmp("GET /command", data, 12) == 0)
    {
#ifdef HOME_ATION_DEBUG
      printf("\n\rcommand\n\r");		
#endif
      byte command[commandAndResponseLength];
      boolean hasParameters = getCommandFromQuery(data, len, command);
      int numberOfRetries = 3;
      while(hasParameters && !hasCommandSend && numberOfRetries > 0)
      {
        numberOfRetries--;
        hasCommandSend = sendRF24Command(command, remoteDevices[command[0]].commandResponse);    
      }
      if (hasCommandSend)
      {
        bfill.emit_p(httpOkHeaders);
        commandResponse(command[0], remoteDevices[command[0]].commandResponse);	
        ether.httpServerReply(bfill.position());
        //thingspeak
        
        char valuesToUpdate[54];
        if (command[0] == 2 || command[0] == 3) //RemoteLedEnv
        {
          if (command[1] == 3) //DHT
          {
            if (command[2] == 2) //read state
            {
              byte envState[4];
              //temp
              envState[0] = 0;
              envState[1] = 0;
              envState[2] = remoteDevices[command[0]].commandResponse[0];
              envState[3] = remoteDevices[command[0]].commandResponse[1];
              float t = bytes2Float(envState);
              //humidity
              envState[2] = remoteDevices[command[0]].commandResponse[2];
              envState[3] = remoteDevices[command[0]].commandResponse[3];
              float h = bytes2Float(envState);
              char str_temp[6];
              dtostrf(t, 4, 2, str_temp);
              char str_humid[6];
              dtostrf(h, 4, 2, str_humid);   
#ifdef HOME_ATION_DEBUG
              printf("?api_key=%s&field1=%s&field2=%s", thingspeakApiKey, str_temp, str_humid);    
#endif           
              sprintf(valuesToUpdate, "?api_key=%s&field1=%s&field2=%s", thingspeakApiKey, str_temp, str_humid);
              if (checkThingspeakUrl)
              {
                ether.dnsLookup(thingspeakApiUrl);
              }
              checkThingspeakUrl = true;
              ether.browseUrl(PSTR("/update"), valuesToUpdate, thingspeakApiUrl, thingspeak_callback);
            }
          }
          else if (command[1] == 4) //Noise
          {
            if (command[2] == 0) //Read Mean
            {
              byte noise[2];
              noise[0] = remoteDevices[command[0]].commandResponse[3];
              noise[1] = remoteDevices[command[0]].commandResponse[2];
              int n = bytes2Int(noise);
#ifdef HOME_ATION_DEBUG
              printf("?api_key=%s&field3=%d", thingspeakApiKey, n);    
#endif           
              sprintf(valuesToUpdate, "?api_key=%s&field3=%d", thingspeakApiKey, n);
              if (checkThingspeakUrl)
              {
                ether.dnsLookup(thingspeakApiUrl);
              }
              checkThingspeakUrl = true;
              ether.browseUrl(PSTR("/update"), valuesToUpdate, thingspeakApiUrl, thingspeak_callback);
            }
          }
        }
      }
      else
      {
        bfill.emit_p(httpErrorHeaders, commandErrorInfo);
        ether.httpServerReply(bfill.position());
      }
    }
    else if (strncmp("GET /echo", data, 9) == 0)
    {
      bfill.emit_p(httpOkHeaders);
      bfill.emit_p(echoText);
      ether.httpServerReply(bfill.position());
    }
    else if (strncmp("GET /devices", data, 12) == 0)
    {
      bfill.emit_p(httpOkHeaders);
      bfill.emit_p(devicesJsonStart);
      int nrOfRemoteDevices = sizeof(remoteDevices)/sizeof(remoteDevices[0]);
      for (int i = 0; i < nrOfRemoteDevices; i++)
      {  
        hasCommandSend = false;      
        byte* commandToSend = remoteDevices[i].deviceReadStateCommand;
        uint8_t* response = remoteDevices[i].commandResponse;
        int numberOfRetries = 3;
        while(!hasCommandSend && numberOfRetries > 0)
        {
          numberOfRetries--;
          hasCommandSend = sendRF24Command(commandToSend, response);    
        }
        if (hasCommandSend)
        {
          commandResponse(commandToSend[0], response);
          if (i < (nrOfRemoteDevices - 1))
            bfill.emit_p(devicesJsonSeparator);
        }
      }
      bfill.emit_p(devicesJsonEnd);
      ether.httpServerReply(bfill.position());
    }
    else
    {
      bfill.emit_p(http404Headers);
      ether.httpServerReply(bfill.position());
    }
#ifdef HOME_ATION_DEBUG
    printf("http end\n\r");
#endif
  }   
}

