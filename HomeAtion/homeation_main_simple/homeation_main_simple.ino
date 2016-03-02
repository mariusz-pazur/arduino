#include <EtherCard.h>
#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include "aes256.h"
//#include "crypto.h"

#define STATIC 1

struct RemoteDevice {
  uint8_t deviceAddress[5];
  uint8_t deviceType;
  uint8_t deviceReadStateCommand[4];
  uint8_t commandResponse[16];
};
static RemoteDevice remoteDevices[] = 
{    
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD2 }, 1, { 0, 1, 3, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } },
    { { 0xF0, 0xF0, 0xF0, 0xF0, 0xD3 }, 2, { 1, 2, 0, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } }   
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
byte Ethernet::buffer[450]; 
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

aes256_context ctxt;
//add your own in crypto.h
uint8_t cryptoKey[] = { 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  }; 

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

void udpSerialPrint(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len) 
{
  if (strncmp(echoRequest, data, 20) == 0)
  {
    ether.sendUdp(homeAtionResponse, sizeof(homeAtionResponse), portMy, broadcastip, portDestination);      
  } 
}

void setupEthernet()
{
  digitalWrite(greenLedPin, HIGH);
  if (ether.begin(sizeof Ethernet::buffer, mymac, ethernetcsPin) == 0) 
  {   
  }
  else
  {
#if STATIC
    ether.staticSetup(myip, gwip, dnsip, mask);
    digitalWrite(greenLedPin, LOW);
#else
    if (ether.dhcpSetup())
    {
      digitalWrite(greenLedPin, LOW);
    }
#endif    
    ether.udpServerListenOnPort(&udpSerialPrint, portMy);
  }  
}

void setupEncryption()
{  
  aes256_init(&ctxt, cryptoKey);          
}

void setup() 
{
  pinMode(greenLedPin, OUTPUT); 
  setupEthernet();  
  setupRF();   
  setupEncryption(); 
}

//commandArray[0] - id - indeks w tablicy adresów
//commandArray[1] - type - 1 - RemotePower
//commandArray[2] - cmd - 0 - enable - commandArray[3] - param - 0-3 (numer portu)
//        - 1 - disable - j.w.
//        - 2 - switch - j.w.
//        - 3 - read all
//	      - 4 - enable all
//	      - 5 - disable all
//        - 6 - set state - commandArray[3] - param - flaga z bitami odpowiadającymi stanom portów

//commandArray[1] - type - 2 - RemoteLEDEnv
//commandArray[2] - cmd - 0 - read state {X1, X2, X3, X4}, X1 --> temp; X2 --> humid; X3 --> noise; X4 --> LEDs
//                      - 1 - LEDs Off
//                      - 2 - LEDs Effects (param - 0 --> RainbowWheel, 1 --> NoiseColor)
//                      - 3 - Set LEDs Color (param - color)
//                      - 4 - Noise Effects (param - 0 --> NoiseBrightness)
boolean sendRF24Command(byte* commandArray, uint8_t* response)
{    
  byte encryptedCommand[commandAndResponseLength];
  for (int i = 0; i< commandAndResponseLength; i++)
    encryptedCommand[i] = commandArray[i]; 
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
    if ( ( millis() - started_waiting_at ) > 2000 ) 
    {
      return false;
    }
  }  
  Mirf.getData(response);
  aes256_decrypt_ecb(&ctxt, response);
  				
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
    return true;      
  }
  else
  {
    return false;
  }                    
}

void commandResponse(byte id, uint8_t* response) 
{   
  bfill.emit_p(deviceJson, id, remoteDevices[id].deviceType, response[0], response[1], response[2], response[3]);  
}

void loop() 
{    
  boolean hasCommandSend = false;
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len); 
  if (pos) 
  {
    delay(1);
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /command", data, 12) == 0)
    {
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
  }   
}

