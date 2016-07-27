/*
	This file is part of Waag Society's BioHack Academy Code.

	Waag Society's BioHack Academy Code is free software: you can 
	redistribute it and/or modify it under the terms of the GNU 
	General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) 
	any later version.

	Waag Society's BioHack Academy Code is distributed in the hope 
	that it will be useful, but WITHOUT ANY WARRANTY; without even 
	the implied warranty of MERCHANTABILITY or FITNESS FOR A 
	PARTICULAR PURPOSE.  See the GNU General Public License for more 
	details.

	You should have received a copy of the GNU General Public License
	along with Waag Society's BioHack Academy Code. If not, see 
	<http://www.gnu.org/licenses/>.


 PLEASE NOTE:

 The thermocycler is perhaps the most complex device in the BioHack
 Academy thus far. The logic of the code is explained in the lecture.
 Please make sure you understand the logic, before changing the code.

 Once again this code is intended to demonstrate the minimal functions
 of a thermocycler. The code is able to run through multiple cycles of
 3 stage (Denaturing, Annealing, Elongation) PCR cycles.
*/

/* *******************************************************
/  Libraries
*/

#include <math.h>   // loads a library with more advanced math functions
#include <Wire.h>   // Needed for I2C connection with LCD screen
#include "LiquidCrystal_I2C.h" // Needed for operating the LCD screen
#include <OneWire.h>// Needed for the temperature sensors
/* *******************************************************
*/

/* *******************************************************
/  LCD
*/
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);
/* *******************************************************
*/

/* *******************************************************
/  Thermocycler Settings
*/
String stageNames[3] = { "Denat", "Anneal", "Elon" }; // Names of Stages
int tempSettings[3] = { 0, 0, 0}; // Temperatures of each stage
int timeSettings[3] = { 0, 0, 0}; // Duration of each stage
int coolSettings[3] = { 1, 0, 0}; // Toggle to enable fan / cooling after stage (only after Denat stage)
int cycleSetting = 0;     // Max number of cycles

// Pins
#define fanPin 5       // The mosfet that drives the 80mm fan is connected to pin 6
#define heatPin 6      // Pin for the mosfet that controls the heating element
#define lidPin 7       // Pin for the mosfet that controls the lid heater

// Temperature read
int val;               // Create an integer variable to temporarily store the thermistor read
double currentTemp;    // Variable to hold the current temperature value
double currentLidTemp; // Variable to hold the current lid temperature value
#define TempPin1 9     // DS18S20 Signal pin on digital 9
#define TempPin2 10    // DS18S20 Signal pin on digital 10

OneWire ds1(TempPin1);
OneWire ds2(TempPin2);

// PCR cycling variables
int stageTemp = 0;      // Target temperature of the current stage
int stageTime = 0;      // Duration of current stage
int cycleCounter = 0;   // Counter of number of cycles completed
int currentState = 0;   // 3 states: Denat, Anneal and Elon
unsigned long currentStageStartTime = 0; // Beginning of the current Stage
int currentStage = 0;   // In each stage, go through 3 states: Ramping, Steady, Cooling
int toggleCooling = 0;  // Toggle to skip or execute Stage 3: Cooling
boolean showtime = false; // Display time on display
int lidTemp = 99;       // Target lid temp

/* *******************************************************
*/

/* *******************************************************
/  Set the initial STATE of the machine
/  In this code we will switch operation modes, from (programming time, to programming temp) x3, to cycling, to stopping/slowing down
*/
#define STATE_DENAT_TIMEPROG 1
#define STATE_DENAT_TEMPPROG 2
#define STATE_ANNEAL_TIMEPROG 3
#define STATE_ANNEAL_TEMPPROG 4
#define STATE_ELON_TIMEPROG 5
#define STATE_ELON_TEMPPROG 6
#define STATE_CYCLESPROG 7
#define STATE_CYCLING 8
#define STATE_STOP 9

byte state = STATE_DENAT_TIMEPROG;
/* *******************************************************
*/

/* *******************************************************
/  Machine User Interface
*/
boolean buttonState = 0;    // Start button
int ledstate = false;       // Blinking indicator LED

// Pins
#define buttonPin 11   // the number of the pushbutton pin
#define ledPin 13      // the number of Arduino's onboard LED pin
/* *******************************************************
*/

/* *******************************************************
/  Rotary Encoder
*/
// These pins can not be changed, because Pin 2 and 3 are special interrupt pins on Arduino UNO. On Leonardo, use 0 and 1
#define encoderPin1 0
#define encoderPin2 1

volatile int lastEncoded = 0;
volatile long encoderValue = 0;

long lastencoderValue = 0;

int lastMSB = 0;
int lastLSB = 0;
/* *******************************************************
*/

/* *******************************************************
/  Variables needed for keeping track of time
*/
uint32_t lastTick = 0;  // Global Clock variable
int LCDTime = 0;        // Time tracker for LCD update

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
 
/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  
/* *******************************************************
*/

/* *******************************************************
/  Setup function, this code is only executed once
*/
void setup() {
  // Update clock
  lastTick = millis();

  // Initialize I2C connection with the LCD screen
  Wire.begin();

  // Open serial connection with the computer and print a message
  Serial.begin(9600);
  Serial.println(F("BioHack Academy Thermocycler"));

  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  
  // rotary encoder as an input
  pinMode(encoderPin1, INPUT); 
  pinMode(encoderPin2, INPUT);
  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on
  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);  

  // fan and heating and set low
  pinMode(fanPin, OUTPUT);
  pinMode(heatPin, OUTPUT);
  pinMode(lidPin, OUTPUT);
  digitalWrite(fanPin, LOW);
  digitalWrite(heatPin, LOW);
  digitalWrite(lidPin, LOW);
  
  // Initialize the LCD and print a message
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("BioHack Academy"));
  lcd.setCursor(0,1);
  lcd.print(F("Thermocycler"));
  delay(1000);
  lcd.clear();
}
/* *******************************************************
*/

/* *******************************************************
/  Thermistor function converts the raw signal into a temperature
/  NOTE: no longer used
*/
double Thermister(int RawADC) {  //Function to perform the fancy math of the Steinhart-Hart equation
  double Temp;
  Temp = log(((10240000/RawADC) - 10000));
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;              // Convert Kelvin to Celsius
  //Temp = (Temp * 9.0)/ 5.0 + 32.0; // Celsius to Fahrenheit - comment out this line if you need Celsius
  return Temp;
}
/* *******************************************************
*/

/* *******************************************************
/  Loop function, this code is constantly repeated
*/
void loop() {
  // Update clock
  uint32_t time = millis();     // current time since start of sketch
  uint16_t dt = time-lastTick;  // difference between current and previous time
  lastTick = time;

  // Read temperature by Thermistor
  //val=analogRead(0);            //Read the analog port 0 and store the value in val
  //currentTemp=Thermister(val);  //Runs the fancy math function on the raw analog value
  
  // Read temperature by digital temp sensor if PCR is running
  if(state == STATE_CYCLING) {
    double tTemp = getTemp1();
    if(tTemp > 1) {
      currentTemp = tTemp;
    }
    tTemp = getTemp2();
    if(tTemp > 1) {
      currentLidTemp = tTemp;
    }
  }
  
  // Print temperature to computer via Serial
  Serial.print("Temperature: ");
  Serial.println(currentTemp);

  // Check whether the button is pressed
  buttonState = digitalRead(buttonPin);
  
  // Blink the LED, indicating that the Arduino is working
  if (ledstate == false) {
    digitalWrite(ledPin, HIGH); // turn LED on
    ledstate = true;
  }
  else {
    digitalWrite(ledPin, LOW); // turn LED off
    ledstate = false;
  }
  
  // Do machine logic
  machineUpdate(dt);
  
  // Reset button state
  buttonState = 0;

  // Wait 200 microsconds before restarting the loop
  delay(200);
}
/* *******************************************************
*/

/* *******************************************************
/  machineUpdate, this function checks in which STATE the device is and executes the code that belongs to that STATE
/  starting with STATEs to allow the user to set the PCR paramters of the device, such as temperate and time of each stage and the number of cycles
/  the next STATE is to execute the PCR protocol
/  final STATE to stop the machine and reset the settings
*/
void machineUpdate(uint16_t dt) {

  // STATE_DENAT_TIMEPROG is the first state in which the user can set the time of the Denat stage should last
  if(state == STATE_DENAT_TIMEPROG) {
    
    // Sanitize the values of the Rotary encoder, no less than 0, no more than 100
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 120) encoderValue = 120;
  
    // Convert encoder value to seconds  
    timeSettings[0] = encoderValue;

    // Display time setting on the LCD
    lcd.setCursor(0,0);
    lcd.print(F("Denat Time"));
    lcd.setCursor(11,0);
    lcd.print(time(timeSettings[0]));    

    // In case the button is pressed, continue to next state
    if(buttonState > 0) {
      stateChange(STATE_DENAT_TEMPPROG);
      encoderValue = 0; // reset encoderValue
    }  
  } 
 
  // STATE_DENAT_TEMPPROG is similar to STATE_DENAT_TIMEPROG, but now the user can set the temperature of the Denat stage
  if(state == STATE_DENAT_TEMPPROG) {
    
    // Sanity check
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 100) encoderValue = 100;
  
    tempSettings[0] = encoderValue;

    // Display the settings on the LCD
    lcd.setCursor(0,1);
    lcd.print(F("Denat Temp"));
    lcd.setCursor(11,1);
    lcd.print(encoderValue);

    // Continue to next state if the button is pressed
    if(buttonState > 0) {
      stateChange(STATE_ANNEAL_TIMEPROG);
      lcd.clear(); // reset LCD screen
      encoderValue = 0; // reset encoderValue
    }     
  } 

  // now the user can set the time of the Anneal stage should last
  if(state == STATE_ANNEAL_TIMEPROG) {
    
    // Sanitize the values of the Rotary encoder, no less than 0, no more than 100
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 120) encoderValue = 120;
  
    // Convert encoder value to seconds  
    timeSettings[1] = encoderValue;

    // Display time setting on the LCD
    lcd.setCursor(0,0);
    lcd.print(F("Anneal Time"));
    lcd.setCursor(11,0);
    lcd.print(time(timeSettings[1]));    

    // In case the button is pressed, continue to next state
    if(buttonState > 0) {
      stateChange(STATE_ANNEAL_TEMPPROG);
      encoderValue = 0; // reset encoderValue
    }  
  } 
 
  // now the user can set the temperature of the Anneal stage
  if(state == STATE_ANNEAL_TEMPPROG) {
    
    // Sanity check
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 100) encoderValue = 100;
  
    tempSettings[1] = encoderValue;

    // Display the settings on the LCD
    lcd.setCursor(0,1);
    lcd.print(F("Anneal Temp"));
    lcd.setCursor(11,1);
    lcd.print(encoderValue);

    // Continue to next state if the button is pressed
    if(buttonState > 0) {
      stateChange(STATE_ELON_TIMEPROG);
      lcd.clear(); // reset LCD screen
      encoderValue = 0; // reset encoderValue
    }     
  } 

  // now the user can set the time of the Elon stage should last
  if(state == STATE_ELON_TIMEPROG) {
    
    // Sanitize the values of the Rotary encoder, no less than 0, no more than 100
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 120) encoderValue = 120;
  
    // Convert encoder value to seconds  
    timeSettings[2] = encoderValue;

    // Display time setting on the LCD
    lcd.setCursor(0,0);
    lcd.print(F("Elon Time"));
    lcd.setCursor(11,0);
    lcd.print(time(timeSettings[2]));    

    // In case the button is pressed, continue to next state
    if(buttonState > 0) {
      stateChange(STATE_ELON_TEMPPROG);
      encoderValue = 0; // reset encoderValue
    }  
  } 
 
  // now the user can set the temperature of the Elon stage
  if(state == STATE_ELON_TEMPPROG) {
    
    // Sanity check
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 100) encoderValue = 100;
  
    tempSettings[2] = encoderValue;

    // Display the settings on the LCD
    lcd.setCursor(0,1);
    lcd.print(F("Elon Temp"));
    lcd.setCursor(11,1);
    lcd.print(encoderValue);

    // Continue to next state if the button is pressed
    if(buttonState > 0) {
      stateChange(STATE_CYCLESPROG);
      lcd.clear(); // reset LCD screen
      encoderValue = 0; // reset encoderValue
    }     
  } 

  // now the user can set the number of PCR cycles
  if(state == STATE_CYCLESPROG) {
    
    // Sanity check
    if(encoderValue < 0) encoderValue = 0;
    if(encoderValue > 100) encoderValue = 100;
  
    cycleSetting = encoderValue;

    // Display the settings on the LCD
    lcd.setCursor(0,1);
    lcd.print(F("Cycles"));
    lcd.setCursor(8,1);
    lcd.print(encoderValue);

    // Continue to next state if the button is pressed
    if(buttonState > 0) {
      stateChange(STATE_CYCLING);
      lcd.clear(); // reset LCD screen
      encoderValue = 0; // reset encoderValue
      currentState = 1; // start at first state ramp up, steady, cool
      currentStage = 0; // start at first stage denat, anneal, elon
      cycleCounter = 1; // start at first cycle
    }     
  } 
 
  // State Cyling is the state in which the thermocycler is running
  if(state == STATE_CYCLING) {

    LCDTime += dt; // Update LCD once every second
    if(LCDTime > 1000) {
      LCDTime = 0;
      
      // Print to LCD
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(F("C "));
      lcd.print(cycleCounter);
      lcd.print("/");
      lcd.print(cycleSetting);
      lcd.print(" ");
      if(currentStage == 0) lcd.print("D ");
      if(currentStage == 1) lcd.print("A ");
      if(currentStage == 2) lcd.print("E ");
      if(showtime) { lcd.print(round((stageTime-(millis()-currentStageStartTime))/1000)); }
      lcd.setCursor(0,1);
      lcd.print(F("Temp "));
      lcd.print(round(currentTemp));
      lcd.print("/");
      lcd.print(stageTemp);
    }
    
    /* Debug info
    Serial.print("stage"); Serial.println(currentStage); // denat, anneal, elon
    Serial.print("state"); Serial.println(currentState); // ramp up, steady, cool
    Serial.print("cycle"); Serial.println(cycleCounter);
    Serial.print("stageTemp"); Serial.println(stageTemp);
    
    Serial.print("tempsettings 0");
    Serial.print(tempSettings[0]);
    Serial.print("tempsettings 1");
    Serial.print(tempSettings[1]);
    Serial.print("tempsettings 2");
    Serial.print(tempSettings[2]);
    Serial.print("timesettings 0");
    Serial.print(timeSettings[0]);
    Serial.print("timesettings 1");
    Serial.print(timeSettings[1]);
    Serial.print("timesettings 2");
    Serial.print(timeSettings[2]);
    */

    if(cycleCounter < cycleSetting) { // Check whether we have not completed all cycles
      // If not, go through 3 PCR stages: Denat, Anneal and Elon
      if(currentStage == 3) {
        currentStage = 0;
        cycleCounter++; // After completion of all three PCR stage, add 1 to the cycleCounter
      }
      stageTemp = tempSettings[currentStage]; // set the PCR stage target temperature
      stageTime = timeSettings[currentStage] * 1000; // set the PCR stage time
      toggleCooling = coolSettings[currentStage]; // set whether the machine needs to cool after completing the PCR stage
    }
    else { // all cycles are done!

      // Print a message to the LCD
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(F("Done! "));
      delay(1000);
      lcd.clear();
      
      stateChange(STATE_STOP);
    }
    
    // Change state if the user presses the button
    if(buttonState > 0) {
      stateChange(STATE_STOP);
      lcd.clear();
    }     
  }

  // StateStop stops the cycling
  if(state == STATE_STOP) {

    // Reset the PCR settings
    tempSettings[0] = 0; // Denat temp
    timeSettings[0] = 0; // Denat time
    tempSettings[1] = 0; // Anneal temp
    timeSettings[1] = 0; // Anneal time
    tempSettings[2] = 0; // Elon temp
    timeSettings[2] = 0; // Elon time
    cycleSetting = 0;    // Max number of cycles

    // PCR cycling variables
    stageTemp = 0;       // Target temperature of the current stage
    stageTime = 0;       // Duration of current stage
    cycleCounter = 0;    // Counter of number of cycles completed
    currentStage = 0;    // Go through 3 stages: Denat, Anneal and Elon
    currentStageStartTime = 0; // Beginning of the current Stage
    currentState = 0;    // In each stage, go through 3 states: Ramping, Steady, Cooling
    toggleCooling = 0;   // Toggle to skip or execute State 3: Cooling

    // Stop heating and fan
    digitalWrite(fanPin, LOW);
    digitalWrite(heatPin, LOW);
    
    // Go back to the first state
    stateChange(STATE_DENAT_TIMEPROG);
  }

/* *******************************************************
/  The actual PCR cycles
/  The code above manages the 3 stages: Denat, Anneal, Elongate
/  Now we need to go through 3 states: Ramping the temperature up, maintain a Steady State, and Cooling if necessary
*/
    
  if(currentState == 1) {
    // RAMPING UP
    if(currentTemp < stageTemp - 10) {
      digitalWrite(heatPin, HIGH);
      digitalWrite(fanPin, LOW);   
      showtime = false;   
    } 
    else if(currentTemp < stageTemp - 5){
      analogWrite(heatPin, 200);
      digitalWrite(fanPin, LOW);
      showtime = false;
    }
    else if(currentTemp < stageTemp - 2){
      analogWrite(heatPin, 100);
      digitalWrite(fanPin, LOW);
      showtime = false;
    }
    else {
      Serial.println(F("Reached Steady State"));
      currentStageStartTime = millis(); // Set timer
      showtime = true;
      currentState = 2; // Continue STEADY STATE stage
    }
  }

  if(currentState == 2) { 
    //STEADY STATE
    if(millis()-currentStageStartTime < stageTime){ // Check whether we completed the state
      if(currentTemp > stageTemp){ // Temperature too high, so switch off heater
        digitalWrite(heatPin, LOW);
      }else{ // Temperature too low, so turn on heater
        digitalWrite(heatPin, HIGH);
      }
    }
    else {
      Serial.print(F("Steady state finished. Temp: ")); 
      Serial.println(currentTemp);
      digitalWrite(heatPin, LOW);
      currentState = 3; // Continue to COOLING stage
    }
  }

  if(currentState == 3) {
    // COOLING
    showtime = false;
    // Set target temp of the next stage
    stageTemp = tempSettings[1];
    
    if(currentTemp > stageTemp && toggleCooling == 1) { // Check whether we need to cool
      Serial.println(F("Cooling down"));
      digitalWrite(fanPin, HIGH);
    }
    else {
      Serial.println(F("currentStage done"));
      digitalWrite(fanPin, LOW);
      
      currentState = 1; // Back to RAMPING UP state
      currentStage ++; // Go from Denat, to Anneal to Elon
    }
  }

  if(currentState > 0) {
    // LID HEATER
    if(currentLidTemp < lidTemp - 10) {
      digitalWrite(lidPin, HIGH); 
    } 
    else if(currentLidTemp < lidTemp - 5){
      analogWrite(lidPin, 200);
    }
    else if(currentLidTemp < lidTemp - 2){
      analogWrite(lidPin, 100);
    }
    else {
      digitalWrite(lidPin, LOW);
    }
  }


/* *******************************************************
*/
  
}
/* *******************************************************
*/

/* *******************************************************
/  stateChange switches the machine logic from one state to another
*/
void stateChange(byte newstate) {
  // set new state
  state = newstate;
  
  // reset button
  buttonState = 0;
}
/* *******************************************************
*/

/* *******************************************************
/  time converts seconds to minutes:seconds format
*/
String time(int val){  
  // calculate number of days, hours, minutes and seconds
  int days = elapsedDays(val);
  int hours = numberOfHours(val);
  int minutes = numberOfMinutes(val);
  int seconds = numberOfSeconds(val);
            
  String returnval = "";
            
  // digital clock display of current time 
  returnval = printDigits(minutes) + ":" + printDigits(seconds) + "   ";
  
  // return value      
  return returnval;
}
/* *******************************************************
*/

/* *******************************************************
/  printDigits adds an extra 0 if the integer is below 10
*/
String printDigits(int digits){
  // utility function for digital clock display: prints colon and leading 0
  String returnval = "";
  if(digits < 10)
    returnval += "0";
  returnval += digits; 
         
  return returnval; 
}
/* *******************************************************
*/

/* *******************************************************
/  updateEncoder is the function that reacts to the rotary encoder interrupts
*/
void updateEncoder(){
  int MSB = digitalRead(encoderPin1); //MSB = most significant bit
  int LSB = digitalRead(encoderPin2); //LSB = least significant bit

  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue --;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue ++;

  lastEncoded = encoded; //store this value for next time
}
/* *******************************************************
*/

/* *******************************************************
/  read the DS18S20 sensor 1
*/
float getTemp1(){
  byte i;
  byte present = 0;
  byte type_s = 2;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds1.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds1.reset_search();
    delay(250);
    //return;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      //return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = ds118S20");  // or old ds11820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = ds118B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = ds11822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a ds11 8x20 family device. DS1");
      //return;
  } 

  if(type_s != 2) {

  ds1.reset();
  ds1.select(addr);
  ds1.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds1.depower() here, but the reset will take care of it.
  
  present = ds1.reset();
  ds1.select(addr);    
  ds1.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds1.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  return celsius;
  }
}

/* *******************************************************
/  read the DS18S20 sensor 2
*/
float getTemp2(){
  byte i;
  byte present = 0;
  byte type_s = 2;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds2.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds2.reset_search();
    delay(250);
    //return;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      //return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = ds218S20");  // or old ds21820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = ds218B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = ds21822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a ds218x20 family device.");
      //return;
  } 

  if(type_s != 2) {
  ds2.reset();
  ds2.select(addr);
  ds2.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds2.depower() here, but the reset will take care of it.
  
  present = ds2.reset();
  ds2.select(addr);    
  ds2.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds2.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
  return celsius;
  }
}
