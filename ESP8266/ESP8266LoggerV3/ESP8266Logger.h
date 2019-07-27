#include <Arduino.h>

#ifndef _ESP8266LOGGER

#define _ESP8266LOGGER
#define FL_DEBUG   1
#define FL_WLESS   2
#define FL_HTTPS   4
#define FL_WLOG    8
#define FL_ADSCL  16 
#define FL_ADRAW  32
#define FL_DREBT  64
#define FL_SECFL 128

#endif

void setFlag( char* iflag);

void setLHours( char* ihours);
void setLMinute( char* iminute);
void setLSecond( char* isecond);
void setTCof( int pos, char* coeff);
void setSsid( char* ssid);
void setPass( char* pass);

void setRurl( char* iurl);

void setKey( uint8_t* key, char* ibuf, int addr, int len, byte flag);
byte setByte( int addr, byte max, char* cval, byte* var, byte flag);
float setFloat( int addr, char* cval, float *fvar, byte flag );
void setString( int addr, int lmax, char* istr, byte flag);

void eedump(int size);
void eezero(int size);
