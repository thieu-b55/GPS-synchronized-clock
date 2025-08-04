/*
* MIT License
*
* Copyright (c) 2025 thieu-b55
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


/*
* GPS
* VCC     >>      +5V
* GND     >>      GND
* RX      >>      3
* TX      >>      4
*
* DS3231SN
* VCC     >>      +5V
* GND     >>      GND
* SDA     >>      SDA (18 / A4)
* SCL     >>      SCL (19 / A5)
*
* PCB board
*   R1    >>      4K7
*   R2    >>      4K7
*
* +5V ---<R10K>------- 2 (INT0)   (pullup R10K)
*                 |
*                SQW
*
* LED DISPLAY
* VCC     >>      +5V
* GND     >>      GND
* CLK     >>      13
* CS      >>      10
* DIN     >>      11
*
* SWITCH ZOMER / WINTER
* GND ----| |---- 14 (A0)
*         N.O.
*
* Contact open    >>  summer time
* Contact closed  >>  winter time
*
* LDR
* +5V ----<LDR>-----<R10K>---- GND
*                |
*                |
*              15 (A1)
*
*/

#include <SPI.h>
#include "Wire.h"
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

TinyGPSPlus gps;
SoftwareSerial ss(4, 3); //RX, TX

/*
* UTC_OFFSET enter your local UTC offset here
*/
#define UTC_OFFSET          1


#define CS                  10
#define DS3231SN            0x68
#define ZOMER_WINTER        14 
#define MINUUT_VOORBIJ      2
#define LDR                 15       

byte dec_naar_bcd(byte waarde); 
byte bcd_naar_dec(byte waarde);
void display_setup(uint8_t adres, uint8_t waarde);
void tijd_naar_led();
void smiley();
void display_digits(uint8_t adres, uint8_t digit_1, uint8_t digit_2, uint8_t digit_3, uint8_t digit_4);
void elke_minuut();

volatile bool minuut_interrupt_bool = false;
bool eerste_run_bool = true;
bool tweede_run_bool = false;
bool zomer_winter_vorig_bool = false;
bool z_w_bool = false;

int utc_uren_int;
int uren_int;
int minuten_int; 
int seconden_int;
int tiental_uur_int;
int eenheden_uur_int;
int tiental_minuut_int;
int eenheden_minuut_int;

unsigned long begin_millis = millis();

uint8_t cijfers[][11] = {{0x38, 0x44, 0x4c, 0x54, 0x64, 0x44, 0x44, 0x38},  //0
                         {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38},  //1
                         {0x38, 0x44, 0x04, 0x04, 0x08, 0x10, 0x20, 0x7c},  //2
                         {0x7c, 0x08, 0x10, 0x08, 0x04, 0x04, 0x44, 0x38},  //3
                         {0x08, 0x18, 0x28, 0x48, 0x48, 0x7c, 0x08, 0x08},  //4
                         {0x7c, 0x40, 0x78, 0x04, 0x04, 0x04, 0x44, 0x38},  //5
                         {0x18, 0x20, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38},  //6
                         {0x7c, 0x04, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20},  //7
                         {0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x44, 0x38},  //8
                         {0x38, 0x44, 0x44, 0x44, 0x3c, 0x04, 0x08, 0x30},  //9
                         {0x3c, 0x42, 0xa5, 0x81, 0xa5, 0x99, 0x42, 0x3c}}; //smiley
                         
                        
void setup() {
  delay(5000);
  //Serial.begin(115200);
  ss.begin(9600);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  pinMode(ZOMER_WINTER, INPUT_PULLUP);
  pinMode(MINUUT_VOORBIJ, INPUT);
  SPI.begin();
  delay(250);
  display_setup(0x0F, 0x00);
  delay(250);
  display_setup(0x0C, 0x00);
  display_setup(0x0C, 0x01);
  display_setup(0x09, 0x00);
  display_setup(0x0A, 0x00);
  display_setup(0x0B, 0x07);
  smiley();
  delay(2000);
  Wire.begin();
  Wire.setClock(40000);               // slow chinese DS3231 modules
  Wire.beginTransmission(DS3231SN);
  Wire.write(0x0B);
  Wire.write(0x80);
  Wire.write(0x80);
  Wire.write(0x80);
  Wire.write(0x46);
  Wire.write(0x00);
  Wire.endTransmission();
  zomer_winter_vorig_bool = digitalRead(ZOMER_WINTER);
  attachInterrupt(digitalPinToInterrupt(MINUUT_VOORBIJ), elke_minuut, FALLING);
}

void loop() {
  while((millis() - begin_millis) < 500){
    while(ss.available()){
      gps.encode(ss.read());
    }
  }
  if(zomer_winter_vorig_bool != digitalRead(ZOMER_WINTER)){
    zomer_winter_vorig_bool = digitalRead(ZOMER_WINTER);
    z_w_bool = true;
  }
  begin_millis= millis();
  if((gps.satellites.value() > 0) && ((tweede_run_bool) || (gps.time.minute() == 30))){
    Wire.beginTransmission(DS3231SN);
    Wire.write(0X00);
    Wire.write(dec_naar_bcd(gps.time.second()));
    Wire.write(dec_naar_bcd(gps.time.minute()));
    Wire.write(dec_naar_bcd(gps.time.hour()));
    Wire.endTransmission();
    tweede_run_bool = false;
    tijd_naar_led();
  }
  if(z_w_bool){
    z_w_bool = false;
    tijd_naar_led();
  }
  if(eerste_run_bool){
    eerste_run_bool = false;
    tweede_run_bool = true;
    tijd_naar_led();
  }
  if(minuut_interrupt_bool){
    minuut_interrupt_bool = false;
    Wire.beginTransmission(DS3231SN);
    Wire.write(0x0F);
    Wire.write(0x00);
    Wire.endTransmission();
    tijd_naar_led();
  }
}

byte dec_naar_bcd(byte waarde){
  return (((waarde / 10) << 4) + (waarde % 10));
}

byte bcd_naar_dec(byte waarde){
  return (((waarde >> 4) * 10) + (waarde % 16));
}

void display_setup(uint8_t adres, uint8_t waarde){
  digitalWrite(CS, LOW);
  SPI.transfer(adres);
  SPI.transfer(waarde);
  SPI.transfer(adres);
  SPI.transfer(waarde);
  SPI.transfer(adres);
  SPI.transfer(waarde);
  SPI.transfer(adres);
  SPI.transfer(waarde);
  digitalWrite(CS, HIGH);
}

void smiley(){
  display_digits(0x01, cijfers[10][0], cijfers[10][0], cijfers[10][0], cijfers[10][0]);
  display_digits(0X02, cijfers[10][1], cijfers[10][1], cijfers[10][1], cijfers[10][1]);
  display_digits(0X03, cijfers[10][2], cijfers[10][2], cijfers[10][2], cijfers[10][2]);
  display_digits(0X04, cijfers[10][3], cijfers[10][3], cijfers[10][3], cijfers[10][3]);
  display_digits(0X05, cijfers[10][4], cijfers[10][4], cijfers[10][4], cijfers[10][4]);
  display_digits(0X06, cijfers[10][5], cijfers[10][5], cijfers[10][5], cijfers[10][5]);
  display_digits(0X07, cijfers[10][6], cijfers[10][6], cijfers[10][6], cijfers[10][6]);
  display_digits(0X08, cijfers[10][7], cijfers[10][7], cijfers[10][7], cijfers[10][7]);
}

void tijd_naar_led(){
  display_setup(0X0A, (analogRead(LDR) / 128));
  Wire.beginTransmission(DS3231SN);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(DS3231SN, 2);
  minuten_int = (bcd_naar_dec(Wire.read()));
  uren_int = (bcd_naar_dec(Wire.read()));
  uren_int = uren_int + UTC_OFFSET + digitalRead(ZOMER_WINTER);
  if(uren_int > 23){
    uren_int = uren_int - 24;
  }
  if(uren_int < 0){
    uren_int = 24 + gps.time.hour() + UTC_OFFSET;
    if(uren_int == 24){
      uren_int = 0;
    }
  }
  tiental_uur_int = uren_int / 10;
  eenheden_uur_int = uren_int - (tiental_uur_int * 10);
  tiental_minuut_int = minuten_int / 10;
  eenheden_minuut_int = minuten_int - (tiental_minuut_int * 10);
  display_digits(0x01, cijfers[tiental_uur_int][0], cijfers[eenheden_uur_int][0], cijfers[tiental_minuut_int][0], cijfers[eenheden_minuut_int][0]);
  display_digits(0X02, cijfers[tiental_uur_int][1], cijfers[eenheden_uur_int][1], cijfers[tiental_minuut_int][1], cijfers[eenheden_minuut_int][1]);
  display_digits(0X03, cijfers[tiental_uur_int][2], cijfers[eenheden_uur_int][2] + 1, cijfers[tiental_minuut_int][2], cijfers[eenheden_minuut_int][2]);
  display_digits(0X04, cijfers[tiental_uur_int][3], cijfers[eenheden_uur_int][3], cijfers[tiental_minuut_int][3], cijfers[eenheden_minuut_int][3]);
  display_digits(0X05, cijfers[tiental_uur_int][4], cijfers[eenheden_uur_int][4], cijfers[tiental_minuut_int][4], cijfers[eenheden_minuut_int][4]);
  display_digits(0X06, cijfers[tiental_uur_int][5], cijfers[eenheden_uur_int][5] + 1, cijfers[tiental_minuut_int][5], cijfers[eenheden_minuut_int][5]);
  display_digits(0X07, cijfers[tiental_uur_int][6], cijfers[eenheden_uur_int][6], cijfers[tiental_minuut_int][6], cijfers[eenheden_minuut_int][6]);
  display_digits(0X08, cijfers[tiental_uur_int][7], cijfers[eenheden_uur_int][7], cijfers[tiental_minuut_int][7], cijfers[eenheden_minuut_int][7]);
}

void display_digits(uint8_t adres, uint8_t digit_1, uint8_t digit_2, uint8_t digit_3, uint8_t digit_4){
  digitalWrite(CS, LOW);
  SPI.transfer(adres);
  SPI.transfer(digit_1);
  SPI.transfer(adres);
  SPI.transfer(digit_2);
  SPI.transfer(adres);
  SPI.transfer(digit_3);
  SPI.transfer(adres);
  SPI.transfer(digit_4);
  digitalWrite(CS, HIGH);
}

void elke_minuut(){
  minuut_interrupt_bool = true;
}
