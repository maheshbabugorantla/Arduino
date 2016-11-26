
// This below code is used to write to NHD 16x2 LCDs using RS-232 Port
void setup() {
  Serial.begin(9600); // Default RS 232 Baud Rate is 9600
  
  clearLCD(); // Clear the Contents of LCD
  displayOn(); // LCD Display On
  //underlineCursorOn(); // Switching On the Underline Cursor
  //blinkingCursorOn(); // Blinking Cursor On
}

//  MAIN CODE
void loop() {
  //backlightOn(0);  // turn the backlight on all the time
  clearLCD();
  cursorHome();
  Serial.write("Hello");  // print text to the current cursor position
  newLine();
  Serial.write("Dr. Barret");
  delay(1000);
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
  cursorSet(1,0); // 1st Row and Column 0.
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
void setContrast(int contrast){
  Serial.write(254); 
  Serial.write(82); // 0x52
  
  // Contrast value should be between 0 and 50 of size 1 byte
  if(contrast <= 0) {
    Serial.write(0);
  }
  else if(contrast >= 50) {
    Serial.write(50);
  }
  else {
  Serial.write(contrast);    
  }
}

void setBacklightBrightness(int brightness) {
 
 Serial.write(254); // 0xFE
 Serial.write(83); // 0x53
 
 if(brightness <= 0) {
   Serial.write(0);
 }
 else if(brightness >= 8) {
   Serial.write(8);
 }
 else {
   Serial.write(brightness);
 }
}

void blinkingCursorOn(){
   Serial.write(254); // 0xFE   
   Serial.write(75); // 0x4B
}

void blinkingCursorOff(){
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
