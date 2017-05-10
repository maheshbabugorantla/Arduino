#include <EEPROM.h>
#include <SD.h>
#include<Wire.h>
#include "RTClib.h"

/* Peripheral Devices */
// This is used to fetch the RealTime Clock
RTC_PCF8523 rtc;

// Assigining a Unique ID to the Accelerometer Device
//Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define LOG_INTERVAL 2000 // millis between entries

#define CurrIn A0 // Potentiometer 1 (To Simulate the Voltage input)
#define VoltIn A1 // Potentiometer 2 (To Simulate the Current input)

// Digital pins that connect to LEDs
const int redLEDPin = 8; // Warning LED
const int BUTTON = 3; // Switch Off Button

// This Pin is used to clear the EEPROM Memory
// This is used to setup an interrupt to clear the EEPROM Memory when a button connected to Digital Pin 2 is pressed
const int ClearMemory = 2;

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// The Logging File
File logFile;
char filename[] = "LOGGER54.csv";

// Analog Sensors
// Used for Debouncing Push Button
boolean currentButton = LOW;
boolean lastButton = HIGH;

int counter = 0;

float VoltVals[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Used to compute the Moving Window Average of Volts
float CurrentVals[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Used to compute the running average of Amperes

float sumCurrent = 0;
float sumVoltage = 0;

float avgVolt = 0;
float avgCurrent = 0;
float avgPower = 0;

float Energy; // = 0.000f;
float Energy1; // = 0.000f;
float newEnergy = 0;

boolean resetFlag = false;

void setup() {
  // put your setup code here, to run once:

  // This is default RS 232 Baud Rate as well (Used for LCD Display).
  // Changing this Value from 9600 to some other value will make LCD Work Abnormally.
  Serial.begin(9600);

  unsigned long millisVal = millis();

  pinMode(VoltIn, INPUT);   // Analog Pin 0
  pinMode(CurrIn, INPUT); // Analog Pin 1
  pinMode(BUTTON, INPUT); // Configuring the Button Pin to the INPUT Mode
  pinMode(redLEDPin, OUTPUT); // Configuring the Warning LED Pin to the OUTPUT Mode
  pinMode(ClearMemory, INPUT); // Configuring the Clear the EEPROM Button

  // Resetting the Warning LED
  digitalWrite(redLEDPin, LOW);
  digitalWrite(BUTTON, HIGH); // Switching on the Internal Pull-Up Resistors
  digitalWrite(ClearMemory, HIGH); // Switching on the Internal Pull-Up Resistors

  // Setting the EEPROM Clear Button
  attachInterrupt(digitalPinToInterrupt(ClearMemory), reset_eeprom, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON), reset_power_led, FALLING);
  
  // Beginning the RTC. If this step is not performed, then RTC will read out very abnormal Values.
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // The following line sets the RTC to date & time this sketch was compiled.
  if (!rtc.initialized()) {
    Serial.println("Adjusting RTC");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize the SD Card
  Serial.print("Initializing SD card... ");
  clearLCD();
  delay(500);

  // Make sure that default chip select pin is set to output
  // even if you don't use it:
  pinMode(chipSelect, OUTPUT);

  // Checking if the SD Card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }

  Serial.println("Card Initialized");
  clearLCD();
  delay(500);

  //Serial.print("Logging to: ");
  Serial.print(filename);
  delay(1000);
  clearLCD();

  logFile = SD.open(filename, FILE_WRITE);
  // Don't forget to use RTC here.
  // Insert the Time Stamp for new logging

  // This below piece of code inserts the Time Stamp for new Logging every time when Power is RESET.
  DateTime now = rtc.now();
  logFile.println("");
  logFile.print(now.month(), DEC);
  logFile.print("/");
  logFile.print(now.day(), DEC);
  logFile.print("/");
  logFile.print(now.year(), DEC);
  logFile.print(" : ");
  logFile.print("(");
  logFile.print(daysOfTheWeek[now.dayOfTheWeek()]);
  logFile.print(")");
  logFile.print(now.hour(), DEC);
  logFile.print(':');
  logFile.print(now.minute(), DEC);
  logFile.print(':');
  logFile.print(now.second(), DEC);
  logFile.println();

  // Printing to the Serial
/*  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println(); */

  logFile.println("millis,Volt,Current,Power(kW),Energy(kJ)");
  logFile.close();
//  Serial.println("millis,Volt,Current,Power(kW),Energy(kJ)");

  clearLCD(); // Clear the Contents of LCD.
  displayOn(); // LCD Display On
  cursorHome(); // Set the LCD
  
  EEPROM.get(0, Energy);
  resetFlag = true;

/*  Serial.print("Setup Time: ");
  Serial.print(millis() - millisVal);
  Serial.println(""); */
  
}

void loop() {

  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();

  if (resetFlag) {
    //Serial.print("New Energy: ");
    //Serial.print(Energy);
    //Serial.println("");
    resetFlag = false;
  }
  
  unsigned long millisVal = millis();

  // This function is used to read the input from the External RESET Button
  // External Reset Logic for the Power Alarm
//  if(digitalRead(BUTTON) == 0) {
//    digitalWrite(redLEDPin, LOW); 
//  }

  // Logging the Moving Window average of the Voltage and Current for every 200 ms.
  if (millisVal % 200 == 0) {

    counter = (counter + 1) % 20;

    // Need to calculate the Running Average
    VoltVals[counter] = readVoltage();
    CurrentVals[counter] = -1*readCurrent(); // Current Reversal is being done due to current sensor configuration on the physics box

    // Calculating the Running Average of Voltage and Current every 200 ms
    avgVolt = MWAvg(counter, &sumVoltage, VoltVals);
    avgCurrent = MWAvg(counter, &sumCurrent, CurrentVals);
    float Power = avgVolt * avgCurrent;

    newEnergy += Power * 0.2; // This always the newly computed Energy

    // Saving the Data to a LogFile.
    saveLogData(avgVolt, avgCurrent, Power, millisVal, Energy + newEnergy / 1000);
  }

  // Storing the Energy into the EEPROM every 10 Second
  if (millisVal % 10000 == 0) {
    // Fetching the stored EEPROM value when the board is reset or is powered.
    EEPROM.get(0, Energy);
    Energy += newEnergy / 1000; // Here we are computing cumulative energy value in kJ
    //Serial.print("Energy Stored: ");
    //Serial.print(Energy);
    //Serial.println("");
    Energy1 += newEnergy / 1000;
    newEnergy = 0;

    // Writing the Value of Energy to the EEPROM Memory so that the most recent Energy Value is available even when the power is switched off.
    EEPROM.put(0, Energy);
    EEPROM.put(100, Energy); // Just Making a Duplicate Copy
  }
}

void reset_eeprom() {
   Serial.println("Inside EEPROM");
  
  // Clearing the value in the Counter
  Energy = 0; // Clearing the Cumulative Energy Stored in the EEPROM.
  EEPROM.put(0, Energy);
  EEPROM.put(100, Energy);
}

void reset_power_led() {
  Serial.println("Reset Power");

  // Switching off the power LED
  digitalWrite(redLEDPin, LOW);
}

// Debouncing PushButton
/*boolean debounce(boolean lastButton) {
  
  currentButton = digitalRead(BUTTON);

  if (lastButton != currentButton)
  {
    delay(10); // Delaying for 10ms will allow the button state to settle down
    currentButton = digitalRead(BUTTON);
  }

  return (currentButton);
}

// This is used to reset the RedLight (That indicates the Energy Outage)
void externalReset(unsigned long millisVal)
{    
  currentButton = debounce(lastButton);

 if(millisVal % 200 == 0) {
  Serial.print("Current Button: ");
  Serial.print(currentButton);
  Serial.println("");

  Serial.print("Last Button: ");
  Serial.print(lastButton);
  Serial.println("");
 }
 
  if (lastButton == LOW && currentButton == HIGH)
  {
    digitalWrite(redLEDPin, LOW);
    //resetFlag = true;
  }
  lastButton = currentButton;
} */

void error(char *str) {

  Serial.print("Error: ");
  Serial.println(str);

  while (1); // Here the Program gets trapped
}

// Always gives a value between -300 and 300 Amperes with 0V => -300A and 5V => 300A
float readCurrent() {
  long Current = analogRead(CurrIn); // Always returns a value between 0 and 1023
  float CurrentVal = (Current / 1023.0) * 5.0; // Converting Digital Values (0 to 1023) to Analog Voltages
  CurrentVal = ((CurrentVal - 2.5) * 320); // -640 Amperes to 640 Amperes
  
  return CurrentVal;
}

// Always returns an Analog Voltage value between 0.00 and 5.00 Volts.
float readVoltage() {
  return ((analogRead(VoltIn) / 1023.0) * 100.0);
}

// Power in Watts, Energy in KiloJoules
void saveLogData(float Volt, float Current, float Power, unsigned long millisVal, float Energy) {
  // This opens a new file for writing if it doesn't exist
  // Else it will append the data to the end of the file
  logFile = SD.open(filename, FILE_WRITE);

  // Logging the PhotoVolt data to the SD Card
  logFile.print(millisVal);
  logFile.print(",");

  //Serial.print(millisVal);
  //Serial.print(", ");


  logFile.print(Volt, 1);
  logFile.print(",");

  //Serial.print(Volt);
  //Serial.print(", ");

  logFile.print(Current, 1);
  logFile.print(",");

  //Serial.print(Current);
  //Serial.print(", ");

  logFile.print(Power / 1000, 1); // Logging the Power in kW

  // Printing Power to Serial Monitor
  //Serial.print(Power / 1000);
  //Serial.print(", ");

    // When the power goes more than 10.2kW
    if (abs(Power/1000) >= 14.4) {
      logFile.print(" (Over Power)");
      //Serial.println("Power Outage");
      digitalWrite(redLEDPin, HIGH);
    }
  
  // Logging the Energy every 2 Seconds in the LogFile
  if ((millisVal % LOG_INTERVAL) == 0) {
    logFile.print(",");
    logFile.print(Energy, 0); // 2s Interval and Logging the Energy in kJ

    clearLCD();
    cursorSet(1, 0); // This sets the Cursor at the First Character on the First Line
    Serial.print(Energy, 0); // This will print the Energy onto the Serial LCD

    if (millisVal % 10000 == 0) {
      logFile.print(",");
      logFile.print("(Energy Stored)");
    }

    // Serial Monitor
    //Serial.print(Energy); // Storing the Energy Value in kJ
    
    // Writing to the Serial LCD
    //cursorSet(1, 0);
    //Serial.write("Energy: ");
    //cursorSet(1, 2);

    //Serial.write(Serial.print(Energy, 2));
    //cursorSet(1, 13);
    //Serial.write(" kJ");
  }

  logFile.println("");
  //Serial.println("");

  logFile.close();
}

// This is used to calculate the Moving Window Average.
// REMEMBER: In this case, we are considering an array of 20 Values but just taking an Moving Window Average of past 10 values
float MWAvg(int newIndex, float* sumVals, float* arrayVals) {

  int lastIndex = (10 + newIndex) % 20;
  *sumVals = *sumVals - arrayVals[lastIndex] + arrayVals[newIndex];
  return *sumVals / 10;
  //    return(sum - arrayVals[lastIndex] + arrayVals[newIndex]);
}

//  LCD  FUNCTIONS-- keep the ones you need.

// More Details at http://www.newhavendisplay.com/specs/NHD-0216K3Z-FL-GBW-V3.pdf on Page [7]

// clear the LCD
void clearLCD() {
  Serial.write(254); // Prefix: 0xFE => 254
  Serial.write(81); // 0x51 => 81
}

void displayOn() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(65);  // 0x41
}

void displayOff() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(66); // 0x42
}

// start a new line
void newLine() {
  cursorSet(1, 0); // 1st Row and Column 0.
}

// move the cursor to the home position
void cursorHome() {
  Serial.write(254);
  Serial.write(70); // Set the Cursor to (0,0) position on the LCD
}

// move the cursor to a specific place
// Here is xpos is Column No. and ypos is Row No.
void cursorSet(int ypos, int xpos) {
  Serial.write(254);
  Serial.write(69);

  // Bounding the X-Position
  if (xpos >= 15) {
    xpos = 15;
  }
  else if (xpos <= 0) {
    xpos = 0;
  }

  // Bounding the Y-Position
  if (ypos >= 1) {
    ypos = 1;
  }
  else if (ypos <= 0) {
    ypos = 0;
  }

  int finalPosition = ypos * 64 + xpos;

  Serial.write(finalPosition);
  //Serial.write(ypos); //Row position
}

// move cursor left by one Space
void cursorLeft() {
  Serial.write(254);
  Serial.write(73);  // 0x49
}

// move cursor right by One Space
void cursorRight() {
  Serial.write(254);
  Serial.write(74); // 0x4A
}
