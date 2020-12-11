/*
 Single 7 segment display digit clock.

 Hardware:
 * Adafruit Bluefruit 32u4 3.3V
 * Common anode 7 segment single digit
 * 74HC595N 8-bit shift register
 * trim pot on an analog port
 * DS3231 realtime clock
 
 This uses a shift register so I can have enough pins to include an RTC.

 Here is the default bit sequence/pin mappings for lighting the segments:
 0 - DP via Q0
 1 - C  via Q1
 2 - B  via Q2
 3 - E  via Q3
 4 - F  via Q4
 5 - D  via Q5
 6 - G  via Q6
 7 - A  via Q7

 Loraxipam@github.com (c) 2020

 See LICENSE.txt for details on MIT Licensing of this source code, which is
 included by reference herein.

 */

// I'm not using the BLE but it could report the current pattern or update the RTC
// It's not often easy to get the serial input to change the clock on a Feather
//#include <Adafruit_BLE.h>
//#include <Adafruit_BluefruitLE_SPI.h>
//#include <Adafruit_BluefruitLE_UART.h>
//#include "BluefruitConfig.h"
#include <LowPower.h>
// for DS3231 RTC
#include <Wire.h>
#include "DS1307.h"

// Feather pins going to the 74HC595
#define CLOCK 12
#define LATCH 11
#define DATA  10

// This is the "LowDPR" LED-to-74HC595 mapping that I typically use
// lowDPR is the default layout but lowDPRows I think is the coolest...maybe
// Put a trim pot on an analog port and determine its low and hi analog values
#ifndef PPIN
#define PPIN 0
#define CPIN 1
#define BPIN 2
#define EPIN 3
#define FPIN 4
#define DPIN 5
#define GPIN 6
#define APIN 7
#define PATTERN 10
#define SELECTOR_ANALOG_PIN A1
#define SELECTOR_ANALOG_BITS 10
#define SELECTOR_ANALOG_LO 50
#define SELECTOR_ANALOG_HI 1010
#endif

// For convenience, here's straight mapping of LED pins to 74HC595
// These numbers skip the actual LED pins 3 and 8 which are the common lines
//#define EPIN 0
//#define DPIN 1
//#define CPIN 2
//#define PPIN 3
//#define BPIN 4
//#define APIN 5
//#define GPIN 6
//#define FPIN 7

// The "pattern" struct defines an individual segment illumination pattern. It's a name and a set of pins.
struct pattern {
  char *pattname;
  byte pinstep[8];
};

// The "patterns" struct holds all the known illumination patterns for the sketch
static struct pattern patterns[] = {
  {"lowDPL",   {PPIN, EPIN, FPIN, CPIN, BPIN, DPIN, GPIN, APIN}}, // DP is low bit, qtr hrs up left side. Hours right side & middle
  {"lowDPR",   {PPIN, CPIN, BPIN, EPIN, FPIN, DPIN, GPIN, APIN}}, // DP is low bit, qtr hrs up right side. Hours left, then middle
  {"midSecsL", {DPIN, GPIN, APIN, EPIN, FPIN, CPIN, BPIN, PPIN}}, // center bottom to top are min/qtr, then hrs up left, right, DP is high bit
  {"midSecsR", {DPIN, GPIN, APIN, CPIN, BPIN, EPIN, FPIN, PPIN}}, // center bottom to top are min/qtr, then hrs up right, left, DP is high bit
  {"lowDPRows",{PPIN, CPIN, EPIN, BPIN, FPIN, DPIN, GPIN, APIN}}, // DP is low bit, outside bottom vert qtrs. Hrs outside top vert then up middle
  {"topSecs",  {APIN, BPIN, FPIN, CPIN, DPIN, EPIN, GPIN, PPIN}}, // A is low bit, top verts are qtrs, bottom five are hours, DP is high bit
  {"lowSecs",  {DPIN, CPIN, EPIN, APIN, BPIN, FPIN, GPIN, PPIN}}, // D is low bit, bottom verts are qtrs, top five are hours, DP is high bit
  {"filler",   {APIN, GPIN, DPIN, EPIN, CPIN, FPIN, BPIN, PPIN}}, // pour it down the center and fill in from the bottom...
  {"bubble",   {DPIN, GPIN, APIN, FPIN, BPIN, EPIN, CPIN, PPIN}}, // bubble up from the bottom and flow down the sides...
  {"bubble2",  {PPIN, DPIN, GPIN, APIN, FPIN, BPIN, EPIN, CPIN}}, // bubble up from the bottom and flow down the sides...(but DP is low bit)
  {"biglyU",   {PPIN, APIN, GPIN, BPIN, CPIN, DPIN, EPIN, FPIN}}, // qrts top center, hours around from top, DP is low bit
  {"center",   {GPIN, DPIN, APIN, BPIN, CPIN, EPIN, FPIN, PPIN}}  // blinks from the center bar, qtrs bottom/top, and hrs go around. DP is high bit
};

// 7 Segment display type
// change this to false if you're using a different LED
const bool commonAnode = true;

// General debugging on serial
const   bool debugging = false;
unsigned int debugDelay = 640;

// Bluetooth
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Realtime clock
DS1307 rtc;
int Hor, Sec;

// The current bit sequence pattern
byte patternIndex;

// "mapPattern" moves the bits of the time byte to the bits of the display pattern byte
uint8_t mapPattern(uint8_t inByte) {
  uint8_t outByte = 0;
  for (uint8_t b=0; b<8; b++){
    if (bitRead(inByte,b)==0) {
      bitClear(outByte,patterns[patternIndex].pinstep[b]);
    } else {
      bitSet(outByte,patterns[patternIndex].pinstep[b]);
    }
  }
  return outByte;
}

// "chooseSelector" reads an analog pin and returns the relative index of the defined bit sequence patterns
byte chooseSelector(int analogPin) {
  int rowSize, structSize, entries, selectorVal, lowVal, hiVal, valWidth;
  byte index;

  // The bracketing for the analog reading is based on the number of pattern entries.
  structSize = (int)(sizeof(patterns));
  rowSize    = (int)(sizeof(patterns[0]));
  entries    = (int)(structSize / rowSize);

  // Read the 10-bit analog port to determine which pattern we'll use.
  selectorVal = analogRead(SELECTOR_ANALOG_PIN);

  // The pot that I use goes from about 60 to about 1000, so if you calibrate your pot
  // then you can and open them a bit and use those values here.
  // Use your actual pot results for the LO and HI values. Probably valid for all ADC resolutions.
  lowVal = SELECTOR_ANALOG_LO; hiVal = SELECTOR_ANALOG_HI;
  valWidth = (hiVal - lowVal) / entries;
  for (byte ent=0;ent<entries;ent++) {
    if (((ent*valWidth+lowVal) <= selectorVal) && (selectorVal <= ((ent+1)*valWidth+lowVal))) {
      index = ent;
      break;
    }
  }
  
  if (debugging) {
    Serial.print("S ");
    Serial.print(structSize);
    Serial.print(" R ");
    Serial.print(rowSize);
    Serial.print(" E ");
    Serial.println(entries);
    Serial.print(" sV ");
    Serial.print(selectorVal);
    Serial.print(" I ");
    Serial.println(index);
  }

  return index;
  
}

// "lightMe" sends eight bits to the shift register
void lightMe(uint8_t bits){
  // throw the latch
  digitalWrite(LATCH, LOW);
  // send the data
  shiftOut(DATA, CLOCK, MSBFIRST, (commonAnode ? ~bits : bits));
  // close the latch
  digitalWrite(LATCH, HIGH);
}

// The "showTimeShift" function receives the hour and the seconds of the hour
// then illuminates the correct segments
void showTimeShift(int hours, int seconds /* of the hour */) {
  
  int eighth;
  uint8_t curTime, mapTime;

  if (hours < 0 || hours > 23 || seconds < 0 || seconds > 3600 ) return;

  // calculate the holders
  eighth=seconds/450;   // 450s is 1/8 of an hour...7.5min.

  // stuff the hour bits on and move them over a bit (or three)
  curTime = hours << 3;    // in order to fit...
  curTime |= eighth;

  // if you want to save processing, (say you only want to use a single pattern)
  // then just comment out this mapping call and
  // then change the shiftOut below to use curTime instead of mapTime.
  // Make sure your pinouts from '595 to LED are okay, though.
  mapTime = mapPattern(curTime);
  
  if (debugging) {
    Serial.print("H ");
    Serial.print(hours);
    Serial.print(" S ");
    Serial.print(seconds);
    Serial.print(" E ");
    Serial.println(eighth);
    Serial.print(" curTime ");
    Serial.print(curTime,BIN);
    Serial.print(" ~curTime ");
    Serial.println((uint8_t)~curTime,BIN);
  }
  
  // light 'em up!
  lightMe(mapTime);
}


// "patternDemo" shows a fast pattern of the bit sequence
void patternDemo() {
  // show the low bits first
  for (int i=0; i<24; i++) {
    showTimeShift(0,(i%8)*450);
    delay(150);
  }

  // show the hours next
  for (int i=0; i<72; i++) {
    showTimeShift(i%24,0);
    delay(150);
  }
}



void setup() {

  String t;
  
  // set the shift register pins
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode( DATA, OUTPUT);

  // start the peripherals
  Wire.begin();
  rtc.begin();
  delay(128);
  rtc.getTime();

  // for debugging and first setup of the RTC
  if (debugging) {
    Serial.begin(115200);
    delay(2048);
    Serial.print(F(":"));Serial.print(F(":"));Serial.print(F(":"));
    Serial.println(F("SingleSevenSegBinaryClock v3, the binary sleeper version."));
    Serial.print(F("Current date: "));
    Serial.print(rtc.year,  DEC);
    Serial.print(F(":"));
    Serial.print(rtc.month < 10 ? F("0") : F(""));Serial.print(rtc.month, DEC);
    Serial.print(F(":"));
    Serial.print(rtc.dayOfMonth < 10 ? F("0") : F(""));Serial.print(rtc.dayOfMonth, DEC);
    Serial.print(F(":"));
    Serial.print(rtc.dayOfWeek);Serial.println(rtc.dayOfWeek, DEC);
    Serial.print(F("Current time: "));
    Serial.print(rtc.hour   < 10 ? F("0") : F(""));Serial.print(rtc.hour,   DEC);
    Serial.print(F(":"));
    Serial.print(rtc.minute < 10 ? F("0") : F(""));Serial.print(rtc.minute, DEC);
    Serial.print(F(":"));
    Serial.print(rtc.second < 10 ? F("0") : F(""));Serial.println(rtc.second, DEC);
    Serial.println(F("If datetime is wrong, enter new time as HH:MM:SS:20YY:mm:dd:dow."));
    delay(18000);
    Serial.println(F("...time's up. Off we go."));
    t = Serial.readString();

    //You'll need to fill in these if your rtc has lost power.
    if (t != "") {
      // add the 18ish seconds it takes to wait for the prompt
      if (t.substring(6,8).toInt() <= 41 ) {
        rtc.fillByHMS(t.substring(0,2).toInt(),
          t.substring(3,5).toInt(),
          t.substring(6,8).toInt()+19); // add 19 seconds to the current time, just to get it looking right
      } else {
        rtc.fillByHMS(t.substring(0,2).toInt(),
          t.substring(3,5).toInt()+1,
          t.substring(6,8).toInt()-41);
      }
      rtc.fillByYMD(t.substring(9,13).toInt(),
          t.substring(14,16).toInt()+1,
          t.substring(17,19).toInt()-41);
      rtc.fillDayOfWeek(t.substring(20,21).toInt());
      
      rtc.setTime();            //write time to the RTC chip
    }
    delay(100);

  }

  // A simple pot can choose the pattern for you
  // get the current pattern from the analog pin
  patternIndex = chooseSelector(SELECTOR_ANALOG_PIN);

  // get the time once more
  rtc.getTime();
  Hor = (int)(rtc.hour);
  Sec = (int)(rtc.minute*60+rtc.second);
  showTimeShift(Hor, Sec);
}

void loop() {

  // DEBUG
  if (debugging && debugDelay > 512 /*&& ((i%15UL)==0)*/) Serial.println(Sec, DEC);

  // Choose a different pattern
  byte selector = chooseSelector(SELECTOR_ANALOG_PIN);
  
  // If the new pattern is not the same as the old one,
  // flash the new one a few times before switchover
  if (selector != patternIndex) {
    for (int flash=0; flash<4; flash++) {
      lightMe(0);
      delay(100);
      lightMe((uint8_t)selector+1);
      delay(100);
    }

    // now remember the new pattern
    patternIndex = selector;
    // provide a short demo
    patternDemo();
    // show the current time
    showTimeShift(Hor, Sec);
  }

  // Read the current time
  rtc.getTime();
  Sec = (int)(rtc.minute*60+rtc.second);
  Hor = (int)(rtc.hour);

  // Show the current time
  if ((Sec%30)==0) {  // every 30 seconds is better than every 450 seconds
    showTimeShift(Hor, Sec);
  }

  // DEBUG
  delay(debugDelay);

  // For Feather32u4 MCU
  // Sleep for most of a second. This gives about 70ms to do real work before the clock strikes 0
  if ("sleep" == "sleep"){
    LowPower.idle(SLEEP_500MS, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);
    LowPower.idle(SLEEP_250MS, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);
    //LowPower.idle(SLEEP_120MS, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);
    //LowPower.idle(SLEEP_60MS, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);
  }

}