
#include<EEPROM.h>

int base_address = 0;
int base_address_1 = 100;

void setup() {
  // put your setup code here, to run once:
    
}

void loop() {

  // The below loop are used to clear the Memmory blocks that store the Energy Data

  // Erasing the first memory Block.
  for(int index=0; index<sizeof(float); index++) {
      EEPROM.write(base_address + index, 0);
  }

  // Erasing the Second Memory Block.
  for(int index=0; index<sizeof(float); index++) {
     EEPROM.write(base_address_1 + index, 0);
  }
}
