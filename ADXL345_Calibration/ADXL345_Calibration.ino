#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
 
/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345); // I2C Mode 

/* Variables for Accelerometer */ 
float AccelMinX = 0;
float AccelMaxX = 0;
float AccelMinY = 0;
float AccelMaxY = 0;
float AccelMinZ = 0;
float AccelMaxZ = 0;

int count = 0;
float avg_x = 0;
float avg_y = 0;
float avg_z = 0;

float x0g = 0.5;
float y0g = -0.25;
float z0g = -0.8;

float avg_x_buffer[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float avg_y_buffer[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float avg_z_buffer[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Sub-Routines for Accelerometer  */
float averageAccel(float* array) {
  int index = 0;
  float sum = 0;
  
  for(index = 0; index < 10; index++)
  {
    sum += array[index];    
  }
  return sum / 10;
}
 
 void readAccel_Data() {
        /* Get a new sensor event */ 
    sensors_event_t accelEvent;  
    accel.getEvent(&accelEvent);
    
    float accel_x = accelEvent.acceleration.x;
    float accel_y = accelEvent.acceleration.y;
    float accel_z = accelEvent.acceleration.z;
    
    if((millis() % 100) == 0)
     {    
         avg_x_buffer[count] = accel_x;
         avg_y_buffer[count] = accel_y;             
         avg_z_buffer[count] = accel_z;
         
         count = (count + 1) % 10;
                  
         avg_x = averageAccel(avg_x_buffer) - x0g;
         avg_y = averageAccel(avg_y_buffer) - y0g;
         avg_z = averageAccel(avg_z_buffer) - z0g;
         
         float net_accel = abs(sqrt( sq(avg_x) + sq(avg_y) + sq(avg_z) ) - 9.8);
         
         Serial.print("X: "); Serial.print(avg_x); Serial.print(", Y: "); Serial.print(avg_y); Serial.print(", Z: ");Serial.print(avg_z);
         Serial.print(", Net Acceleration: "); Serial.print(net_accel);
         Serial.println(" ");         
      }
} 
  
void setup(void) 
{
  Serial.begin(9600);
  Serial.println("ADXL345 Accelerometer Calibration"); 
  Serial.println("");
  
  /* Initialise the sensor */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
  }
}
 
void loop(void)
{
   readAccel_Data();
}
