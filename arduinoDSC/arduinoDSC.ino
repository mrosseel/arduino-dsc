/*
  Arduino based Digital Setting Circle
 
 Orlando Andico 2011-12-24
 
 <orly.andico at gmail.com>
 
 Changes in v1.0.1:
 - updated to compile on Arduino 1.0
 - full quadrature (4X) encoding via 4 virtual interrupts and PinChangeInt library - http://www.arduino.cc/playground/Main/PinChangeInt
 - more efficient reading of ports via digitalWriteFast library - http://code.google.com/p/digitalwritefast/
 - fast quadrature decode based on code from Dr Rainer Hessmer - http://www.hessmer.org/blog/2011/01/30/quadrature-encoder-too-fast-for-arduino/
 - only tested on Uno R3
 - note: all the above libraries needed minor edits to compile properly on Arduino 1.0
 
 
 Michael Fulbright 2010-06-06
 
 <mike.fulbright at pobox.com>
 
 Feel free to modify and share - please let me know if you make any changes so I can keep my version up to date
 
 2010-06-06:  Initial relase - supports 2X encoding
 
 */

// constants won't change. They're used here to 
// set pin numbers:

#include <PinChangeInt.h>
#include <PinChangeIntConfig.h>

#include <digitalWriteFast.h>

// include the library code:
#include <LiquidCrystal.h>


// mstimer screws with PinChangeInt
#include <MsTimer2.h>


// RA
const int AZ_enc_A = 2;
const int AZ_enc_B = 3;

// DEC
const int ALT_enc_A = 12;
const int ALT_enc_B = 13;


// if either counts are updated..
volatile bool isUpdated = LOW;


// encoder resolution - some cheap encoders have lower tics on the DEC (like my JMI's!)
const int altRES = 10000;    // resolution of encoders
const int azRES = 10000;    // resolution of encoders

char beenAligned = 0;  

volatile bool _AzEncoderASet;
volatile bool _AzEncoderBSet;
volatile long AZ_pos = azRES / 2;

volatile bool _AltEncoderASet;
volatile bool _AltEncoderBSet;
volatile long ALT_pos = altRES / 2;


// timer stuff
volatile unsigned long masterCount = 0;
volatile unsigned long oldCount = 0;

void timerRoutine() {
  masterCount += 10;
}


// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup() {

  // initialize the encoder inputs
  pinMode(ALT_enc_A, INPUT);
  pinMode(ALT_enc_B, INPUT);
  pinMode(AZ_enc_A, INPUT);
  pinMode(AZ_enc_B, INPUT);

  // turn on pullup resistors.. the quadrature algorithm is extremely sensitive
  digitalWrite(ALT_enc_A, LOW);
  digitalWrite(ALT_enc_B, LOW);
  digitalWrite(AZ_enc_A, LOW);
  digitalWrite(AZ_enc_B, LOW);

  // hook interrupts off the ALT and AZ pins
  PCintPort::attachInterrupt(ALT_enc_A, altFuncA, CHANGE);
  PCintPort::attachInterrupt(ALT_enc_B, altFuncB, CHANGE);

  PCintPort::attachInterrupt(AZ_enc_A, azFuncA, CHANGE);
  PCintPort::attachInterrupt(AZ_enc_B, azFuncB, CHANGE);

  Serial.begin(9600);

  // backlight control
  pinMode(10, OUTPUT);
  analogWrite(10, 10);  // default LCD backlight brightness
  lcd.begin(16, 2);

  // 10ms period
  MsTimer2::set(10, timerRoutine);
  MsTimer2::start();
}

// ################### ALTITUDE DECODING FUNCTION ###################

void altFuncA() {
  _AltEncoderASet = digitalReadFast(ALT_enc_A) == HIGH;   // read the input pin
  _AltEncoderBSet = digitalReadFast(ALT_enc_B);   // read the input pin

  ALT_pos += (_AltEncoderASet != _AltEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void altFuncB() {
  _AltEncoderASet = digitalReadFast(ALT_enc_A);   // read the input pin
  _AltEncoderBSet = digitalReadFast(ALT_enc_B) == HIGH;   // read the input pin

  ALT_pos += (_AltEncoderASet == _AltEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


// ################### AZIMUTH DECODING FUNCTION ###################

void azFuncA() {
  _AzEncoderASet = digitalReadFast(AZ_enc_A) == HIGH;   // read the input pin
  _AzEncoderBSet = digitalReadFast(AZ_enc_B);   // read the input pin

  AZ_pos += (_AzEncoderASet != _AzEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void azFuncB() {
  _AzEncoderASet = digitalReadFast(AZ_enc_A);   // read the input pin
  _AzEncoderBSet = digitalReadFast(AZ_enc_B) == HIGH;   // read the input pin

  AZ_pos += (_AzEncoderASet == _AzEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void loop() { 

  char inchar;

  // update every so often..
  if ((masterCount - oldCount) > 250) {
    lcd.setCursor(0, 0);
    printEncoderValue(AZ_pos, HIGH, HIGH);
    lcd.setCursor(8, 0);
    printEncoderValue(ALT_pos, HIGH, HIGH);

    lcd.setCursor(0, 1);
    lcd.print(masterCount);
    oldCount = masterCount;
  }

  if (!Serial.available())
  {
    delay(10);
  }
  inchar = Serial.read();

  // throw away rest of command - we don't need it
  Serial.flush();

  if (inchar == 'Q')
  {
    printEncoderValue(AZ_pos, HIGH, LOW);
    Serial.print("\t");
    printEncoderValue(ALT_pos, HIGH, LOW);
    Serial.print("\r");
  }
  else if (inchar == 'R' || inchar == 'Z' || inchar == 'I' || inchar == 'z')
  {
    // ignore command - just return proper code
    if (inchar == 'R' || inchar == 'I')  
      Serial.print("R");
    else if (inchar == 'Z')
      Serial.print("*"); 
    else if (inchar == 'z')
      Serial.print("r");
  }
  else if (inchar == 'r') 
  {
    // print out resolution - in future this may be configurable
    printEncoderValue(azRES, LOW, LOW);
    Serial.print("\t");
    printEncoderValue(altRES, LOW, LOW);
    Serial.print("\r");

  }
  else if (inchar == 'V')
  {
    //version
    Serial.print("V 1.0.2\r");
  }
  else if (inchar == 'T')
  {
    // test mode - output resolutions and error count
    printEncoderValue(azRES, LOW, LOW);
    Serial.print(",");
    printEncoderValue(altRES, LOW, LOW);
    Serial.print(",00000\r");
  }
  else if (inchar == 'q')
  {
    // error count
    Serial.print("00000\r");
  }
  else if (inchar == 'P')
  {
    // encoder power up
    Serial.print("P");
  }
  else if (inchar == 'p')
  {
    // 
    // dave eks error command
    Serial.print("00");
  } 
  else if (inchar == 'h')
  {
    // report resolution in Dave Eks format
    Serial.write(0xA0);
    Serial.write(0x0F);
    Serial.write(0xA0);
    Serial.write(0x0F);
  }
  else if (inchar == 'y')
  {
    // report encoders in Dave Eks format
    printHexEncoderValue(ALT_pos);
    printHexEncoderValue(AZ_pos);
  }  
  else if (inchar == 'a')
  {
    if (beenAligned)
      Serial.print("Y");
    else
      Serial.print("N");
  }
  else if (inchar == 'A')
  {
    beenAligned = 1;
  }
}


// print encoder value with leading zeros
void printEncoderValue(long val, bool lead, bool toLCD)
{
  unsigned long aval; 

  if (lead) {
    if (!toLCD) {
      if (val < 0)
        Serial.print("-");
      else
        Serial.print("+");
    } 
    else {
      if (val < 0)
        lcd.print("-");
      else
        lcd.print("+");
    }      
  }

  aval = abs(val);

  if (!toLCD) {
    if (aval < 10) 
      Serial.print("0000");
    else if (aval < 100)
      Serial.print("000");
    else if (aval < 1000)
      Serial.print("00");
    else if (aval < 10000) 
      Serial.print("0");

    Serial.print(aval);  
  } 
  else {
    if (aval < 10) 
      lcd.print("0000");
    else if (aval < 100)
      lcd.print("000");
    else if (aval < 1000)
      lcd.print("00");
    else if (aval < 10000) 
      lcd.print("0");

    lcd.print(aval);  
  }    
}

void printHexEncoderValue(long val)
{
  char low, high;

  high = val / 256;

  low = val - high*256;

  Serial.print(low, HEX);
  Serial.print(high, HEX);

  return;
}


















