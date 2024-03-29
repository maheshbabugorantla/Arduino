#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include "RTClib.h"

RTC_DS1307 rtc; // This is used to fetch the RealTime Clock
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define LOG_INTERVAL 2000 // millis between entries

#define VoltIn A0 // Potentiometer 1 (To Simulate the Voltage input)
#define CurrIn A1 // Potentiometer 2 (To Simulate the Current input)

// Digital pins that connect to LEDs
const int redLEDPin = 7; // Warning LED
const int BUTTON = 2; // Switch Off Button

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// The Logging File
File logFile;
char filename[] = "LOGGER51.csv";

// Analog Sensors

// Used for Debouncing Push Button
boolean currentButton = LOW;
boolean lastButton = HIGH;

int counter = 0;
float PowerVals[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Used to compute the running average.
float VoltVals[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Used to compute the running average of Volts
float CurrentVals[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Used to compute the running average of Amperes

float avgVolt = 0;
float avgCurrent = 0;
float avgPower = 0;

float Energy; // = 0.000f;
float Energy1; // = 0.000f;
float newEnergy;


//float** Energy_ptr;

boolean resetFlag = false;

int reset = 1;

void setup() {
  // put your setup code here, to run once:

  // This is default RS 232 Baud Rate as well (Used for LCD Display).
  // Changing this Value from 9600 to some other value will make LCD Work Abnormally.
  Serial.begin(9600);

  pinMode(VoltIn, INPUT);   // Analog Pin 0
  pinMode(CurrIn, INPUT); // Analog Pin 1
  pinMode(BUTTON, INPUT); // Configuring the Button Pin to the INPUT Mode
  pinMode(redLEDPin, OUTPUT); // Configuring the Warning LED Pin to the OUTPUT Mode

  // Resetting the Warning LED
  digitalWrite(redLEDPin, LOW);

  // Beginning the RTC. If this step is not performed, then RTC will read out very abnormal Values.
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // The following line sets the RTC to date & time this sketch was compiled.
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // Initialize the SD Card
  Serial.print("Initializing SD card... ");

  // Make sure that default chip select pin is set to output
  // even if you don't use it:
  pinMode(chipSelect, OUTPUT);

  // Checking if the SD Card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }

  Serial.println("Card Initialized");

  Serial.print("Logging to: ");
  Serial.println(filename);

  logFile = SD.open(filename, FILE_WRITE);
  // Don't forget to use RTC here.
  // Insert the Time Stamp for new logging

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
  Serial.print(now.year(), DEC);
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
  Serial.println();

  logFile.println("millis,light,Power,Energy");
  logFile.close();
  Serial.println("millis,light,Power,Energy");

  clearLCD(); // Clear the Contents of LCD.
  displayOn(); // LCD Display On
  cursorHome(); // Set the LCD

  // Getting the Stored Energy values from the EEPROM.
  /*EEPROM.get(0,Energy);
  float newEnergy = Energy;
  Serial.print(newEnergy);
  Serial.print(" Joules");
  Serial.println("");
  EEPROM.put(0, newEnergy); */

  
  /* EEPROM.get(100,Energy1);
  float newEnergy1 = Energy1;
  Serial.print(newEnergy1);
  Serial.print(" Joules");
  Serial.println("");
  EEPROM.put(100, newEnergy1); */

  //*Energy_ptr = &Energy;
  
  //Serial.println(Energy,3);
  //Serial.println(Energy1,3);
  
  //**Energy_ptr = Energy;
  
//  EEPROM.put(0,Energy);
//  EEPROM.put(10,Energy1);
  
  clearLCD(); // Clear the Contents of LCD.
  
  reset = 1;
}

void loop() {
  
  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();

  // This function is used to read the input from the External RESET Button
  //externalReset();
  
  unsigned long millisVal = millis();

  float Volt = readVoltage(); // Instantaneous Battery Voltage
  float Power = Volt * readCurrent(); // Instananeous Power

  // Reading the Power every 100 milliseconds
  if (millisVal % 100 == 0) {
    // Reading the Instanenous Power Values Pi, into a Circular Buffer
    counter = (counter + 1) % 10;
    PowerVals[counter] = Power;
    
    // Need to calculate the Running Average
    VoltVals[counter] = Volt;
    CurrentVals[counter] = readCurrent();
  }

  // Logging the Running average of the Voltage and Current for every 200 ms.
  if(millisVal % 200 == 0) {
    
     // Calculating the Running Average of Voltage and Current every 200 ms
     avgVolt = calcAvg(VoltVals, 10);
     avgCurrent = calcAvg(CurrentVals, 10);
    
     logFile = SD.open(filename, FILE_WRITE);
     logFile.print(millisVal);
     logFile.print(",");
     logFile.print(avgVolt); // Running Average of Voltage
     logFile.print(",");
     logFile.print(avgCurrent); // Running Average of Current
     logFile.close(); // Closing the logFile file pointer.
  }
  
  // Logging the Power every 1 Second
  if (millisVal % 1000 == 0) {
    
    // The below line is used to compute the 1Sec average Power
    avgPower = calcAvg(PowerVals, 10);

    // Writing the Power Value to the LCD
    cursorHome();
    Serial.write("Power: ");
    cursorSet(0, 8); // Here 8 is no. of characters in "Power: " + 1, 0 => First Line
    Serial.write(Serial.print(avgPower / 1000, 2));
    cursorSet(0, 13);
    Serial.write(" kW");

    // Fetching the stored EEPROM value when the board is reset or is powered.      
    EEPROM.get(0, Energy);
    clearLCD();
    Energy += abs(avgPower)/1000; // Finding the Energy Consumption every Second

    // Writing the Value of Energy to the EEPROM Memory so that the most recent Energy Value is available even when the power is switched off.
    EEPROM.put(0, Energy);
    EEPROM.put(100, Energy); // Just Making a Duplicate Copy
    
    saveLogData(avgPower, millisVal, Energy); // Logging the Power Data
  }
}

// Debouncing PushButton
boolean debounce(boolean lastButton)
{
  boolean currentButton = digitalRead(BUTTON);

  if (lastButton != currentButton)
  {
    delay(10); // Delaying for 10ms will allow the button state to settle down
    currentButton = digitalRead(BUTTON);
  }

  return (currentButton);
}

// This is used to reset the RedLight (That indicates the Energy Outage)
void externalReset()
{
  currentButton = debounce(lastButton);

  if (lastButton == LOW && currentButton == HIGH)
  {
    digitalWrite(redLEDPin, LOW);
    //resetFlag = true;
  }
  lastButton = currentButton;
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
  CurrentVal = ((CurrentVal - 2.5) / 2.5) * 300; // 300 Amperes
  return CurrentVal;
}

// Always returns an Analog Voltage value between 0.00 and 5.00 Volts.
float readVoltage() {
  return ((analogRead(VoltIn) / 1023.0) * 5.0);
}

// Power in Watts, Energy in KiloJoules
void saveLogData(float Power, unsigned long millisVal, float Energy) {
  // This opens a new file for writing if it doesn't exist
  // Else it will append the data to the end of the file
  logFile = SD.open(filename, FILE_WRITE);

  // Logging the PhotoVolt data to the SD Card
  logFile.print(millisVal);
  logFile.print(",");

  Serial.println("");
  Serial.print(millisVal);
  Serial.print(", ");

//  logFile.print(Volt);
//  logFile.print(",");

  logFile.print(Power/1000); // Logging the Power in kW

  Serial.print(Power/1000);
  
  // Logging the Energy every 10 Seconds
  if ((millisVal % (5 * LOG_INTERVAL)) == 0) {
    logFile.print(",");
    logFile.print(Energy); // 10s Interval and Logging the Energy in kJ

    if ((Energy) > 10) {
      digitalWrite(redLEDPin, HIGH);
    }

    // Serial Monitor
    Serial.print(",");
    Serial.print(Energy); // Storing the Energy Value in kJ

    // Writing to the Serial LCD
    cursorSet(1, 0);
    Serial.write("Energy: ");
    cursorSet(1, 9);

    Serial.write(Serial.print(Energy, 2));
    cursorSet(1, 13);
    Serial.write(" kJ");
  }

  logFile.println("");
  Serial.println("");

  logFile.close();
}

float calcAvg(float* arrayVals, int vals) {

  int index = 0;
  float sum = 0;
  for (index = 0; index < vals; index++) {
    sum += arrayVals[index];
  }
  return sum / vals;
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

void underlineCursorOn() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(71); // 0x47
}

void underlineCursorOff() {
  Serial.write(254); // Prefix 0xFE
  Serial.write(72); // 0x48
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

// backspace and erase previous character
void backSpace() {
  Serial.write(254);
  Serial.write(78); // 0x4E
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


// set LCD contrast
void setContrast(int contrast) {
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
}
