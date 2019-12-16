#include "SoftwareSerial.h"
#include "SoftwareSerialStable.h"
#include "arduino.h"

#define DEBUG
#define DEBUG2
//#define DEBUG3

/* define for arduino
SoftwareSerial sw(3,4); // RX, TX

and for ESP8266 Nodemcu 3.0
#define D7 (13)  //Yellow to Arduino pin 4
#define D8 (15)  //Orange/Blue to ARduino pin 3
SoftwareSerial sw;

sw.begin(115200, SWSERIAL_8E1, D7, D8, false, 64);
*/

// global variables
char tmpbuf[65];
const int numChars = 65;
char receivedChars[numChars];
char receivedChars3[numChars];
bool newData = false;
bool sendMsgActive = false;
bool receiveMsgActive = false;
int8_t partNo = -1;
int8_t nextPartNo = -1;  

uint8_t recvWithStartEndMarkers();


// ===== Send part =====
bool transmitAllowed = true;

void myDelay(int del) {
  unsigned long timer = millis();
  while ( millis() - timer < del ) {} // wait
}

uint8_t getAck(char *tmpbuf) {
  for (uint8_t i = 0; (tmpbuf[i]>0x00 ); i++ ) {
#ifdef DEBUG3
    Serial.print("getAck i=");
    Serial.print(i); Serial.print(" ");
    if ( isGraph(receivedChars[i]) ) { Serial.print(char(receivedChars[i]));
    }else{Serial.print("%0x"); Serial.print(receivedChars[i],HEX); }
    Serial.print(" <==> ");
    if ( isGraph(tmpbuf[i]) ) { Serial.print(char(tmpbuf[i]));
    }else{Serial.print("%0x"); Serial.print(tmpbuf[i],HEX); }
    Serial.println();
#endif
    if ( receivedChars[i] != tmpbuf[i] ) { myDelay(150); transmitAllowed = true; return -1; }
  }
  myDelay(150); 
  transmitAllowed = true;
  return 0;
}

void SWSerialWrite(char *tmpbuf) {
// the 3 lines must be here
  Serial.print("SWSerialWrite ");
  Serial.print(char(tmpbuf[0]));
  Serial.println(char(tmpbuf[1]));
  
  sw.print('<'); // start marker
  uint8_t i = 0;
  while ((i < 64) && ( tmpbuf[i] > 0x00 )) {
    sw.print(tmpbuf[i]);
    i++;
  }
  sw.print('>'); // end marker
 
#ifdef DEBUG3
  Serial.print("I send =>");
  i=0;
  while (i < 12) {
    if ( isGraph(tmpbuf[i]) ) { Serial.print(char(tmpbuf[i]));
    }else{if (tmpbuf[i] > 0x00) {Serial.print("%0x"); Serial.print(tmpbuf[i],HEX); }}
    i++;
  }
  Serial.println();
#endif
  
  myDelay(150);  // wait 150 ms for transfer to be done
}

// ===== Receive part =====
//////////////////////////////////////////////////////
// https://forum.arduino.cc/index.php?topic=396450
uint8_t recvWithStartEndMarkers() {
  bool recvInProgress;
  uint8_t ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  uint8_t rc;

  ndx=0;
  recvInProgress = false;
  for (uint8_t i=0; i<63; i++) { receivedChars[i] = 0x00; }
  uint8_t i=0;
  newData = false;

  while ((sw.available() > 0 ) && ( newData == false ) && (ndx < numChars)) {
    rc = sw.read();

#ifdef DEBUG3
    Serial.print(rc, HEX);Serial.print(" ");
    if ( isGraph(rc) ) { Serial.print(char(rc)); Serial.print(" ");}
#endif

    if (recvInProgress == true) {
      if (rc == endMarker) {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        newData = true;
      } else {
        receivedChars[ndx] = rc;
        ndx++;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
#ifdef DEBUG3
  Serial.println();
#endif
  return ndx;
}

void sendAck(char *tmpbuf){
#ifdef DEBUG3
  Serial.print("sendAck=>");
  uint8_t i=0;
  while ((i < 12) && ( tmpbuf[i] > 0x00 )) {
    if ( isGraph(tmpbuf[i]) ) { Serial.print(char(tmpbuf[i]));
    }else{Serial.print("%0x"); Serial.print(tmpbuf[i],HEX); }
    i++;
  }
  Serial.println();
#endif
  tmpbuf[0] = 'A';
  SWSerialWrite(tmpbuf);
}

void sendNack(){
  char nack[5];
  nack[0] = 'N';
  nack[1] = 'A';
  nack[2] = 'C';
  nack[3] = 'K';
  nack[4] = '\0';
#ifdef DEBUG
  Serial.print("sendNack=>");
  Serial.println(nack);
#endif
  SWSerialWrite(nack);
}

uint8_t P0_7(){
  partNo = receivedChars[1] - 0x30;
  // check the sequence
  if (( partNo == ( nextPartNo - 1 ) ) || ( partNo == nextPartNo )) {
    for (uint8_t i = 0; i < 8; i++ ) { receivedChars3[i+(partNo*8)] = receivedChars[i+2]; }
    nextPartNo = partNo + 1;
#ifdef DEBUG3
    Serial.print("part ");
    Serial.print(partNo);
    Serial.print(" received, next is ");
    Serial.println(nextPartNo);
#endif
    sendAck(receivedChars);
    return 0;
  } else { // partNo != nextPartNo
#ifdef DEBUG2
    Serial.print("Error in sequence received=");
    Serial.print(partNo);
    Serial.print(" expected=");
    Serial.println(nextPartNo);
#endif
    newData = false;
    sendNack();
    return -1;
  }
}

uint8_t P8(){
  uint8_t datalen;
  for (datalen = 0; ((datalen < 64 ) && ( receivedChars3[datalen] > 0x00 )) ; datalen++) {}
  sendAck(receivedChars);
  return datalen;
}

uint8_t P9(){
  // Check length marker ( P9 )
  String receivedString = "";
  receivedString = (char)receivedChars[2];
  receivedString += (char)receivedChars[3];
  receivedString += '\0';
  uint8_t totallen = receivedString.toInt();
  
  sendAck(receivedChars);
  return 0;
  // we got the total length, continue
}

uint8_t Full( ) {
  uint8_t datalen;
  for (datalen = 0; ((datalen < 8) && (receivedChars[datalen+1] > 0x00)); datalen++ ) {
    receivedChars3[datalen] = receivedChars[datalen+1];
  }
  receivedChars3[datalen] = 0x00;
  sendAck(receivedChars);
  return(datalen);
}

int8_t receivePartMsg(char *tmpbuf){
  // receive 1 to 8, 8 uint8_t parts and put them together
  // protocol:
  // P9 total length
  // P8 end of message
  // Pn (n: 0..7) part messages

  // get the total length of the transferred data
  for (uint8_t i = 0; i < 12; i++ ) { receivedChars[i] = 0x00; }
  uint8_t datalen = recvWithStartEndMarkers();
  if ( datalen == 0 ) {
    newData = false;
    return -1;
  }

#ifdef DEBUG3
  Serial.print("receivePartMsg: data received=>");
  for (uint8_t i = 0; i < 12; i++ ) { Serial.print(char(receivedChars[i])); }
  Serial.println("<=");    
#endif

  if ( receivedChars[0] == 'A' ) { receivedChars[0] = 'P'; return getAck(tmpbuf); }

  if ( receivedChars[0] == 'N' && receivedChars[1] == 'A' && receivedChars[2] == 'C' && receivedChars[3] == 'K' && receivedChars[4] == '\0' ) { return -1; }

  if ( receivedChars[0] == 'F' ) { return Full(); }
  
  if ( receivedChars[0] == 'P' ) {
    if (( receivedChars[1] == '9' ) && ( partNo == -1 )) { nextPartNo = 0; return P9(); } // start message
    if (( receivedChars[1] == '8' ) && ( nextPartNo > 0 )) { return P8(); } // end message
    if (( receivedChars[1] < '8' ) && ( receivedChars[1] >= '0' )) { return P0_7(); } // the message
  }
  sendNack();
  return -1;
}

int8_t SWSerialReceive(char *receivedChars2){
  // reset output buffer
//  if ( sendMsgActive == true ) { return 0; } // waiting for ack or nack
//  receiveMsgActive = true;

 // no data yet
  int8_t datalen = 0;
  unsigned long interval = millis() + 1000;

  partNo = -1; // global variable
  while (( datalen == 0 ) && ( datalen > -1 ) && ( interval > millis())) {
    if ( sw.available()) {
      transmitAllowed = false;
      datalen = receivePartMsg(receivedChars2); // just need some buffer
      transmitAllowed = true;
    }
    unsigned long waitDelay = millis() + 20;
    while ( waitDelay > millis() ) {} // wait 20 milliseconds
  }
#ifdef DEBUG4
  if ( datalen > 0 ) {
    Serial.println("new data");
  }else{
    Serial.print(millis());
    Serial.println(" timeout 2");
  }
  Serial.print("datalen received="); Serial.println(datalen);
#endif
  if ( datalen > 0 ) {
    for (uint8_t i = 0; i < datalen; i++) { receivedChars2[i] = receivedChars3[i]; }
  }else{
    for (uint8_t i = 0; i < 63; i++) { receivedChars2[i] = 0x0; }
  }
  newData = false;
  receiveMsgActive = false;
  return datalen;
}

//////////////////////////////////////////////////
// Full message
//////////////////////////////////////////////////

uint8_t sendFullMsg(char *tmpbuf) {
  char partMsg[10];
  uint8_t errCnt = 0;

#ifdef DEBUG
  Serial.println(tmpbuf);
#endif
  int8_t i;
  partMsg[0] = 'F';  // Full data frame
  for (i = 0 ; i < 9; i ++) {
    partMsg[i+1] = tmpbuf[i];
  }
  partMsg[i+1] = 0x00; // end of data
  
  while ( errCnt < 10 ) {
        
    // wait for acknowledge from peer
    SWSerialWrite(partMsg);
    if ( receivePartMsg(partMsg) == 0 ) {
      // if end of data return 0 == success
      return 0;
    }else{
      // no acknowledge, retry max 10 times
#ifdef DEBUG5
      Serial.println("no acknowledge, retry max 10 times");
#endif
      errCnt++;
      myDelay ( 30 * errCnt );
    }
  }
#ifdef DEBUG
  Serial.println(errCnt);
#endif
  return 1;
}

//////////////////////////////////////////////////
// Message sent in parts
//////////////////////////////////////////////////
uint8_t sendPartMsg(char *tmpbuf, uint8_t totallen) {
  // divide the message in parts of 8 byte, to avoid data corruption from WiFi processing
  char partMsg[12];
  uint8_t errCnt = 0;
   int8_t partNo = 0;
  uint8_t endOfData = 0;
  uint8_t bufferEmpty = 0;
  uint8_t i = 0;
  
#ifdef DEBUG3
  Serial.print("sendPartMsg ");
  Serial.println(partMsg);
#endif

  partNo = 0;
  errCnt = 0;
  bufferEmpty = 0;
  for (uint8_t i=0; i<12; i++) { partMsg[i] = 0x00; }

  // set header P9 + length of message
  while (( bufferEmpty == 0) && ( errCnt < 10 ) && ( partNo == 0 )) {
    sprintf(partMsg, "P9%2d", totallen);
    
    // wait for acknowledge from peer
    SWSerialWrite(partMsg);
    if ( receivePartMsg(partMsg) == 0 ) {
      bufferEmpty = 1;
    }else{
      // no acknowledge, retry max 10 times
      errCnt++;
      myDelay ( 30 * errCnt );
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // set header Pn ( n is 0 to 7 ) + 8 chars
  //////////////////////////////////////////////////////////////////////////

  if ( errCnt < 10 ) { errCnt = 0; } else { partNo = 9; } // no good length, skip rest
  bufferEmpty = 0;

  while (( errCnt < 10 ) && ( bufferEmpty == 0) && ( partNo < 8 )) {
    partMsg[0] = 'P';
    partMsg[1] = partNo + 0x30;

    // Copy data, note the offsets, tmpbuf is 0x00 terminated (and rest of buffer is 0x00 as well)
    for (i = 0; i < 8; i++ ) {
      partMsg[i+3] = char(0x00); // terminate
      partMsg[i+2] = tmpbuf[i+(partNo*8)];
      if ( partMsg[i+2] == 0x00 ) { bufferEmpty = 1; }
    }

    // wait for acknowledge from Fossasat-Wifi
    SWSerialWrite(partMsg);
    if ( receivePartMsg(partMsg) == 0 ) {
      // if end of data set flag
      partNo++; // next 8 uint8_t part
      errCnt = 0;
    }else{
      // no acknowledge, retry max 10 times with increased delay
#ifdef DEBUG5
      Serial.print("no acknowledge, retry max 10 times >>");
      Serial.println(errCnt);
#endif
      errCnt++;
      myDelay ( 30 * errCnt );
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // set header P8 end of data
  //////////////////////////////////////////////////////////////////////////

  if ( errCnt < 10 ) { errCnt = 0; } else { partNo = 9; } // no good message
  while (( errCnt < 10 ) && ( partNo < 9 )) {
    // set header P8 end of message
    partMsg[0] = 'P';
    partMsg[1] = 0x38;
    partMsg[2] = 0x00;
    
#ifdef DEBUG3
    // Send the message P8
    Serial.println("end of message");
    if ( errCnt == 0 ) { Serial.println(partMsg); }
#endif
    
    // wait for acknowledge from peer
    SWSerialWrite(partMsg);
    if ( receivePartMsg(partMsg) == 0 ) {
      // if end of data set flag
	    partNo = 9;
      return 0; // success
    }else{
      // no acknowledge, retry max 10 times
      errCnt++;
      myDelay ( 30 * errCnt );
    }
  }
#ifdef DEBUG
  Serial.print("End of sendPartMsg");
#endif
  return 1;
}

bool SWSerialTransmit(char * optional) {
  // find datalen
  uint8_t datalen;
  uint8_t retryCount;
  uint8_t rc;
  unsigned long timer = 0;

  for (datalen=0; ( datalen < 63 ) && ( optional[datalen] > 0x00 ); datalen++) {}
  rc = 1;
  retryCount = 0;
  
  timer = millis();
  while (( transmitAllowed == false ) && ( millis() - timer > 1000 )) { yield(); }
  if ( transmitAllowed == true ) {
    sendMsgActive = true;
    //if ( datalen < 9 ) { while (( retryCount < 10 ) && ( rc > 0 )) { rc = sendFullMsg(optional); retryCount++; } }
    //else               { 
    while (( retryCount < 10 ) && ( rc > 0 )) { rc = sendPartMsg(optional, datalen); retryCount++; }// }
    sendMsgActive = false;
    return true;
  }
  return false;
}
