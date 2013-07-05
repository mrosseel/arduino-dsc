#define USE_LCD

#include <digitalWriteFast.h>

#include <PinChangeInt.h>

#ifdef USE_LCD
  // include the library code:
  #include <LiquidCrystal.h>
#endif

// mstimer screws with PinChangeInt
#include <MsTimer2.h>


/*
  Arduino based Digital Setting Circle
 
 Orlando Andico 2011-12-24
 
 <orly.andico at gmail.com>
 
 Changes in v1.0.1:
 - updated to compile on Arduino 1.0
 - full quadrature (4X) encodirg via 4 virtual interrupts and PinChangeInt library - http://www.arduino.cc/playground/Main/PinChangeInt
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

// RA
const int AZ_enc_A = 2;
const int AZ_enc_B = 3;

// DEC
const int ALT_enc_A = 12;
const int ALT_enc_B = 13;


// if either counts are updated..
volatile bool isUpdated = LOW;


// encoder resolution - some cheap encoders have lower tics on the DEC (like my JMI's!)
int altRES = 8192;    // resolution of encoders
int azRES = 8192;    // resolution of encoders

char beenAligned = 0;  

volatile bool _AzEncoderASet;
volatile bool _AzEncoderBSet;
volatile long AZ_pos = azRES / 2;

volatile bool _AltEncoderASet;
volatile bool _AltEncoderBSet;
volatile long ALT_pos = altRES / 2;


// timer stuff
// TODO should be an int? multibyte variables updated with an interrupt are dangerous
volatile unsigned long masterCount = 0;
volatile unsigned long oldCount = 0;
String commandLine = "";

void timerRoutine() {
  masterCount += 10;
}

#ifdef USE_LCD
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#endif

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
  #ifdef USE_LCD
  lcd.begin(16, 2);
  lcd.noCursor();
  #endif

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

  while (!Serial.available())
  {
    delay(10);
    
    // update every so often..
    if ((masterCount - oldCount) > 250) {
      #ifdef USE_LCD
      lcd.setCursor(0, 0);
      lcd.print(getEncoderValue(AZ_pos, HIGH));
      lcd.setCursor(8, 0);
      lcd.print(getEncoderValue(ALT_pos, HIGH));
      lcd.setCursor(0, 1);
      if(commandLine.length() < 16) {
        lcd.print(commandLine);
      } else {
        int length = commandLine.length();
        lcd.print(commandLine.substring(length-16));
      }
      #endif
      oldCount = masterCount;
    }
  }

  inchar = Serial.read();
  
  // build a history of commands sent to this sketch
  if(inchar != '\r' && inchar != '\n') {
    commandLine.concat(inchar);
  }

  if (inchar == 'Q')
  {
    printToSerial(getEncoderValue(AZ_pos, HIGH));
    printToSerial("\t");
    printToSerial(getEncoderValue(ALT_pos, HIGH));
    printToSerial("\r");
  }
  else if (inchar == 'R')
  {
    if (inchar == 'R')   {
      // first comes azimuth, then altitude (project pluto)
      String resolution1 = "";
      String resolution2 = ""; 
      if(Serial.available() > 0) {
        inchar = Serial.read();
      }
      while(Serial.available() > 0 && inchar != '\t') {
        resolution1.concat(inchar);
        inchar = Serial.read();
      }
      while(Serial.available() > 0 && inchar != '\r') {
        inchar = Serial.read();
        resolution2.concat(inchar);
      }      
      commandLine+="." + resolution1 + "-R-" + resolution2 + ".";
//      setAzRes(resolution1.toInt());
//      setAltRes(resolution2.toInt());
      printToSerial("R");
    }
  }
  // ignore command - just return proper code
  else if(inchar == 'Z' || inchar == 'I' || inchar == 'z') {
    if(inchar == 'I') {
      printToSerial("R");
    }
    else if (inchar == 'Z') {
      printToSerial("*"); 
    }
    else if (inchar == 'z') {
      printToSerial("r");
    }

  }
  else if (inchar == 'r') 
  {
    // print out resolution - in future this may be configurable
    printEncoderValue(azRES, LOW, LOW);
    printToSerial("\t");
    printEncoderValue(altRES, LOW, LOW);
    printToSerial("\r");

  }
  else if (inchar == 'V')
  {
    //version
    printToSerial("W1.0.2\r");
  }
  else if (inchar == 'T')
  {
    // test mode - output resolutions and error count
    printEncoderValue(azRES, LOW, LOW);
    printToSerial(",");
    printEncoderValue(altRES, LOW, LOW);
    printToSerial(",00000\r");
  }
  else if (inchar == 'q')
  {
    // error count
    printToSerial("00000\r");
  }
  else if (inchar == 'P')
  {
    // encoder power up
    printToSerial("P");
  }
  else if (inchar == 'p')
  {
    // 
    // dave eks error command
    printToSerial("00");
  } 
  else if (inchar == 'h' || inchar == 'H')
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
      printToSerial("Y");
    else
      printToSerial("N");
  }
  else if (inchar == 'A')
  {
    beenAligned = 1;
  } 
  else {
    addOutputToCommandLine("?"); 
  }

  Serial.flush();
}

void addOutputToCommandLine(char* output) {
  commandLine +="." + *output;  
}

String getEncoderValue(long val, bool outputLeadingSign) {
  
  unsigned long aval = abs(val);
  String result = "";

  if (outputLeadingSign) {
    if (val < 0)
      result+="-";
    else
      result+="+";       
  }

  if (aval < 10) 
    result+="0000";
  else if (aval < 100)
    result+="000";
  else if (aval < 1000)
    result+="00";
  else if (aval < 10000) 
    result+="0";

  result+=aval;
  return result;  
}

// print encoder value with leading zeros
void printEncoderValue(long val, bool outputLeadingSign, bool toLCD)
{
  printToSerial(getEncoderValue(val,outputLeadingSign));  
}

void printToSerial(String toPrint) {
  #ifdef USE_LCD
    commandLine += toPrint;
  #endif
  char str[toPrint.length()];
  toPrint.toCharArray(str, toPrint.length());
  Serial.print(str);
}


void printToSerial(char* toPrint) {
  #ifdef USE_LCD
    commandLine += *toPrint;
  #endif
  Serial.print(*toPrint);
}

void printToSerial(long toPrint) {
  #ifdef USE_LCD
    commandLine += toPrint;
  #endif
  Serial.print(toPrint);
}


void printHexEncoderValue(long val)
{
  char low, high;

  high = val / 256;

  low = val - high*256;

  Serial.print(low, HEX);
  Serial.print(high, HEX);

//  printToSerial(low, HEX);
//  printToSerial(high, HEX);

  return;
}

void setAltRes(int newAltRes) {
  altRES = newAltRes;    // resolution of encoders
  ALT_pos = altRES / 2; 
}

void setAzRes(int newAzRes) {
  azRES = newAzRes;    // resolution of encoders
  AZ_pos = azRES / 2; 
}

















