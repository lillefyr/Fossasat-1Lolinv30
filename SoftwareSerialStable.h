#ifndef _SOFTWARE_SERIAL_STABLE
#define _SOFTWARE_SERIAL_STABLE

int8_t SWSerialReceive(char *);
bool SWSerialTransmit(char *);
extern SoftwareSerial sw;
#endif
