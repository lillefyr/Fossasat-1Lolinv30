
#include <connectwifi.h>
#include <ESP8266HTTPClient.h>
// get http
HTTPClient http;


//This are your own credentials for MQTT (These are private, do not share them!):
//User: lillefyr
//Pass: DjtLrZwZqivZkdEw


// used to be 2 3, hardware serial, with only receive from radio enabled.
#include "SoftwareSerial.h"
#define D7 (13)  //Yellow to Arduino pin 4
#define D8 (15)  //Orange/Blue to ARduino pin 3
SoftwareSerial sw;  // parameters defined in setup
#include "SoftwareSerialStable.h"

String TIME;
String OUTPOWER="";
String PACKETSNR="";
String PACKETRSSI="";
String DATA="";
String url = "";

char payloadMessage[255];
byte dataBuf[127];

uint8_t satCmd = 0x30;
uint8_t sentSatCmd = 0x00;
char optional[65];
char sentOptional[65];
unsigned long counter = 0;
int8_t indexOfS1;
String field;
String dataStr;

uint8_t retryCnt = 0;
const byte numChars = 65;
char receivedChars2[numChars];

////////////// Sat comms part ///////
uint8_t sendCommandToSatellite(uint8_t, char *);

String getUrl(String url){
  String payload2;
  startWifi();

  http.begin(net, url);
  int httpCode = http.GET();
  if (httpCode > 0) { //Check for the returning code
    payload2 = http.getString();
  } else {
    if (httpCode == -11) {
      //Serial.println("Timeout");
    }
    payload2 = http.getString();
    //Serial.println(payload2);
    //Serial.println("Error on HTTP request");
  }
  http.end();
  return payload2;
}

void SendMessageToLogfile(String LOGDATA){
  url = "http://eutech.org/put-fossasat-1.php?id=17&msg="+LOGDATA;
  getUrl(url);
}

          
uint8_t getCommandFromTheNet() {
  url = "http://eutech.org/get-fossasat-1-command.php";
  String Command = getUrl(url);

  // format:
  // cmd ; text
  // cmd byte
  // text : bytes
  
  if (Command.length() == 0) { return 0x39; }
  //Serial.print("Command=");
  //Serial.print(Command);
  if (Command.length() > 3) {
    for (int i=0; i<Command.length()-2; i++ ) {
      optional[i] = Command[i+3];
//      Serial.print(char(optional[i]));
//      Serial.print(" ");
//      Serial.println(optional[i],HEX); 
    }
  }
  return Command.toInt();
}

void getTimeFromTheNet(){
  yield();
  url = "http://eutech.org/gettime.php"; 
  TIME = getUrl(url);
  Serial.print(TIME);
}

//
// AB    = 160 + 11
//
// %41 %42
// %33 %35

int bytesToInt(byte top, byte bottom) {
  top = top - 0x30;  // possible ranges now 0-9 , 0x11 - 0x16 or 0x31 - 0x36
  if ( top > 0x30 ) { top -= 0x27; } // lowcase letters, range now 10 to 15 dec
  if ( top > 0x10 ) { top -= 0x07; } //upcase letters, range now 10 to 15 dec
  //Range now 0 - 15 dec for top
  
  bottom = bottom - 0x30;  // possible ranges now 0-9 , 0x11 - 0x16 or 0x31 - 0x36
  if ( bottom > 0x30 ) { bottom -= 0x27; } // lowcase letters, range now 10 to 15 dec
  if ( bottom > 0x10 ) { bottom -= 0x07; } //upcase letters, range now 10 to 15 dec
  //Range now 0 - 15 dec for bottom
  
//  Serial.print(" top = "); Serial.print(top,HEX);
//  Serial.print(" bottom = "); Serial.print(bottom,HEX);
  return ((top*16)+bottom);
}

void getSatelliteData(uint8_t datalen) {
  // get data from DraginoHat (and satellite)

  String satdata = "";
  for (uint8_t i = 0; i < datalen; i++ ) { satdata += receivedChars2[i]; }

  Serial.print(" ");

  Serial.print(satdata);
  Serial.print(" size of received data=");
  Serial.println(satdata.length());

  indexOfS1 = satdata.indexOf(':');
  field = satdata.substring(0, indexOfS1);
  dataStr = satdata.substring(indexOfS1+1, satdata.length());

  //Serial.print(field);
  //Serial.print(" ");
  //Serial.print("sizeof dataStr=");
  //Serial.println(dataStr.length());
   
  if (indexOfS1 > 0 ) {
    Serial.print("field= "); Serial.print(field);
    Serial.print(" dataStr= "); Serial.print(dataStr);
    Serial.println("<");

    // We have a string on the format 0f,c4,f1,03,c4,00,00,00,90,01,56,fa,4c,09,00,16
    // it should be converted to an array of integers
    
    for (uint8_t i = 0; i < dataStr.length(); i++) {
      dataBuf[i] = bytesToInt(dataStr[i*3], dataStr[(i*3)+1]);
 //     Serial.print(dataBuf[i],HEX); Serial.print(" <==> ");Serial.print(char(dataStr[i*3]));Serial.println(char(dataStr[(i*3)+1]));
    }

    if (field=="ID10") { //pong received
      url = "http://eutech.org/put-fossasat-1.php?id=16"; // pong received
    }

    uint8_t datalen = (dataStr.length()/7)+1;
    if (field=="ID11") {
      DATA="";
      for (uint8_t i = 0; i< datalen; i++) {
        if (isGraph(dataBuf[i])){
          DATA+=dataBuf[i];
          //Serial.print(char(dataBuf[i]));
        }
        else
        {
          DATA+='%';
          //Serial.print('%');
          char tmp1 = dataBuf[i] / 0x10; // top nibble
          if ( tmp1 > 0x09 ) { tmp1 += 0x37; } else { tmp1 += 0x30; }
          DATA += tmp1;
          //Serial.print(char(tmp1));
         
          tmp1 = dataStr[i] & 0x0F; // bottom nibble
          if ( tmp1 > 0x09 ) { tmp1 += 0x37; } else { tmp1 += 0x30; }
          DATA += tmp1;
          //Serial.print(char(tmp1));
        }
      }
      //asbjorn url  = "http://eutech.org/put-fossasat-1.php?id=17&msg="+DATA;
    }
    if (field=="ID12") {
      // databuf[0] is message length   
      sprintf(payloadMessage,"http://eutech.org/put-fossasat-1.php?id=18&msg=BANDWIDTH:%d;SPREADING:%d;CODING:%d;PREAMBLE:%d;PREAMBLE:%d;OUTPOWER:%d;MSG:",
      dataBuf[1],                     //  bandwidth value (0x00 for 7.8 kHz, 0x07 for 125 kHz)
      dataBuf[2],                     //  spreading factor value (0x00 for SF5, 0x07 for SF12)
      dataBuf[3],                     //  coding rate (0x05 to 0x08, inclusive)
      ((dataBuf[5]*0x100)+dataBuf[4]),   //  preamble length in symbols (0x0000 to 0xFFFF; LSB first)
      dataBuf[6],                     //  CRC enabled (0x01) or disabled (0x00)
      dataBuf[7]);                    //  output power in dBm (signed 8-bit integer; -17 to 22)

      DATA="";
      for (uint8_t i = 8; i< datalen; i++) {
        if (isGraph(dataBuf[i])){
          DATA+=dataBuf[i];
          //Serial.print(char(dataBuf[i]));
        }
        else
        {
          DATA+='%';
          //Serial.print('%');
          char tmp1 = dataBuf[i] / 0x10; // top nibble
          if ( tmp1 > 0x09 ) { tmp1 += 0x37; } else { tmp1 += 0x30; }
          DATA += tmp1;
          //Serial.print(char(tmp1));
         
          tmp1 = dataBuf[i] & 0x0F; // bottom nibble
          if ( tmp1 > 0x09 ) { tmp1 += 0x37; } else { tmp1 += 0x30; }
          DATA += tmp1;
          //Serial.print(char(tmp1));
        }
      }        
      //asbjorn url  = "http://eutech.org/put-fossasat-1.php?id=18&msg="+payloadMessage+DATA;
    }
    
    if (field=="ID13") {
      // databuf[0] is message length   
 /*     
      // Battery voltage = 3920mV
      // Battery charging current = 10.09mA
      // Solar cell voltage A, B and C = 0mV

// We have a string on the format 0f,c4,f1,03,c4,00,00,00,90,01,56,fa,4c,09,00,16

Bat: 3.98V 10.5ºC ⚡️ Chrg: 3.98V 4mA
Board: 18ºC 🌡 MCU: 7ºC 
Solar: A:0.54V B:0.84V C:0
ResetCounter: 9 PowerConfig: 22
*/
      sprintf(payloadMessage,"http://eutech.org/put-fossasat-1.php?id=19&msg=BCV:%2.2f;BC:%2.2f;BV:%d;SA:%d;SB:%d;SC:%d;BAT:%2.2f;BOT:%2.2f;MCT:%2.2f;RC:%d;PWR:%d",
      ((float)(dataBuf[1]*0.02)),                         // battery charging voltage 25 * 20 mV  C4
      ((float)(((dataBuf[3]*0x100)+dataBuf[2])*0.01)),    // battery charging current * 10uA      03F1
      dataBuf[4]*20,                                      // battery voltage 25 * 20 mV  C4
      dataBuf[5]*20,                                      // solar cell A voltage 20 * 20 mV  00
      dataBuf[6]*20,                                      // solar cell B voltage 20 * 20 mV  00
      dataBuf[7]*20,                                      // solar cell C voltage 20 * 20 mV  00
      ((float)(((dataBuf[9]*0x100)+dataBuf[8])*0.01)),    // battery temp * 0.01 C
      ((float)(((dataBuf[11]*0x100)+dataBuf[10])*0.01)),  // board temp * 0.01 C
      (float)(dataBuf[12]*0.1),                           // mcu temp *0.1 C                  4C
      ((dataBuf[14]*0x100)+dataBuf[13]),                  // reset counter
      dataBuf[15]);                                       // pwr                     16  
      Serial.println(payloadMessage);
    }
    
    if (field=="ID14") {
      // databuf[0] is message length   
      sprintf(payloadMessage,"http://eutech.org/put-fossasat-1.php?id=20&msg=PACKETSNR:%d;PACKETSNR:%d",
      ((dataBuf[2]*0x100)+dataBuf[1]),   // SNR * 4 dB
      ((dataBuf[4]*0x100)+dataBuf[3]));  // RSSI * -2 dBm
      //asbjorn url  = "http://eutech.org/put-fossasat-1.php?id=20&msg="+payloadMessage;
    }
/*
    if(field=="ERR") {ERR=data.toFloat();}                      // Frequency error
    if(field=="SNR") {SNR=data.toFloat();}                      // Signal Noice
    if(field=="RSSI"){RSSI=data.toFloat();}                       // Pizza's
*/    
/*
     * ;RSSI:"+RSSI+";SNR:"+SNR+";ERR:"+ERR;

    if(field=="KEEPALIVE"){
      getTimeFromTheNet();
      digitalWrite(LED_BUILTIN, LOW);
      if (TIME.length() > 3) {
        url ="http://eutech.org/put-fossasat-1.php?keepalive="+TIME;
        getUrl(url);
      }
    }
      */
 
    Serial.println(payloadMessage);
    getUrl(String(payloadMessage));
    
 //   Serial.println("getTimeFromTheNet");
 //   getTimeFromTheNet();
    //display.showNumberDec(TIME.toInt());  //PWR.toInt()); // Display the PWR counter number value;
  } //if indexOfS0 > 0
}


uint8_t sendCommandToSatellite(uint8_t satCmd, char *optional) {
  char tmpbuf2[65];
  // send to Fossasat-1DraginoHat
  uint8_t  datalen = 0;
  if ( satCmd > 0x35 ) { return 0; }
  for (uint8_t i = 0; i < 64; i++) { tmpbuf2[i] = 0x00; }
  Serial.print("send command to Fossasat-1DraginoHat ");
  Serial.println(satCmd, HEX);
  tmpbuf2[0] = satCmd;
  sentSatCmd = satCmd; // save for retry
  if (( satCmd == 0x31 ) ||( satCmd == 0x32 ) || ( satCmd == 0x35 )) {
    uint8_t i;
    for (i=0; ( i < 63 ) && ( optional[i] > 0x1F ); i++) {
      tmpbuf2[i+1] = optional[i];
      sentOptional[i] = optional[i];  // save for retry
    }
    tmpbuf2[i+1] = 0x00; 
    datalen = i+1;
  }else{
    tmpbuf2[1]='\n';
    tmpbuf2[2]=0x00;
    datalen = 2;
  }

  while ( SWSerialTransmit(tmpbuf2) == false ) { yield(); }
}


////////////// Sat comms part ///////


//-- Setup
void setup(void) {

  // Serial port for debug, not works if shield is plugged in arduino
  
  pinMode(LED_BUILTIN, OUTPUT);
    
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Fossasat1-Lolin30");

  while ( startWifi() > 0 ) { Serial.println("No connection to wifi"); }

  sw.begin(115200, SWSERIAL_8E1, D7, D8, false, 64);

  url ="http://eutech.org/put-fossasat-1.php?keepalive=0000";
  Serial.println(url);
  getUrl(url);
  
  // Set ipaddress
  IPAddress ipaddress = WiFi.localIP();
  char registerurl[255];
  sprintf(registerurl, "http://eutech.org/putHostIP.php?h=Fossasat-1Wifi&i=%d.%d.%d.%d", ipaddress[0],ipaddress[1],ipaddress[2],ipaddress[3]);
  Serial.print("url=");
  Serial.println(registerurl);
  getUrl(registerurl);

  //display.setBrightness(0x0a); //set the diplay to maximum brightness

  //Serial.println("remember serial connection");

  digitalWrite(LED_BUILTIN, LOW);
  counter = millis();
}

void loop(){
  char tmpbuf2[65];

  int8_t len = SWSerialReceive(receivedChars2);
  if ( len > 0 ){
    Serial.println("SWSerialReceive in loop");
    getSatelliteData(len); // Get data if any
  }

  // check for new commands every 15 seconds, skip while we wait for message from satellite
  if ( counter < millis()) {
    digitalWrite(LED_BUILTIN, HIGH);
    counter = millis() + 60000;

    getTimeFromTheNet();
    satCmd = getCommandFromTheNet();  // gets optional data
    Serial.print("Uplink command 0x");
    Serial.println(satCmd,HEX);
    if (( satCmd < 0x35 ) && ( satCmd > 0x2F )) {
#ifdef DEBUG
      Serial.print("satCmd="); Serial.println(satCmd,HEX);
      Serial.print("optional="); Serial.println(optional);
#endif
 //     sendCommandToSatellite(satCmd, optional);
      sentSatCmd = satCmd;
      retryCnt = 5; // max 5 retries
    }
    digitalWrite(LED_BUILTIN, LOW);
  }
}
