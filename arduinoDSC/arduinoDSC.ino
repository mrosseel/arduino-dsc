#define USE_LCD

//#include <digitalWriteFast.h>

//#include <PinChangeInterrupt.h>

#ifdef USE_LCD
  // only used when testing with the LCD screen
  #include <Wire.h>
  #include "SSD1306.h"
#endif

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

// mstimer screws with PinChangeInt
//#include <MsTimer2.h>

/*
  Arduino based Digital Setting Circle
 
 Mike Rosseel 2013-07-22
 
 - make more compatible with http://eksfiles.net/digital-setting-circles/circuit-description/ protocol description, implemented 'h' and 'y'. 'p' always returns 0, 'z' is not implemented
 
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
unsigned long startTime = millis();
String commandLine = "";
String debugCommandLine = "";

//void timerRoutine() {
//  masterCount += 10;
//}

#ifdef USE_LCD
  // initialize the library with the numbers of the interface pins
  SSD1306 display(0x3c, 5, 4); 
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
  attachInterrupt(ALT_enc_A, altFuncA, CHANGE);
  attachInterrupt(ALT_enc_B, altFuncB, CHANGE);

  attachInterrupt(AZ_enc_A, azFuncA, CHANGE);
  attachInterrupt(AZ_enc_B, azFuncB, CHANGE);

  //Serial.begin(9600);
  Serial.begin(115200);
  if(!SerialBT.begin("arduinoDSC")){ //Bluetooth device name
    Serial.println("An error occurred initializing Bluetooth");
  }else{
    Serial.println("The device started, now you can pair it with bluetooth!");
  }

#ifdef USE_LCD
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0,"Starting ...");
  display.display();
#endif
  // 10ms period
  //MsTimer2::set(10, timerRoutine);
  //MsTimer2::start();
}

// ################### ALTITUDE DECODING FUNCTION ###################

void altFuncA() {
  _AltEncoderASet = digitalRead(ALT_enc_A) == HIGH;   // read the input pin
  _AltEncoderBSet = digitalRead(ALT_enc_B);   // read the input pin

  ALT_pos += (_AltEncoderASet != _AltEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void altFuncB() {
  _AltEncoderASet = digitalRead(ALT_enc_A);   // read the input pin
  _AltEncoderBSet = digitalRead(ALT_enc_B) == HIGH;   // read the input pin

  ALT_pos += (_AltEncoderASet == _AltEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


// ################### AZIMUTH DECODING FUNCTION ###################

void azFuncA() {
  _AzEncoderASet = digitalRead(AZ_enc_A) == HIGH;   // read the input pin
  _AzEncoderBSet = digitalRead(AZ_enc_B);   // read the input pin

  AZ_pos += (_AzEncoderASet != _AzEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void azFuncB() {
  _AzEncoderASet = digitalRead(AZ_enc_A);   // read the input pin
  _AzEncoderBSet = digitalRead(AZ_enc_B) == HIGH;   // read the input pin

  AZ_pos += (_AzEncoderASet == _AzEncoderBSet) ? +1 : -1;
  isUpdated = HIGH;
} 


void loop() { 
  char inchar;
  int positionCounter = 0;

  while (!SerialBT.available())
  {
    delay(10);
    
    #ifdef USE_LCD
    // update every so often..
    if ((millis() - startTime) >= 250) {
      startTime = millis();
      display.clear();
      String value = getEncoderValue(AZ_pos, HIGH);
      value[1] = ' ';
      display.drawString(0, 0, value);
      value = getEncoderValue(ALT_pos, HIGH);
      value[1] = ' ';
      display.drawString(0, 24, value);
      //debugCommandLine.concat(commandLine.length());
      if(commandLine.length() > 16) {
        commandLine = commandLine.substring(1);        
      }
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 48, commandLine.substring(0, 16));
      display.setFont(ArialMT_Plain_24);

      display.display();
    }
    #endif
  }

  inchar = SerialBT.read();
  Serial.print(inchar);
  
  #ifdef USE_LCD
  // build a history of commands sent to this sketch
  if(inchar != '\r' && inchar != '\n') {
    commandLine.concat(inchar);
  }
  /*
  display.drawString(0, 0, getEncoderValue(AZ_pos, HIGH));
  display.drawString(0, 10, getEncoderValue(ALT_pos, HIGH));
  //debugCommandLine.concat(commandLine.length());
  if(commandLine.length() > 16) {
    commandLine = commandLine.substring(1);        
  }  
  display.drawString(0, 26, commandLine.substring(0, 16));
  display.display();
*/
  #endif

  if (inchar == 'Q')
  {
    printToSerial(getEncoderValue(AZ_pos, HIGH));
    printToSerial("\t");
    printToSerial(getEncoderValue(ALT_pos, HIGH));
    printToSerial("\r");
  }
  else if (inchar == 'R')
  {
      // first comes azimuth, then altitude (project pluto)
      String resolution1 = String("");
      String resolution2 = String(""); 
  
      while(SerialBT.available() > 0) {
        inchar = SerialBT.read();
        SerialBT.print(inchar);
        if(inchar == '\t') { break; }
        resolution1.concat(inchar);        
      }
     
      while(Serial.available() > 0) {
        inchar = SerialBT.read();
        SerialBT.print(inchar);  
        if(inchar == '\r') { break; }
          resolution2.concat(inchar);    
      }      
      commandLine+="." + resolution1 + "-R-" + resolution2 + ".";
//      setAzRes(resolution1.toInt());
//      setAltRes(resolution2.toInt());
      printToSerial("R");
//      Serial.print('\t');
//       Serial.print(resolution1);
//       Serial.print('\t');
//        Serial.print(resolution2);
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
    // print out resolution
    printEncoderValue(azRES, LOW);
    printToSerial("\t");
    printEncoderValue(altRES, LOW);
    printToSerial("\r");

  }
  else if (inchar == 'V')
  {
    //version
    printToSerial("V 1.0.3\r");
  }
  else if (inchar == 'T')
  {
    // test mode - output resolutions and error count
    printEncoderValue(azRES, LOW);
    printToSerial(",");
    printEncoderValue(altRES, LOW);
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
    printHexEncoderValue(altRES);
    printHexEncoderValue(azRES);
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
    addOutputToCommandLine('?'); 
  }

  //Serial.flush();
}

void addOutputToCommandLine(char output) {
  commandLine +="." + output;  
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
void printEncoderValue(long val, bool outputLeadingSign)
{
  printToSerial(getEncoderValue(val,outputLeadingSign));  
}

void printToSerial(String toPrint) {
  #ifdef USE_LCD
    commandLine += toPrint;
  #endif

  Serial.print(toPrint);
  SerialBT.print(toPrint);
}


void printHexEncoderValue(long val)
{
  char low, high;

  high = val / 256;

  low = val - high*256;

  if (low<0x10) {SerialBT.print("0");} 
  SerialBT.print(low, HEX);
  if (high<0x10) {SerialBT.print("0");} 
  SerialBT.print(high, HEX);

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

















