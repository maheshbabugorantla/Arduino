#include <EEPROM.h>
#include <SD.h>
#include<Adafruit_Sensor.h>
#include<Adafruit_ADXL345_U.h>
#include "RTClib.h"

/* Peripheral Devices */
// This is used to fetch the RealTime Clock
RTC_DS1307 rtc;

// This is used to refer to the Accelerometer (Initializations done in the setup() function)
Adafruit_ADXL345_Unified accel;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define LOG_INTERVAL 2000 // 2000 millisecond between updates
#define VOLT_CURRENT_LOG_INTERVAL 200 // 200 millisecond between updates
#define RPM_LOG_INTERVAL 10000 // 200 millisecond between updates
#define EEPROM_UPDATE_INTERVAL 10000 // 10 seconds between updates

#define CurrIn A0 // Potentiometer 1 (To Simulate the Voltage input)
#define VoltIn A1 // Potentiometer 2 (To Simulate the Current input)

#define AnalogIn2 A2 // Analog Input for something (WILL UPDATE LATER)
#define AnalogIn3 A3 // Analog Input for something (WILL UPDATE LATER)

// This pin is used to connect the Hall Effect Sensor
const int Hall_Effect_Sensor = 3; // Switch Off Button

// This Pin is used to clear the EEPROM Memory
// This is used to setup an interrupt to clear the EEPROM Memory when a button connected to Digital Pin 2 is pressed
const int ClearMemory = 2;

const int add_Digital_IO = 4; // Additional Digital IO (WILL UPDATE LATER)

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// The Logging File
File logFile;
char filename[] = "LOGGER55.csv";

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

// This is used to keep track of the no.of revolutions made
float revolutions = 0;

// This is used to store the RPM (Revolutions Per Minute) of the Kart
float rpm = 0;

// This is used to multiply the impulses computed for every 'x' ms as specified by ACCELEROMETER_LOG_INTERVAL
float rpm_factor = 0;

boolean resetFlag = false;

void setup() {
  // put your setup code here, to run once:

  // This is default RS 232 Baud Rate as well (Used for LCD Display).
  // Changing this Value from 9600 to some other value will make LCD Work Abnormally.
  Serial.begin(9600);

  unsigned long millisVal = millis();

  // Setting the pin Configurations
  pinMode(VoltIn, INPUT);   // Analog Pin 0
  pinMode(CurrIn, INPUT); // Analog Pin 1
  pinMode(Hall_Effect_Sensor, INPUT); // Configuring the Hall Effect Sensor Pin to the INPUT Mode
  pinMode(ClearMemory, INPUT); // Configuring the Clear the EEPROM Button

  // These pins will read 5V when there is no input from external interface or the no inputs are connected
  pinMode(AnalogIn2, INPUT_PULLUP); // Activating the Pull-Up Resistors for Analog Pin 2
  pinMode(AnalogIn3, INPUT_PULLUP); // Activating the Pull-Up Resistors for Analog Pin 3
    
  // Setting initial Pin Values
  digitalWrite(Hall_Effect_Sensor, HIGH); // Switching on the Internal Pull-Up Resistors
  digitalWrite(ClearMemory, HIGH); // Switching on the Internal Pull-Up Resistors

  /* Setting up all the interrupts */
  // Setting the EEPROM Clear Button
  attachInterrupt(digitalPinToInterrupt(ClearMemory), reset_eeprom, FALLING);
  // Setting the Clear for Power Outage LED
  attachInterrupt(digitalPinToInterrupt(Hall_Effect_Sensor), count_revolutions, FALLING);

  // Assigining a Unique ID to the Accelerometer Device and initializing it.
  accel = Adafruit_ADXL345_Unified(12345);
  
  // Beginning the RTC. If this step is not performed, then RTC will read out very abnormal Values.
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // The following line sets the RTC to date & time this sketch was compiled.
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  /*
   * Accelerometer Initialization
   */
  if(!accel.begin()) {
    Serial.println("Accel Failure");
    //while(1); //
  }

  // Setting the range of g's the accelerometer can measure
  accel.setRange(ADXL345_RANGE_16_G);

  // This is used to compute RPM of the vehicle from the count of impulses
  rpm_factor = ((1000 * 60) / RPM_LOG_INTERVAL);

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
    delay(2000);
    clearLCD();
    //return; //
  }

  Serial.println("Card Initialized");
  clearLCD();
  delay(1000);

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

  logFile.println("millis,Volt,Current,Power(kW),Energy(kJ),Analog Input 2,Analog Input 3,X-Accel(m/s^2),Y-Accel(m/s^2),Z-Accel(m/s^2),RPM");
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

  // Logging the Moving Window average of the Voltage and Current for every 200 ms.
  if (millisVal % VOLT_CURRENT_LOG_INTERVAL == 0) {

    counter = (counter + 1) % 20;

    // Need to calculate the Running Average
    VoltVals[counter] = readVoltage();
    CurrentVals[counter] = -1*readCurrent(); // Current Reversal is being done due to current sensor configuration on the physics box

    // Calculating the Running Average of Voltage and Current every 200 ms
    avgVolt = MWAvg(counter, &sumVoltage, VoltVals);
    avgCurrent = MWAvg(counter, &sumCurrent, CurrentVals);
    float Power = avgVolt * avgCurrent;
    
    newEnergy += Power * 0.2; // This always the newly computed Energy

    // Getting new Accelerometer Event
    sensors_event_t accelEvent;
    accel.getEvent(&accelEvent); // Library Call
    
    // Computing the RPM for every 10 seconds
    if(millisVal % RPM_LOG_INTERVAL == 0)
    {
      rpm = (revolutions * rpm_factor);
      revolutions = 0;
    }

    float analog_input_2 = readAnalogInput2();
    float analog_input_3 = readAnalogInput3();

    // Saving the Data to a LogFile.
    saveLogData(avgVolt, avgCurrent, Power, millisVal, Energy + newEnergy / 1000, analog_input_2, analog_input_3, rpm, accelEvent);
  }

  // Storing the Energy into the EEPROM every 10 Second
  if (millisVal % EEPROM_UPDATE_INTERVAL == 0) {
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

// This keeps count of the no. of revolutions for every 'x' ms as specified by VOLT_CURRENT_LOG_INTERVAL 
void count_revolutions() {
  revolutions += 1;  
}

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

float readAnalogInput2() {
  return (analogRead(A2) * (5.0 / 1023.0));
}

float readAnalogInput3() {
  return (analogRead(A3) * (5.0 / 1023.0));
}

// Power in Watts, Energy in KiloJoules
void saveLogData(float Volt, float Current, float Power, unsigned long millisVal, float Energy, float AI2, float AI3, float rpm, sensors_event_t accelEvent) {
  // This opens a new file for writing if it doesn't exist
  // Else it will append the data to the end of the file
  logFile = SD.open(filename, FILE_WRITE);

  Serial.println("Inside write file");

  // Logging the PhotoVolt data to the SD Card
  logFile.print(millisVal);
  logFile.print(",");

  //Serial.print(millisVal);
  //Serial.print(", ");

  logFile.print(Volt);
  logFile.print(",");

  //Serial.print(Volt);
  //Serial.print(", ");

  logFile.print(Current);
  logFile.print(",");

  logFile.print(Power / 1000); // Logging the Power in kW
  
  // When the power goes more than 10.2kW
//  if (abs(Power/1000) >= 14.4) {
//     digitalWrite(redLEDPin, HIGH);
//  }

  // Logging the Energy every 2 Seconds in the LogFile
  if ((millisVal % LOG_INTERVAL) == 0) {
    logFile.print(",");
    logFile.print(Energy); // 2s Interval and Logging the Energy in kJ

    clearLCD();
    cursorSet(1, 0); // This sets the Cursor at the First Character on the First Line
    Serial.print(Energy); // This will print the Energy onto the Serial LCD

    if (millisVal % EEPROM_UPDATE_INTERVAL == 0) {
      logFile.print(",");
      logFile.print("(Energy Stored)");
    }
  }

  // Logging the Analog Input 2
  logFile.print(",");
  logFile.print(AI2);

  // Logging the Analog Input 3
  logFile.print(",");
  logFile.print(AI3);

  // Logging the RPM
  logFile.print(",");
  logFile.print(rpm);

  // X-Axis g-Force (Acceleration)
  logFile.print(",");
  logFile.print(accelEvent.acceleration.x);

  // Y-Axis g-Force (Acceleration)
  logFile.print(",");
  logFile.print(accelEvent.acceleration.y);

  // Z-Axis g-Force (Acceleration)
  logFile.print(",");
  logFile.print(accelEvent.acceleration.z);
  
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

/*void underlineCursorOn() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(71); // 0x47
}

void underlineCursorOff() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(72); // 0x48
}*/

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

// backspace and erase previous character
/*void backSpace() {
  Serial.write(254);
  Serial.write(78); // 0x4E
}*/

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

// set LCD contrast
/*void setContrast(int contrast) {
  Serial.write(254);
  Serial.write(82); // 0x52

  // Contrast value should be between 0 and 50 of size 1 byte
  if (contrast <= 0) {
    Serial.write(0);
  }
  else if (contrast >= 50) {
    Serial.write(50);
  }
  else {
    Serial.write(contrast);
  }
}

void setBacklightBrightness(int brightness) {

  Serial.write(254); // 0xFE
  Serial.write(83); // 0x53

  if (brightness <= 0) {
    Serial.write(0);
  }
  else if (brightness >= 8) {
    Serial.write(8);
  }
  else {
    Serial.write(brightness);
  }
}

void blinkingCursorOn() {
  Serial.write(254); // 0xFE
  Serial.write(75); // 0x4B
}

void blinkingCursorOff() {
  Serial.write(254); // 0xFE
  Serial.write(76); // 0x4C
}

// Shift the Display Left by One Space
// REMEMBER: The Cursor Position also moves with the Display and the Display data is not altered
void shiftLeft() {
  Serial.write(254); // 0xFE
  Serial.write(85); // 0x55
}

// Shift the Display Right by One Space
// REMEMBER: The Cursor Position also moves with the Display and the Display data is not altered
void shiftRight() {
  Serial.write(254); // 0xFE
  Serial.write(86); // 0x56
} */
