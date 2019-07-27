#include <Arduino.h>

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <PolledTimeout.h>

#include "ESP8266Logger.h"

#define ADC_SCNT 16
#define EESIZE 512

const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

uint8_t fingerprint[20] = {0x2F, 0xF0, 0x8D, 0x6B, 0x6B, 
                           0x46, 0x75, 0x36, 0xA4, 0x07, 
                           0x96, 0xDE, 0x8B, 0xF2, 0xB7, 
                           0x40, 0x03, 0x4F, 0x64, 0x68};

char cleartext[20];
char ciphertext[40];

#define FLAG_ADDR 0
#define LHRS_ADDR 1
#define LMIN_ADDR 2
#define LSEC_ADDR 3
#define SSID_ADDR 4
#define PASS_ADDR 64 
#define FING_ADDR 128
#define AKEY_ADDR 208    
#define TCOF_ADDR 224
#define RURL_ADDR 256

ESP8266WiFiMulti WiFiMulti;

byte eval;

byte flag = 0;    // flags
byte lhrs =  0;   // log time interval (hours)
byte lmin =  0;   // log time interval (minutes)
byte lsec =  0;   // log time interval (seconds)
char wifi_ssid[64];
char wifi_pass[64];

float tcof[8];
char akey[16];  // sha-1 encryption key 

uint8_t ee_fingerprint[20];

char rurl[256];   // remote url

char inByte = 0;
char ibuffer[512];
char iop = ' ';
int ipos = 0;

bool connected = false; // connection flag

long cdel = 0;

long msgid = 1;

void setup() {
  
  EEPROM.begin(512);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(A0,INPUT);
  Serial.begin(115200);
  delay(1000);  
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.flush();
  delay(1000);
  
  digitalWrite(LED_BUILTIN, HIGH);
  if(Serial){
      connected = true;
      Serial.println("ESP8266 WiFi Logger (V3)");        
  }

  byte ssidDone = 1;
  byte passDone = 1;
  byte rurlDone = 1;
  byte akeyDone = 1;
  int ssidPos = 0;
  
  for( int eaddr = 0; eaddr <EESIZE; eaddr ++ ){
    
       byte eval = EEPROM.read( eaddr );  // addr: 0   flags 

       // flags
       if( eaddr == FLAG_ADDR ){
          flag = eval;
          if(Serial){
             Serial.printf("Flags: %d\n", flag);

             if( (flag & FL_DEBUG) == FL_DEBUG ){
               Serial.printf(" DEBUG: ON\n");              
             }else{
               Serial.printf(" DEBUG: OFF\n");              
             }

             if( (flag & FL_WLESS) == FL_WLESS ){
               Serial.printf(" WIRELESS: ON\n");              
             }else{
               Serial.printf(" WIRELESS: OFF\n");              
             }

             if( (flag & FL_HTTPS) == FL_HTTPS ){
               Serial.printf(" HTTPS: ON\n");              
             }else{
               Serial.printf(" HTTPS: OFF\n");              
             }   

             if( (flag & FL_DREBT) == FL_DREBT ){
               Serial.printf(" DAILY REBOOT: ON\n");              
             }else{
               Serial.printf(" DAILY REBOOT: OFF\n");              
             }   
          }
       }

       // log interval (hours)
       if( eaddr == LHRS_ADDR ){
          lhrs = eval;
           if(Serial)
              Serial.printf("Log interval(hours): %d\n", lhrs);
       }

       // log interval (minutes)
       if( eaddr == LMIN_ADDR ){
          lmin = eval;
           if(Serial)
              Serial.printf("Log interval(minutes): %d\n", lmin);
       }
       // log interval (seconds)
       if( eaddr == LSEC_ADDR ){
          lsec = eval;
           if(Serial)
              Serial.printf("Log interval(seconds): %d\n", lsec);
       }
       
       if( eaddr >= SSID_ADDR && eaddr < PASS_ADDR ){
           // get wifi ssid 
           if( ssidDone > 0 ){   
              wifi_ssid[eaddr - SSID_ADDR] = eval;
              ssidDone = eval;
           } else {
              wifi_ssid[eaddr - SSID_ADDR] = eval;
              if( Serial && (eaddr == PASS_ADDR-1) )
                 Serial.printf("SSID: %s\n", wifi_ssid);
          }
       }

       if( eaddr >= PASS_ADDR && eaddr < FING_ADDR ){
           // get wifi pass
           if( passDone > 0 ){
              wifi_pass[eaddr - PASS_ADDR] =  eval;
              passDone = eval;
           } else {
              wifi_pass[eaddr - PASS_ADDR] = eval;
              if( Serial && (eaddr == FING_ADDR-1) )
                 Serial.printf("PASS: %s\n", wifi_pass);
           }
       }
       
       if( eaddr >= FING_ADDR && eaddr - FING_ADDR < 20 ){
           // get remote url           
           ee_fingerprint[eaddr - FING_ADDR] = eval;
       }

       if( eaddr >= AKEY_ADDR && eaddr < TCOF_ADDR ){
           // get wifi pass
           if( akeyDone > 0 ){
              akey[eaddr - AKEY_ADDR] =  eval;
              akeyDone = eval;
           } else {
              akey[eaddr - AKEY_ADDR] = eval;
              if( Serial && (eaddr == TCOF_ADDR-1) )
                 Serial.printf("AES KEY: %s\n", akey);
           }
       }

       if( eaddr >= RURL_ADDR && eaddr-RURL_ADDR <512){
           if( rurlDone > 0 ){
              rurl[eaddr - RURL_ADDR] =  eval;
              rurlDone = eval;
           } else {
              rurl[eaddr - RURL_ADDR] = eval;
              if(Serial && ( eaddr == RURL_ADDR - 256))
                 Serial.printf("RURL: %s\n", rurl);
           }        
       }

       if( eaddr >= TCOF_ADDR && eaddr - TCOF_ADDR < 8*4){
           byte *tcb = (byte *) tcof;
           *(tcb+(eaddr - TCOF_ADDR)) = eval;           
       }       
  }

  for(int c =0; c < 5; c++){
      Serial.printf("  tcof[%i]: %7.4e\n",c,tcof[c]);
  }

  if( (flag & FL_WLESS) == FL_WLESS ){               
     WiFi.mode(WIFI_STA);     
     WiFiMulti.addAP("BOYER HALL","611paulboyer");
     if(Serial){
        Serial.println("WiFI: ON");
        Serial.print("SSID:");
        Serial.println(wifi_ssid);       
     } 
     
    int ct = 0;   
    while( WiFiMulti.run() != WL_CONNECTED && ct < 20){
      delay(500);
      ct++;
      if(Serial) Serial.print(".");
    }
    if(Serial){
       Serial.println("");
       Serial.printf( "WiFi SSID: %s :\n",wifi_ssid );
       Serial.print(" WiFi IP: ");
       Serial.println( WiFi.localIP() );
    }
  } else {
    if(Serial){
       Serial.println("WiFi: OFF");
    }
  }
}

void wifiLogger(float adc){

   if( (flag & FL_WLOG) != FL_WLOG ){
      return;  
   }
   
   BearSSL::WiFiClientSecure client;

   if( (flag & FL_SECFL) == FL_SECFL ){
     client.setFingerprint( ee_fingerprint );
     if(Serial) Serial.println( "[HTTPS] Secure Flag: ON" );
   } else {
     client.setInsecure();
     if(Serial) Serial.println( "[HTTPS] Secure Flag: OFF" );
   }
   HTTPClient https;
   
   digitalWrite(LED_BUILTIN, LOW);

   String url = String(rurl);
   url = url + String("&mid=") + String(msgid);
   msgid++;

   url = url + String("&err=0");
   
   if( (flag & FL_ADSCL) == FL_ADSCL ){
     url = url + String("&a0=")+ String(ad2temp(adc),2);
   }
   if( (flag & FL_ADRAW) == FL_ADRAW ){
     url = url + String("&a1=") + String(adc);
   }
   if(Serial){
      Serial.println( "[HTTPS] URL: " + url );
      if( (flag & FL_DEBUG) == FL_DEBUG ){
        Serial.println( "[HTTPS] begin..."); 
      }
   }
   if( https.begin(client,url) ){
      if(Serial){
         if( (flag & FL_DEBUG) == FL_DEBUG ){
            Serial.println("[HTTPS] GETing...");
        }
      }     
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
         // HTTP header has been send and Server response header has been handled
         if(Serial){
              Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
         }     
         // file found at server
         if( httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            if(Serial){
               if( (flag & FL_DEBUG) == FL_DEBUG ){
                  Serial.println(payload);
               }
            }
         }
      } else {
         if(Serial){
            Serial.printf( "[HTTPS] GET... failed, error: %s\n", 
                          https.errorToString(httpCode).c_str());
         }
      }
      https.end();
   } else {
      if(Serial){
         Serial.print("[HTTPS] Unable to connect, WiFi status: ");
         Serial.print("[HTTPS] Status: ");
      }
      if( WiFi.status() !=  WL_CONNECTED ){
         if( Serial ){
            Serial.println(" NOT CONNECTED (" + String(WiFi.status()) + ")" );
            Serial.println("[LOG] Reboot in progress");
            Serial.flush();
         }
         delay(5000);
         ESP.restart();
      } else {
         if(Serial){
            Serial.println(" CONNECTED");
         }
      }      
   }
   digitalWrite(LED_BUILTIN, HIGH);
}

float ad2temp( float adc ){
  
  float temp = 0.0; //tcof[4]*adc;
  for(int i = 4; i>0; i--){
    temp = (temp + tcof[i])*adc;        
  }
  return temp + tcof[0];
}

void loop() {

  if( Serial ){
    if(Serial.available() > 0) {
      inByte = Serial.read();   // read the incoming non-digit
      if( (flag & FL_DEBUG) == FL_DEBUG){
        Serial.printf( ":%d:\n",inByte );
      }
      if( ipos <512 ){
        if(ipos == 0){ 
          iop = inByte;
          ipos++;
        } else {
          ibuffer[ipos-1] = inByte;
          ipos++;
          if(inByte == '\n' ){
             ibuffer[ipos-2] = 0;  // terminate buffer
             if( (flag & FL_DEBUG) == FL_DEBUG){
               Serial.printf( "OP: %c Buffer:%s:\n",iop, ibuffer );
             }
             ipos=0;
             
             if( iop == 'F' ) setFlag( ibuffer );    // flag
             if( iop == 'I' ) setSsid( ibuffer );    // ssid
             if( iop == 'P' ) setPass( ibuffer );    // pass           
             if( iop == 'R' ) setRurl( ibuffer);     // remote url
             if( iop == 'T' ) setKey( ee_fingerprint, ibuffer, FING_ADDR ,20, flag);  // set fingerprint
             if( iop == 'H' ) setLHours( ibuffer);   // log hour interval
             if( iop == 'M' ) setLMinute( ibuffer);  // log minute interval
             if( iop == 'S' ) setLSecond( ibuffer);  // log second interval
             if( iop == 'X' ) eedump(EESIZE);        // dump eeprom
             if( iop == 'Z' ) eezero(EESIZE);        // zero eeprom
             
             // 4th order polynomial 
             // out = A + B*ad0 + C * ad0^2 + D * ad0^3 + E * ad0^4
             //----------  
             if( iop == 'A' ) setTCof( 0, ibuffer);      // A coefficient
             if( iop == 'B' ) setTCof( 1, ibuffer);      // B coefficinet
             if( iop == 'C' ) setTCof( 2, ibuffer);      // C coefficient
             if( iop == 'D' ) setTCof( 3, ibuffer);      // D coefficinet
             if( iop == 'E' ) setTCof( 4, ibuffer);      // E coefficinet

             if( iop == 'K' ) setKey( ee_fingerprint, ibuffer,AKEY_ADDR, 16, flag );   // SHA-1 key
             
             iop = ' ';
         }
       }
      }
    }
  }

  delay(100);
  cdel++;

  if( cdel > 10L * (lsec + lmin * 60 + lhrs *60 *60 ) ){ 
    cdel = 0L;
    if( lsec + lmin + lhrs > 0 ){   // log me....
       if( (flag & FL_DEBUG) == FL_DEBUG ){
           if( Serial ) Serial.print( "[ADCLOG] A0(raw): " );
       }
       float adAvg = 0.0;
 
       for(int s = 0; s < ADC_SCNT; s++ ){ 
          int ad =  analogRead(A0); 
          adAvg = adAvg + ad;
          if(Serial){
             if( (flag & FL_DEBUG) == FL_DEBUG ){
                Serial.print( ad );
                Serial.print( " " );
             }
          }          
          delay(100);
       }
       if(Serial){
         if( (flag & FL_DEBUG) == FL_DEBUG ){
            Serial.println();
            Serial.flush();
         }
       }

       if( (flag & FL_DEBUG) == FL_DEBUG ){
         if(Serial){ 
            Serial.printf("[ADCLOG] A0(avg): %8.3f\n", adAvg / ADC_SCNT );
            Serial.printf("[ADCLOG] A0(conv):%8.3f\n",ad2temp(adAvg / ADC_SCNT));
         }
       }
       if( (WiFiMulti.run() == WL_CONNECTED)) {
          wifiLogger( adAvg / ADC_SCNT ); 
              
          if(Serial){ 
             if( (flag & FL_DEBUG) == FL_DEBUG ){
                Serial.println();
             }
          }
       } else {
          if(Serial){ 
             if( (flag & FL_DEBUG) == FL_DEBUG ){
                Serial.println(WiFiMulti.run());
             }
          }
       }
    }  
  }

  if(cdel > 10L * 60 * 60 * 24 ){
     if( (flag & FL_DREBT) == FL_DREBT ){
        if(Serial){          
           Serial.println("[LOG] Daily reboot in progress" );
           Serial.flush();
        }
        delay(5000);
        ESP.restart();
     }  
  }
}

void setFlag( char* iflag){  
  if(Serial) Serial.printf( "Setting Flag: %s \n", iflag );
  byte nflag = setByte(FLAG_ADDR, 255, iflag, NULL,flag );

  if( ((nflag & FL_WLESS) == FL_WLESS ) && 
      not ((flag & FL_WLESS) == FL_WLESS ) ){
     
     // turn on wifi
        
     //WiFi.mode(WIFI_STA);
    
  }

  if( not((nflag & FL_WLESS) == FL_WLESS ) && 
        ((flag & FL_WLESS) == FL_WLESS ) ){

      // turn off wireless 
  }
  flag = nflag; 
}

void setLHours( char* ihours){  
  if(Serial) Serial.printf( "Setting Log Hours: $s\n", ihours );
  setByte( LHRS_ADDR, 255, ihours, &lhrs, flag );  
}

void setLMinute( char* iminute){  
  if(Serial) Serial.printf( "Setting Log Minutes: %s\n", iminute );
  setByte( LMIN_ADDR, 60, iminute, &lmin, flag );  
}
void setLSecond( char* isecond){  
  if(Serial) Serial.printf( "Setting Log Seconds: %sn", isecond );
  setByte( LSEC_ADDR, 60, isecond, &lsec, flag );  
}

void setTCof( int pos, char* coeff){  
  if(Serial) Serial.printf( "Setting Temperature Coefficient: %i %s\n", pos, coeff );
  setFloat( TCOF_ADDR + pos*4, coeff, NULL, flag );  
}

void setSsid( char* ssid){
  if(Serial) Serial.printf( "Setting SSID: %s \n", ssid );
  setString( SSID_ADDR, 32, ssid, flag ); 
}

void setPass( char* pass){
  if(Serial) Serial.printf( "Setting Pass: %s \n", pass );
  setString( PASS_ADDR, 127, pass, flag );
}

void setRurl( char* iurl){
  setString( RURL_ADDR, 255, iurl, flag );
}
