

#include <avr/pgmspace.h>         //This is a built-in Arduino library, all credits and how-to-use at https://www.arduino.cc/reference/en/language/variables/utilities/progmem/
#include <Wire.h>                 //This lib handles I2C protocoll, all credits and how-to-use at https://www.arduino.cc/en/Reference/Wire
#include <U8x8lib.h>              //This lib handles the OLED display drivers, all credits and how-to-use https://github.com/olikraus/u8g2/wiki/u8x8reference, from Olli Kraus
#include <RTClib.h>               //This lib handles the RTC, all credits and how-to-use at https://github.com/adafruit/RTClib/blob/master/RTClib.h//Uses, from Adafuit         //pins A4 and A5 
#include <EEPROM.h>               //This is a built-in Arduino library, all credits and how-to-use at https://www.arduino.cc/en/Reference/EEPROM
#include <SPI.h>                  //This is a built-in Arduino library, all credits and how-to-use at https://www.arduino.cc/en/Reference/SPI            //Uses pins D11, D12 and D13                   //Uses pin D2 as chip select pin (SPI protocoll)
#include "SdFat.h"                //This lib handles the data storing on SD card, all credits and how-to-use at https://github.com/greiman/SdFat, from greimann


//initialization of display
U8X8_SSD1306_128X64_NONAME_4W_SW_SPI u8x8(/* clock=*/ 4, /* data=*/ 6, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

//which button uses which pin is defined here
#define pin_settings A2               //Uses pins D4 to D7
#define pin_start_stop A3
#define pin_up A0
#define pin_select A1

//interrupt pins
#define pin_ISR_heartRate 2       //Uses pins D2 and D3
//#define pin_ISR_stepCount 3     //stepCount isn't using an ISR anymore. We had to adjust the concept to the way the xBee is working.

//declaration for RTC
RTC_DS1307 rtc;
unsigned long time_start;
unsigned long time_elapsed;
DateTime nowx;

//for saving to file
File currentLog;
#define address_number_of_files 200

//for step counting & heart beat measuring
int steps_total = 0;
int steps_since = 0;
byte beats_since = 0;
byte bpm = 0;
byte bpm_old = 0;
byte bpm_new = 0;

//variables for saving in EEPROM
#define address_height 100
#define address_weight 110
#define address_gender 120
#define address_unit   130

//variables for SD card routine
SdFat SD;
#define SD_CS_PIN 5
#define every_x_seconds 6   //run save_to_SD every x seconds
//#define chipSelect 5   //Pin for selecting the SD card

//variables for void loop
byte mode_settings = 0;
byte mode_running = 0;
bool runnings = false;
float calories;

//Display the different options in main menu
unsigned long start_millis_loop;
unsigned long start_millis_loop_plus3000;

//variables for settings loop
byte temporary_height;
byte temporary_weight;
char temporary_gender;
char temporary_unit;

const char run_started[16] = "Run starts in...";
const char welcome_back[16] = "Welcome back!";

//this is for continous filenames, all credits to https://www.reddit.com/r/arduino/comments/2qe97j/help_with_dynamic_file_naming_for_a_datalogger/
unsigned int number_of_files;
char* give_me_filename(int count) {
  static char filename[12];
  snprintf(filename, 12, "Log%02d.txt", count);
  return filename;
}

void setup() {

  Serial.begin(9600);
  Wire.begin();
  SPI.begin();

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  //rtc.adjust(DateTime(2018, 12, 7, 16, 27, 30));

  //set pins for out buttons to INPUT
  pinMode(pin_settings, INPUT);
  pinMode(pin_start_stop, INPUT);
  pinMode(pin_up, INPUT);
  pinMode(pin_select, INPUT);
  pinMode(pin_ISR_heartRate, INPUT);
  //pinMode(pin_ISR_stepCount, INPUT);      //not used anymore
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(1, OUTPUT);

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  if (Serial)  {
    u8x8.setCursor(0, 0);
    u8x8.print(F("Serial"));
    u8x8.setCursor(0, 1);
    u8x8.print(F("initialized!"));
  }

  else {
    u8x8.setCursor(0, 0);
    u8x8.print(F("Serial cannot"));
    u8x8.setCursor(0, 1);
    u8x8.print(F("be initialized!"));
    while (1);
  }

  delay(2000);

  // Initialize SD card
  u8x8.setCursor(0, 0);
  u8x8.print(F("Initializing"));
  u8x8.setCursor(0, 1);
  u8x8.print(F("SD card...! "));

  delay(1500);
  if (!SD.begin(SD_CS_PIN)) {     //SD_CS_PIN to select the proper pin

    u8x8.setCursor(0, 4);
    u8x8.print(F("Initialization"));
    u8x8.setCursor(0, 5);
    u8x8.print(F("failed!"));
    while (1); //infinite loop
  }

  u8x8.setCursor(0, 4);
  u8x8.print(F("Initialization"));
  u8x8.setCursor(0, 5);
  u8x8.print(F("done! "));
  //SD card initialized

  //Initialize EEPROM and read settings from there
  if (EEPROM.read(address_height) == 255)  {    //address_height
    EEPROM.write(address_height, 180);
  }

  if (EEPROM.read(address_weight) == 255)  {
    EEPROM.write(address_weight, 70);
  }

  if (EEPROM.read(address_gender) == 255)  {
    EEPROM.write(address_gender, 'm');
  }

  if (EEPROM.read(address_unit) == 255)  {
    EEPROM.write(address_unit, 'm');
  }
  //EEPROM initialized

  delay(2000);

  u8x8.clear();
  animation(welcome_back, 3);
  delay(3000);
  u8x8.clear();

  start_millis_loop = millis();
  start_millis_loop_plus3000 = start_millis_loop + 3000;
  delay(200);
}

void loop() {

  nowx = rtc.now();                                             // this is for getting the time and write it to display
  u8x8.setCursor(0, 0);
  u8x8.print(F("Time:"));
  u8x8.setCursor(0, 1);
  char hours[3];
  sprintf(hours, "%02u", nowx.hour());
  u8x8.print(hours);
  u8x8.setCursor(2, 1);
  u8x8.print(F(":"));
  u8x8.setCursor(3, 1);
  char minutes[3];
  sprintf(minutes, "%02u", nowx.minute());
  u8x8.print(minutes);
  u8x8.setCursor(5, 1);
  u8x8.print(F(":"));
  u8x8.setCursor(6, 1);
  char seconds[3];
  sprintf(seconds, "%02u", nowx.second());
  u8x8.print(seconds);

  if (start_millis_loop <= (millis() - 6000))  {                  //this is for the hints regarding how to use the device

    start_millis_loop = millis();
    u8x8.setCursor(0, 4);
    u8x8.print(F("Press ""Settings"""));
    u8x8.setCursor(0, 5);
    u8x8.print(F("to access the"));
    u8x8.setCursor(0, 6);
    u8x8.print(F("settings menu."));
  }

  if (start_millis_loop_plus3000 <= (millis() - 6000))  {         //this is for the hints regarding how to use the device

    start_millis_loop_plus3000 = millis();
    u8x8.setCursor(0, 4);
    u8x8.print(F("Press ""Start/  "));
    u8x8.setCursor(0, 5);
    u8x8.print(F("Stop"" to start "));
    u8x8.setCursor(0, 6);
    u8x8.print(F("your training! "));
  }
  //End of main menu

  //checks if settings pin is pressed and sets settings variable to true if so
  if (digitalRead(pin_settings) == HIGH)  {

    //inititalize temporary variables for settings menu
    temporary_height = EEPROM.read(address_height);    //address_height
    temporary_weight = EEPROM.read(address_weight);
    temporary_gender = EEPROM.read(address_gender);
    temporary_unit   = EEPROM.read(address_unit);
    mode_settings = 0;
    delay(200);
    Settings();
  }

  if (digitalRead(pin_start_stop) == HIGH) {

    DateTime time_starts = rtc.now();
    time_start = time_starts.unixtime();
    mode_running = 0;
    delay(200);
    Running();
  }
}

void Settings() {             //Code for settings, please dont change anything here because it's kinda complex code ;

  u8x8.clear();
  animation("Enter settings.", 3);
  delay(1000); //Don't delete this delay! Necessary to have some delay for button inputs, otherwise we would possibly immediatly exit the menu if we press settings for too long
  u8x8.clear();

START:

  switch (mode_settings) {    //this switches between the different modes, mode 0 is for height, mode 1 is for weight

    default:                  //in case of failure, mode settings is restored to 0
      mode_settings = 0;

      break;

    case 0:                   //if mode==0, this case will run

      u8x8.setCursor(0, 0);
      u8x8.print(F("Height: "));             //writes the menu interface on the screem
      u8x8.setCursor(9, 0);
      u8x8.print(temporary_height);
      u8x8.drawString(14, 0, "cm");

      if (digitalRead(pin_up) == HIGH)  {           //if up pin is pressed, mode is increased by 1
        mode_settings = mode_settings + 1;

        if (mode_settings == 4) {
          mode_settings = 0;
        }
        delay(200);
        u8x8.clear();
        goto START;
      }
      if (digitalRead(pin_select) == HIGH) {   //if select pin is pressed, we can access and increase the variable height; this is a loop that repeatedly starts at Start2 again and again

        u8x8.setCursor(9, 0);
        u8x8.print(F("   "));
        u8x8.setCursor(0, 0);
        u8x8.print(F("Height: "));                                    //line 144 to line 163 makes the selected variable blink
        unsigned long start_millis_height = millis();
        unsigned long start_millis_height_plus500 = start_millis_height + 500;       //both start_millis variables are needed for blinking
        delay(500);                                                               //delay is important here

START2:

        if (start_millis_height <= (millis() - 1000))  {

          start_millis_height = millis();

          u8x8.setCursor(0, 0);
          u8x8.print(F("Height: "));
          u8x8.setCursor(9, 0);
          u8x8.print(temporary_height);
          u8x8.drawString(14, 0, "cm");
        }

        if (start_millis_height_plus500 <= (millis() - 1000))  {

          start_millis_height_plus500 = millis();
          u8x8.setCursor(0, 0);
          u8x8.print(F("Height:         "));
        }

        if (digitalRead(pin_up) == HIGH) {          //if up pin is pressed in this instance, then height is increased (notice the different function of the up pin as compared to before)

          temporary_height = temporary_height + 5;

          if (temporary_height >= 225) {
            temporary_height = 100;

          }

          u8x8.setCursor(0, 0);
          u8x8.print(F("Height: "));
          u8x8.setCursor(9, 0);
          u8x8.print(temporary_height);
          u8x8.drawString(14, 0, "cm");
          delay(500);
          start_millis_height = millis();
          start_millis_height_plus500 = start_millis_height + 500;
        }

        if (digitalRead(pin_select) == HIGH) {                  //if select pin is pressed, height will be saved permanently and the loop for increasing the height is left (-> goto Start)

          EEPROM.write(address_height, temporary_height);   //address_height
          u8x8.clear();
          animation("Saved!         ", 3);
          delay(2000);
          u8x8.clear();
          goto START;
        }

        if (digitalRead(pin_settings) == HIGH) {                //if settings is pressed, we end end the settings loop and go back to void loop() (->goto END_SETTINGS)

          goto END_SETTINGS;
        }

        goto START2;
      }

      if (digitalRead(pin_settings) == HIGH) {

        goto END_SETTINGS;
      }

      break;

    case 1:                                                               //for description look at case 0, works exactly the same, except for weight instead of height

      u8x8.setCursor(0, 0);
      u8x8.print(F("Weight: "));             //writes the menu interface on the screem
      u8x8.setCursor(9, 0);
      u8x8.print(temporary_weight);
      u8x8.drawString(14, 0, "kg");

      if (digitalRead(pin_up) == HIGH)  {           //if up pin is pressed, mode is increased by 1
        mode_settings = mode_settings + 1;

        if (mode_settings == 4) {
          mode_settings = 0;

        }
        delay(200);
        u8x8.clear();
        goto START;
      }
      if (digitalRead(pin_select) == HIGH) {   //if select pin is pressed, we can access and increase the variable weight; this is a loop that repeatedly starts at Start2 again and again

        u8x8.setCursor(9, 0);
        u8x8.print(F("   "));
        u8x8.setCursor(0, 0);
        u8x8.print(F("Weight: "));                                    //line 144 to line 163 makes the selected variable blink
        unsigned long start_millis_weight = millis();
        unsigned long start_millis_weight_plus500 = start_millis_weight + 500;       //both start_millis variables are needed for blinking
        delay(500);                                                               //delay is important here

START3:

        if (start_millis_weight <= (millis() - 1000))  {

          start_millis_weight = millis();

          u8x8.setCursor(0, 0);
          u8x8.print(F("Weight: "));
          u8x8.setCursor(9, 0);
          u8x8.print(temporary_weight);
          u8x8.drawString(14, 0, "kg");
        }

        if (start_millis_weight_plus500 <= (millis() - 1000))  {

          start_millis_weight_plus500 = millis();
          u8x8.setCursor(0, 0);
          u8x8.print(F("Weight:         "));
        }

        if (digitalRead(pin_up) == HIGH) {          //if up pin is pressed in this instance, then weight is increased (notice the different function of the up pin as compared to before)

          temporary_weight = temporary_weight + 5;

          if (temporary_weight >= 155) {
            temporary_weight = 40;
            u8x8.clear();

          }

          u8x8.setCursor(0, 0);
          u8x8.print(F("Weight: "));
          u8x8.setCursor(9, 0);
          u8x8.print(temporary_weight);
          u8x8.drawString(14, 0, "kg");
          delay(500);
          start_millis_weight = millis();
          start_millis_weight_plus500 = start_millis_weight + 500;
        }

        if (digitalRead(pin_select) == HIGH) {                  //if select pin is pressed, weight will be saved permanently and the loop for increasing the weight is left (-> goto Start)

          EEPROM.write(address_weight, temporary_weight);
          u8x8.clear();
          animation("Saved!         ", 3);
          delay(2000);
          u8x8.clear();
          goto START;
        }

        if (digitalRead(pin_settings) == HIGH) {                //if settings is pressed, we end end the settings loop and go back to void loop() (->goto END_SETTINGS)

          goto END_SETTINGS;
        }

        goto START3;
      }

      if (digitalRead(pin_settings) == HIGH) {

        goto END_SETTINGS;
      }

      break;


    case 2:                                                               //for description look at case 0, works exactly the same, except for weight instead of height

      u8x8.setCursor(0, 0);
      u8x8.print(F("Gender: "));             //writes the menu interface on the screem
      u8x8.setCursor(8, 0);

      if (temporary_gender == 'm') {
        u8x8.print(F("Male  "));
      }

      else {
        u8x8.print(F("Female"));
      }


      if (digitalRead(pin_up) == HIGH)  {           //if up pin is pressed, mode is increased by 1
        mode_settings = mode_settings + 1;

        if (mode_settings == 4) {
          mode_settings = 0;

        }
        delay(200);
        u8x8.clear();
        goto START;
      }
      if (digitalRead(pin_select) == HIGH) {   //if select pin is pressed, we can access and increase the variable weight; this is a loop that repeatedly starts at Start2 again and again

        u8x8.setCursor(8, 0);
        u8x8.print(F("        "));
        u8x8.setCursor(0, 0);
        u8x8.print(F("Gender: "));                                    //line 144 to line 163 makes the selected variable blink
        delay(100);
        unsigned long start_millis_gender = millis();
        delay(100);
        unsigned long start_millis_gender_plus500 = start_millis_gender + 500;       //both start_millis variables are needed for blinking
        delay(300);                                                               //delay is important here

START13:

        if (start_millis_gender <= (millis() - 1000))  {

          start_millis_gender = millis();

          u8x8.setCursor(0, 0);
          u8x8.print(F("Gender: "));
          u8x8.setCursor(8, 0);

          if (temporary_gender == 'm') {
            u8x8.print(F("Male  "));
          }

          else {
            u8x8.print(F("Female"));
          }
        }

        if (start_millis_gender_plus500 <= (millis() - 1000))  {

          start_millis_gender_plus500 = millis();
          u8x8.setCursor(0, 0);
          u8x8.print(F("Gender:         "));
        }

        if (digitalRead(pin_up) == HIGH) {          //if up pin is pressed in this instance, then weight is increased (notice the different function of the up pin as compared to before)

          if (temporary_gender == 'm') {
            temporary_gender = 'f';
          }

          else {
            temporary_gender = 'm';
          }

          delay(100);
          u8x8.setCursor(0, 0);
          u8x8.print(F("Gender: "));
          u8x8.setCursor(8, 0);
          if (temporary_gender == 'm') {
            u8x8.print(F("Male  "));
          }

          else {
            u8x8.print(F("Female"));
          }
          delay(400);
          start_millis_gender = millis();
          start_millis_gender_plus500 = start_millis_gender + 500;
        }

        if (digitalRead(pin_select) == HIGH) {                  //if select pin is pressed, weight will be saved permanently and the loop for increasing the weight is left (-> goto Start)

          EEPROM.write(address_gender, temporary_gender);
          u8x8.clear();
          animation("Saved!         ", 3);
          delay(2000);
          u8x8.clear();
          goto START;
        }

        if (digitalRead(pin_settings) == HIGH) {                //if settings is pressed, we end end the settings loop and go back to void loop() (->goto END_SETTINGS)

          goto END_SETTINGS;
        }

        goto START13;
      }

      if (digitalRead(pin_settings) == HIGH) {

        goto END_SETTINGS;
      }

      break;

    case 3:                                                               //for description look at case 0, works exactly the same, except for weight instead of height

      u8x8.setCursor(0, 0);
      u8x8.print(F("Units: "));             //writes the menu interface on the screem
      u8x8.setCursor(8, 0);

      if (temporary_unit == 'm')  {
        u8x8.print(F("Metric "));
      }

      else {
        u8x8.print(F("Imperial"));
      }

      if (digitalRead(pin_up) == HIGH)  {           //if up pin is pressed, mode is increased by 1
        mode_settings = mode_settings + 1;

        if (mode_settings == 4) {
          mode_settings = 0;

        }
        delay(200);
        u8x8.clear();
        goto START;
      }
      if (digitalRead(pin_select) == HIGH) {   //if select pin is pressed, we can access and increase the variable weight; this is a loop that repeatedly starts at Start2 again and again

        u8x8.setCursor(8, 0);
        u8x8.print(F("        "));
        u8x8.setCursor(0, 0);
        u8x8.print(F("Unit: "));                                    //line 144 to line 163 makes the selected variable blink
        delay(100);
        unsigned long start_millis_unit = millis();
        delay(100);
        unsigned long start_millis_unit_plus500 = start_millis_unit + 500;       //both start_millis variables are needed for blinking
        delay(300);                                                               //delay is important here

START14:

        if (start_millis_unit <= (millis() - 1000))  {

          start_millis_unit = millis();

          u8x8.setCursor(0, 0);
          u8x8.print(F("Unit: "));
          u8x8.setCursor(8, 0);
          if (temporary_unit == 'm')  {
            u8x8.print(F("Metric "));
          }

          else {
            u8x8.print(F("Imperial"));
          }
        }

        if (start_millis_unit_plus500 <= (millis() - 1000))  {

          start_millis_unit_plus500 = millis();
          //u8x8.clear();
          u8x8.setCursor(0, 0);
          u8x8.print(F("Unit:           "));
        }

        if (digitalRead(pin_up) == HIGH) {          //if up pin is pressed in this instance, then weight is increased (notice the different function of the up pin as compared to before)

          if (temporary_unit == 'm') {
            temporary_unit = 'i';
          }

          else {
            temporary_unit = 'm';
          }

          delay(100);
          u8x8.setCursor(0, 0);
          u8x8.print(F("Unit: "));
          u8x8.setCursor(8, 0);
          if (temporary_unit == 'm')  {
            u8x8.print(F("Metric  "));
          }

          else {
            u8x8.print(F("Imperial"));
          }

          delay(400);
          start_millis_unit = millis();
          start_millis_unit_plus500 = start_millis_unit + 500;
        }

        if (digitalRead(pin_select) == HIGH) {                  //if select pin is pressed, weight will be saved permanently and the loop for increasing the weight is left (-> goto Start)

          EEPROM.write(address_unit, temporary_unit);
          u8x8.clear();
          animation("Saved!         ", 3);
          delay(2000);
          u8x8.clear();
          goto START;
        }

        if (digitalRead(pin_settings) == HIGH) {                //if settings is pressed, we end end the settings loop and go back to void loop() (->goto END_SETTINGS)

          goto END_SETTINGS;
        }

        goto START14;
      }

      if (digitalRead(pin_settings) == HIGH) {

        goto END_SETTINGS;
      }

      break;
  }
  
  delay(100);

  goto START;

END_SETTINGS:
  u8x8.clear();
  animation("Exit settings. ", 3);           //exit settings, go back to void loop()
  delay(2000);
  start_millis_loop = millis();
  start_millis_loop_plus3000 = start_millis_loop + 3000;
  u8x8.clear();
  Serial.print(temporary_height);
  
}


void Running()  {
  //only run this code once when training is started!!! Required for creating a new file on SD card!

  u8x8.clear();
  u8x8.setCursor(0, 0);
  animation(run_started, 3);
  delay(700);
  u8x8.setCursor(0, 5);
  u8x8.print(F("3"));
  delay(1000);
  u8x8.setCursor(2, 5);
  u8x8.print(F("2"));
  delay(1000);
  u8x8.setCursor(4, 5);
  u8x8.print(F("1"));

  nowx = rtc.now();
  delay(50);
  calories = 0;
  float distance = 0;
  float speed_avg = 0;
  float step_length;
  byte steps_min = 0;

  while (Serial.available() != 0)  {
    Serial.parseInt();
  }

  if (EEPROM.read(address_number_of_files) >= 100)  {                               //the following code is for naming the logfiles dynamically
    EEPROM.write(address_number_of_files, 0);
  }

  number_of_files = EEPROM.read(address_number_of_files);
  EEPROM.write(address_number_of_files, number_of_files + 1);
  delay(50);

  if (EEPROM.read(address_gender) == "m")  {
    step_length = float(EEPROM.read(address_height)) * 45.0 / 100.0;
  }
  
  else  {
    step_length = float(EEPROM.read(address_height)) * 43.0 / 100.0;
  }

  char* filename = give_me_filename(number_of_files);
  currentLog = SD.open(filename, FILE_WRITE);
  //the code above is only to name the Log files dynamically
  delay(50);

  currentLog.print("Date:,");                                                         //this code is for writing the header information to the .txt file
  char years[5];
  sprintf(years, "%04u", nowx.year());
  currentLog.print(years);
  currentLog.print("/");
  char months[3];
  sprintf(months, "%02u", nowx.month());
  currentLog.print(months);
  currentLog.print("/");
  char days[3];
  sprintf(days, "%02u", nowx.day());
  currentLog.print(days);
  currentLog.println();

  currentLog.print(F("Time:,"));
  char hours[3];
  sprintf(hours, "%02u", nowx.hour());
  currentLog.print(hours);
  currentLog.print(":");
  char minutes[3];
  sprintf(minutes, "%02u", nowx.minute());
  currentLog.print(minutes);
  currentLog.print(":");
  char seconds[3];
  sprintf(seconds, "%02u", nowx.second());
  currentLog.print(seconds);
  currentLog.println();
  currentLog.flush();

  currentLog.print(F("Height:,"));
  currentLog.println(EEPROM.read(address_height));
  currentLog.print(F("Weight:,"));
  currentLog.println(EEPROM.read(address_weight));
  currentLog.print(F("Gender:,"));
  currentLog.println(char(EEPROM.read(address_gender)));
  currentLog.print(F("Unit:,"));
  currentLog.println(char(EEPROM.read(address_unit)));
  currentLog.println();

  currentLog.print(F("time_stamp, beats_since, steps_since"));
  currentLog.println();
  currentLog.close();
  //end of initialization code of Running()

  delay(650);
  u8x8.clear();

  runnings = true;

  attachInterrupt(digitalPinToInterrupt(pin_ISR_heartRate), heartRate, RISING);       //pin_ISR_heartRate

  unsigned long start_millis_save = millis();

  //Start of displaying to display
START7:                                                                             //the following code will write all our information to display and calculate the variables

  switch (mode_running)  {

    default:
      mode_running = 0;

      break;

    case 0:                                                                                     //case 0 is for displaying time, bpm and steps

      u8x8.clear();

      u8x8.setCursor(0, 3);
      u8x8.print(F("BPM:"));
      u8x8.setCursor(5, 3);
      u8x8.print(bpm);
      u8x8.setCursor(0, 6);
      u8x8.print(F("Steps:"));

START4:

      nowx = rtc.now();
      u8x8.setCursor(0, 0);
      u8x8.print(F("Time:"));
      u8x8.setCursor(0, 1);
      //char hours[3];
      sprintf(hours, "%02u", nowx.hour());
      u8x8.print(hours);
      u8x8.drawString(2, 1, ":");
      u8x8.setCursor(3, 1);
      //char minutes[3];
      sprintf(minutes, "%02u", nowx.minute());
      u8x8.print(minutes);
      u8x8.drawString(5, 1, ":");
      u8x8.setCursor(6, 1);
      //char seconds[3];
      sprintf(seconds, "%02u", nowx.second());
      u8x8.print(seconds);
      u8x8.setCursor(7, 6);
      u8x8.print(steps_total);


      if (digitalRead(pin_start_stop) == HIGH)  {
        goto END_RUNNING;
      }

      if (digitalRead(pin_up) == HIGH) {
        mode_running = mode_running + 1;

        if (mode_running == 3) {
          mode_running = 0;
        }

        delay(500);
        goto START7;
      }

      if (start_millis_save <= (millis() - (every_x_seconds * 1000)))  { //ALPHA
        start_millis_save = millis();
        write_to_SD();
      }

      if (Serial.available() != 0)  {
        Serial.parseInt();
        stepCount();
      }

      goto START4;

      break;

    case 1:                                                                                               //case 1 is for displaying time elapsed, distance and calories

      u8x8.clear();

START5:

      if (EEPROM.read(address_unit) == 'm')  {
        distance = ((float)step_length * (float)steps_total) / 100000.0;
        u8x8.setCursor(12, 3);
        u8x8.print(F("km"));
      }
      
      else  {
        distance = (((float)step_length * (float)steps_total) / 100000.0) / 1.609;
        u8x8.setCursor(12, 3);
        u8x8.print(F("mi"));
      }

      u8x8.setCursor(12, 6);
      u8x8.print(F("kcal"));

      u8x8.setCursor(0, 3);
      u8x8.print(F("Dist.:"));
      u8x8.setCursor(7, 3);
      u8x8.print(distance);

      u8x8.setCursor(0, 6);
      u8x8.print(F("Cal.:"));
      u8x8.setCursor(7, 6);
      u8x8.print(calories);

      nowx = rtc.now();
      time_elapsed = (nowx.unixtime() - time_start);
      u8x8.setCursor(0, 0);
      u8x8.print(F("Time elapsed:"));
      u8x8.setCursor(0, 1);
      u8x8.print(time_elapsed / 60);
      u8x8.setCursor(3, 1);
      u8x8.print(F("min"));

      if (digitalRead(pin_start_stop) == HIGH)  {
        goto END_RUNNING;
      }

      if (digitalRead(pin_up) == HIGH) {


        mode_running = mode_running + 1;

        if (mode_running == 3) {
          mode_running = 0;
        }
        
        delay(500);
        goto START7;
      }

      if (start_millis_save <= (millis() - (every_x_seconds * 1000)))  {                                //write data to SD
        start_millis_save = millis();
        write_to_SD();
      }

      if (Serial.available() != 0)  {
        Serial.parseInt();
        stepCount();
      }

      goto START5;

      break;


    case 2:                                                                                 //case 2 is for displaying avg speed and avg steps per min

      u8x8.clear();

START15:

      nowx = rtc.now();
      time_elapsed = (nowx.unixtime() - time_start);

      if (EEPROM.read(address_unit) == 'm')  {
        distance = ((float)step_length * (float)steps_total) / 100000.0;
        speed_avg = (float(distance) * 3600.0) / (float)time_elapsed;
        u8x8.setCursor(0, 0);
        u8x8.print(F("Avg speed:"));
        u8x8.setCursor(0, 1);
        u8x8.print(speed_avg);
        u8x8.print(F(" km/h"));
      }

      else  {
        distance = (((float)step_length * (float)steps_total) / 100000.0) / 1.609;
        speed_avg = (float(distance) * 3600.0) / (float)time_elapsed / 1.609;
        u8x8.setCursor(0, 0);
        u8x8.print(F("Avg speed:"));
        u8x8.setCursor(0, 1);
        u8x8.print(speed_avg);
        u8x8.print(F(" mph "));
      }

      steps_min = steps_total * 60 / time_elapsed;
      u8x8.setCursor(0, 3);
      u8x8.print(F("Avg steps/min:"));
      u8x8.setCursor(0, 4);
      u8x8.print(steps_min);
      u8x8.print(".00  ");

      if (digitalRead(pin_start_stop) == HIGH)  {
        goto END_RUNNING;
      }

      if (digitalRead(pin_up) == HIGH) {


        mode_running = mode_running + 1;

        if (mode_running == 3) {
          mode_running = 0;
        }
        delay(500);
        goto START7;
      }

      if (start_millis_save <= (millis() - (every_x_seconds * 1000)))  { //ALPHA
        start_millis_save = millis();
        write_to_SD();
      }

      if (Serial.available() != 0)  {
        Serial.parseInt();
        stepCount();
      }

      goto START15;

      break;
  }
//end of displaying to display


END_RUNNING:

  steps_total = 0;                                                                        //exit settings, go back to void loop()
  steps_since = 0;
  beats_since = 0;
  runnings = false;
  u8x8.clear();
  animation("Training ended!", 3);
  delay(2000);
  start_millis_loop = millis();
  start_millis_loop_plus3000 = start_millis_loop + 3000;
  u8x8.clear();

}

void heartRate()  {

  if (runnings == true)  {
    beats_since = beats_since + 1;

    bpm_new = bpm_new + 1;
  }
}

void stepCount()  {

  if (runnings == true) {
    //write steps to display
    steps_since = steps_since + 1;
    steps_total = steps_total + 1;

    if (mode_running == 0) {
      u8x8.setCursor(7, 6);
      u8x8.print(F("   "));
      u8x8.setCursor(7, 6);
      u8x8.print(steps_total);
    }
  }
}

void write_to_SD()  {

  if ((float(steps_since) / float(every_x_seconds)) >= 2.0)  {                                                                    //calculate calories
    calories = calories + ((float(every_x_seconds) / 60.0) * 0.175 * float(EEPROM.read(address_weight)));
  }

  else if ((float(steps_since) / float(every_x_seconds)) < 2.0 && (steps_since / every_x_seconds) > 0.5) {
    calories = calories + ((float(every_x_seconds) / 60.0) * 0.07 * float(EEPROM.read(address_weight)));
  }

  if (runnings == true)  {
    //write beats per minute to OLED  bpm=beats_since*6
    if (mode_running == 0) {

      bpm = (bpm_old + bpm_new) * 5;
      bpm_old = bpm_new;
      bpm_new = 0;

      u8x8.setCursor(5, 3);
      u8x8.print(F("   "));
      u8x8.setCursor(5, 3);
      u8x8.print(bpm);
    }

    char* filename = give_me_filename(number_of_files);
    currentLog = SD.open(filename, FILE_WRITE);

    nowx = rtc.now();
    char hours[3];
    sprintf(hours, "%02u", nowx.hour());
    currentLog.print(hours);
    currentLog.flush();
    currentLog.print(":");
    char minutes[3];
    sprintf(minutes, "%02u", nowx.minute());
    currentLog.print(minutes);
    currentLog.flush();
    currentLog.print(":");
    char seconds[3];
    sprintf(seconds, "%02u", nowx.second());
    currentLog.print(seconds);
    currentLog.print(F(","));
    currentLog.flush();

    currentLog.print(beats_since);
    currentLog.print(F(","));
    currentLog.print(steps_since);
    currentLog.println();
    currentLog.close();

    beats_since = 0;
    steps_since = 0;
  }
}

void animation(char text[16], byte x) {                                                         //Animation for text on display
  for (byte i = 0; i < 16; i++) {
    u8x8.setCursor(i, x);
    u8x8.print(text[i]);
    delay(100);
  }
}
