/*
 * DataLogger.ino
   
   Go Kart data logger
   Purdue University EPICS program
   
   Created 08/23/2016
   Rev 4.8 12/6/2016  sj
   
   Created by: StephJohnson
               john1254@purdue.edu
*/
////////////////////////////////////
/*
  Detailed pin connections

    Digital Pin 0: NC
    Digital Pin 1: Display Communication
    Digital Pin 2: Hall Effect Sensor
    Digital Pin 3: Additional Digital Input
    Digital Pin 4: NC
    Digital Pin 5: NC
    Digital Pin 6: NC
    Digital Pin 7: NC
    Digital Pin 8: NC
    Digital Pin 9: NC
    Digital Pin 10: SPI CS pin for Data Logger (through stack)
    Digital Pin 11: SPI MOSI pin for Data Logger (through stack)
    Digital Pin 12: SPI MISO pin for Data Logger (through stack)
    Digital Pin 13: SPI CLK pin for Data Logger (through stack)

    Analog Pin 0: Current Sensor Input
    Analog Pin 1: Kart Voltage Divider Input
    Analog Pin 2: Additional Analog Input 1
    Analog Pin 3: Additional Analog Input 2
    Analog Pin 4: I2C SDA pin for Accelerometer and RTC (through stack for RTC)
    Analog Pin 5: I2C SCL pin for Accelerometer and RTC (through stack for RTC)

    5V Pin:  Current Sensor PWR/GND
             Hall Effect Sensor PWR/GND
             Accelerometer PWR/GND
             Display PWR/GND
             
    GND Pin: 9V battery (negative terminal)
             Kart Voltage Divider twisted pair
             Current Sensor PWR/GND
             Current Sensor twisted pair
             Hall Effect Sensor PWR/GND
             Hall Effect Sensor twisted pair
             Accelerometer PWR/GND
             Display PWR/GND
             Display twisted pair
             Additional Analog Input 1 twisted pair
             Additional Analog Input 2 twisted pair
             Additional Digital Input
             
    Vin Pin: 9V battery (positive terminal)
    
*/
////////////////////////////////////
//
//Preprocessor Commands
//

//Includes
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

//Peripheral Defines
RTC_DS1307 rtc;
Adafruit_ADXL345_Unified accel;

//Defined Constants
#define LOG_INTERVAL 200 //record every 200 millisecond

//Global Variables
const int chipSelect = 10;
float totalVoltage;
const byte interruptPin = 2;
const byte sdPin = 10;
float scaledInput1;
float scaledInput2;
float kartCurrent;
float kartPower;
float kartEnergy = 0;
float rev = 0;
float rpm;
int ai1;
int ai2;
int dIO;

File logfile;                 //Compact Flash File

/////////////////////////////////////
//
// Setup
//
void setup() {
  //
  //Serial communications
  //
  Serial.begin(9600);
  
  //
  // SD Card
  //
  pinMode(sdPin, OUTPUT); // make sure that the default chip select pin is set to output, even if you don't use it:
  // see if card is present and initialized
  if (!SD.begin(chipSelect)) {
    error("SD Card Fail");
  }
  
  // Setting Accelerometer ID and initializing it 
  accel = Adafruit_ADXL345_Unified(12345);
  
  // create a new file/open existing file
  char filename[] = "PhysBox.CSV";
  logfile = SD.open(filename, FILE_WRITE);
  
  //File can't be made/opened
  if (! logfile) {
    error("CSV Failure");
  }

  //
  // LCD
  //
  displayOn();
  clearLCD();
  cursorHome();
  Serial.write("Logging to: ");
  newLine();
  Serial.write(filename);
  delay(3000);  

  //
  // Hall Effect Sensor Initilization
  //
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin),angVel,FALLING);

  //
  // Additional Digital Input Initilization
  //
  pinMode(3, INPUT_PULLUP);

  //
  // Analog Input Check
  //
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  delay(5);
  ai1 = (analogRead(A2)>1020);
  ai2 = (analogRead(A3)>1020);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  delay(5);

  //
  //Accelerometer Initilization
  //
  if(!accel.begin())
  {
    error("Accel Failure");
    while(1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  //sensors_event_t accelEvent;
  
  //
  //RTC
  //
  if (!rtc.begin()) {
    logfile.println("RTC failed");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  DateTime now;


  //
  //Logfile Headings
  //
  logfile.println("");
  logfile.println("Day/Month/Year, Hour:Minute:Second");
  now = rtc.now();                          //Get Current Time
  logfile.print(now.day(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.year(), DEC);
  logfile.print(",");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.println(now.second(), DEC);
  logfile.print("Time");
  logfile.print(",");
  logfile.print("Volt");
  logfile.print(",");
  logfile.print("Amp");
  logfile.print(",");
  logfile.print("Power (KWatt)");
  logfile.print(",");
  logfile.print("Energy (KWatt Hr)");
  logfile.print(",");
  logfile.print("X-Accel");
  logfile.print(",");
  logfile.print("Y-Accel");
  logfile.print(",");
  logfile.print("Z-Accel");
  logfile.print(",");
  logfile.print("RPM");
  logfile.print(",");
  logfile.print("AIO1");
  logfile.print(",");
  logfile.print("AIO2");
  logfile.print(",");
  logfile.println("DIO");
  
  clearLCD();
}

/////////////////////////////////
//
// Loop
//
// Date Logger
// Prints current system values to data file
//
void loop() { 
  //
  // Log at LOG_INTERVAL intervals
  //  
  if((millis() % LOG_INTERVAL) == 0) {
    
    // Read real time
    DateTime now = rtc.now();

    // Read Acceleration
    sensors_event_t accelEvent;
    accel.getEvent(&accelEvent);      // Library Call
    
    totalVoltage = voltageDivider();
    scaledInput1 = additionalAIO1();
    scaledInput2 = additionalAIO2();
    kartCurrent = currentSense();
    kartPower = (totalVoltage * kartCurrent/1000);
    kartEnergy = kartEnergy + (kartPower * LOG_INTERVAL/(60*60));
    dIO = digitalInput();

    // Read Rev from Interupt routine and calculate RPM
    rpm = (rev*1000.0*60.0)/LOG_INTERVAL;
    rev = 0;
    
    // Print time
    logfile.print(millis());
    logfile.print(",");

    // Print values
    logfile.print(totalVoltage);
    logfile.print(",");

    logfile.print(kartCurrent);
    logfile.print(",");

    logfile.print(kartPower);
    logfile.print(",");

    logfile.print(kartEnergy);
    logfile.print(",");

    logfile.print(accelEvent.acceleration.x);
    logfile.print(",");
    logfile.print(accelEvent.acceleration.y);
    logfile.print(",");
    logfile.print(accelEvent.acceleration.z);
    logfile.print(",");
    logfile.print(rpm);
    logfile.print(",");
    
    if(ai1 == 0){
      logfile.print(scaledInput1);
    }else{
      logfile.print("0");
    }
    logfile.print(",");
    if(ai2 == 0){
      logfile.print(scaledInput2);
    }else{
      logfile.print("0");
    }
    logfile.print(",");
    logfile.print(dIO);
  
    //Display Communication
    cursorHome();
    Serial.write("RPM: ");
    Serial.print(rpm);
    Serial.write("    ");
    newLine();
    Serial.write("KWhr: ");
    Serial.print(kartEnergy);

    //Save data to csv file
    logfile.println("");
    logfile.flush();
  }
} //End Loop


void error(char const *str)
{
  clearLCD();
  cursorHome();
  Serial.write("ERR:");
  Serial.write(str);
  while(1);
}

//Scales input voltage to 20x value to represent Kart voltage
float voltageDivider(){
  float tmp;
  tmp = analogRead(A1);
  tmp = tmp * (5.0 / 1023.0);
  tmp = tmp * 20;
  if(tmp > 100){
    error("Over Voltage");
  }
  return(tmp);
}

// Measures Optional Analog Input 1
float additionalAIO1(){
  float tmp;
  tmp = analogRead(A2);
  tmp = tmp*(5.0/1023.0);
  return(tmp);
}

// Measures Optional Analog Input 2
float additionalAIO2(){
  float tmp;
  tmp = analogRead(A3);
  tmp = tmp*(5.0/1023.0);
  return(tmp);
}

// Read Voltage from Current Sensor
// Referenced to 2.5 V
float currentSense(){
  float tmp;
  tmp = analogRead(A0);
  tmp = tmp * (5.0 / 1023.0);
  tmp = tmp - 2.5;
  tmp = tmp * 120.0;
  return(tmp);
}

// Interrupt Routine for Hall Effect Sensor
void angVel(){
  rev++;
}

//Read and Invert Digital Input to check for 0
int digitalInput(){
  dIO = digitalRead(3);
  dIO = !dIO;
  return(dIO);
}


//  Functions Related to New Haven Display LCD using RS-232 Serial Port
//  More Details at http://www.newhavendisplay.com/specs/NHD-0216K3Z-FL-GBW-V3.pdf

// clear the LCD
void clearLCD() {
  Serial.write(254); // Prefix: 0xFE => 254
  Serial.write(81); // 0x51 => 81
}

//Turn display on
void displayOn() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(65);  // 0x41
}

// start a new line
void newLine() {
  cursorSet(1,0); // 1st Row and Column 0.
}

// move the cursor to the home position
void cursorHome() {
  Serial.write(254);
  Serial.write(70); // Set the Cursor to (0,0) position on the LCD
}

void blinkingCursorOn(){
   Serial.write(254); // 0xFE   
   Serial.write(75); // 0x4B
}

// move the cursor to a specific place
// Here is xpos is Column No. and ypos is Row No.
void cursorSet(int ypos, int xpos) {
  Serial.write(254);
  Serial.write(69);
  
  // Bounding the X-Position
  if(xpos >= 15) {
    xpos = 15;
  }
  else if(xpos <= 0) {
   xpos = 0; 
  }
  
  // Bounding the Y-Position
  if(ypos >= 1) {
    ypos = 1;
  }
  else if(ypos <= 0) {
   ypos = 0; 
  }
  
  int finalPosition = ypos * 64 + xpos;
  
  Serial.write(finalPosition);
  //Serial.write(ypos); //Row position 
}
