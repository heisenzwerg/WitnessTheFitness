#include <EEPROM.h>

const int xpin = A1;                                  //input analogue pin for x
int ypin = A2;                                        //input analogue  pin for Y

int threshhold;                             //threshhold value
int height;

int steps, flag = 0;                                  //define steps as integer. Set flag to 0.
float totvect;                                        //define totvect as decimal
float xaccl;                                          //define xaccl as decimal
float yaccl;                                          //define xaccl as decimal

void setup()
{
  Serial.begin(9600);

  if (EEPROM.read(1) == 255) {
    EEPROM.write(1, 180);
  }

  if (Serial.available()) {
    EEPROM.write(1, Serial.read());
  }

  height = EEPROM.read(1);;

  if (height >= 180) {
    threshhold = 540;
  }
  else if (height > 160 && height < 180) {
    threshhold = 525;
  }
  else {
    threshhold = 515;
  }


}

void loop()
{

  if (Serial.available()) {
    height = Serial.read();
    EEPROM.write(1, height);

    if (height >= 180) {
      threshhold = 540;
    }
    else if (height > 160 && height < 180) {
      threshhold = 525 ;
    }
    else {
      threshhold = 515;
    }
  }


  xaccl = (analogRead(xpin));                            //read x axis
  delay(1);

  yaccl = (analogRead(ypin));                            //read y axis
  delay(1);

  totvect = sqrt(((xaccl) * (xaccl)) + ((yaccl) * (yaccl))); //pythagoras vector calculation to combine X (up/down) and Y (forward/backwards movement)
  //Serial.println(totvect);
  delay(200);

  if (totvect > threshhold && flag == 0)
  {
    Serial.print(1);
    flag = 1;                                            //if threshhold is crossed, count a step and set the flag.

  }
  else if (totvect > threshhold && flag == 1)            //do nothing until flag is removed. Stops mutilple readings, during one step, counting up.
  {

  }
  if (totvect < threshhold && flag == 1)                 //if reading is below thresdhold, remove the flag to allow for the next step to be recorded.
  {
    flag = 0;
  }
  //Serial.println('\n');
  //Serial.print("steps=");
  //Serial.print(1);
}
