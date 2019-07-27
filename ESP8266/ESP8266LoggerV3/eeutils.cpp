#include <Arduino.h>
#include <EEPROM.h>

#include "ESP8266Logger.h"

void eezero(int size){
   for( int eaddr = 0; eaddr <size; eaddr ++ ){
      EEPROM.write( eaddr, 0 );
   }
   EEPROM.commit();
   if(Serial) Serial.printf("EEPROM cleared\n"); 
}

void eedump(int size){
   if(Serial){
      for( int eaddr = 0; eaddr <size; eaddr ++ ){
         byte eval = EEPROM.read( eaddr );
         Serial.printf( "0x%02X ", eval);
         if((eaddr+1) % 16 == 0){
           Serial.printf("  |  ");
           for(int il = 0; il<16; il++){  
                byte b = EEPROM.read( eaddr - 15 + il);
                if(b > 31 && b < 127 ){
                  Serial.printf("%c", b );
                } else {
                  Serial.printf(".");
                }
           }
           Serial.printf("\n");
         }
      }
   }
}

byte setByte( int addr, byte max, char* cval, byte* var, byte flag){

  String sval = String(cval);
  byte bVal = sval.toInt();
  if(bVal > max) bVal = max;
  if(Serial) Serial.printf( " Setting Byte: 0x%02X \n", bVal );
  
  EEPROM.write( addr, bVal );
  if( (flag & FL_DEBUG) == FL_DEBUG)
     if(Serial) Serial.printf( "  0x%2X : 0x%02X \n", addr, bVal );
  EEPROM.commit();

  if(var != NULL){
    *var = bVal;
  }
  return  bVal; 
}

float setFloat( int addr, char* cval, float *fvar, byte flag ){

  String sval = String(cval);
  float fVal = sval.toFloat();
  if(Serial) Serial.printf( " Setting Float: %f \n", fVal );

  byte *fptr = (byte *) &fVal;
  for( int b = 0; b < 4; b++ ){  
     EEPROM.write( addr++, *(fptr + b) );
     if( (flag & FL_DEBUG) == FL_DEBUG)
        if(Serial) Serial.printf( "  0x%2X : 0x%02X \n", addr, *(fptr + b ) );
  }
  EEPROM.commit();

  if(fvar != NULL){
    *fvar = fVal;
  }
  return  fVal; 
}

void setString( int addr, int lmax, char* istr, byte flag){
  if(Serial) Serial.printf( "Setting String: %s \n", istr );
  int i = 0;
  for(i = 0; i < lmax && istr[i] > 0; i++){
     EEPROM.write(addr+i, istr[i]);
     if( (flag & FL_DEBUG) == FL_DEBUG)
       if(Serial) Serial.printf( "0x%02X : %c \n", addr+i, istr[i] );
  }
  EEPROM.write(addr+i, 0);
  if(Serial) Serial.printf( "0x%02X : NULL\n", addr+i);
  EEPROM.commit();
}

void setKey( uint8_t* key, char* ibuf, int addr, int len, byte flag){

  String sbuf = String( ibuf ); 
  char cfinger[32];
  boolean fstat = true;
  unsigned char cbuf[32];
  
  for( int i = 0; i < len; i++){
    if( 3*i < sbuf.length() ){
      String digit = sbuf.substring(3*i,3*i+2);
      sbuf.substring(3*i,3*i+2).getBytes(cbuf,3);
      cfinger[i] = strtol( (const char*) cbuf, NULL, 16);
    } else {
      fstat = false;
    }
  }
  if( fstat ){
    for( int j=0; j <len; j++ ){
      EEPROM.write( addr+j, (byte) cfinger[j] );
      key[j] = cfinger[j];
      if(Serial) Serial.printf( "%02x ",cfinger[j]  );
    }
    if(Serial) Serial.println();
    EEPROM.commit();
    if(Serial) Serial.printf( "Get Key: " );
   for( int j=0; j <len; j++ ){
      byte ch = EEPROM.read( addr+j );
      if(Serial) Serial.printf( "%02x:",ch  );
    }
    if(Serial) Serial.println("");
  } else{
     if(Serial) Serial.printf( "Set Key ERROR: %s\n", ibuf );
  }
}
