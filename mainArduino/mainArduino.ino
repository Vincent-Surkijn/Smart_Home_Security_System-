/*****LCD screen*****/
//D0,D1  are used for serial comm!
#include <LiquidCrystal.h>
LiquidCrystal lcd(8,9,4,5,6,7);
int buttons = A0; // Analog pin for the state of the buttons
String screenState = "Home"; // The state of the current shown screen

/*****Bluetooth*****/
#include <SoftwareSerial.h>
SoftwareSerial BTserial(3, 11); // Setup of Bluetooth module on pins 3 (TXD) and 11 (RXD);

/*****Photoresistor*****/
int photoresistor = A1;  // Analog pin for the photoresistor
int LEDpin = A3; // Digital pin for the "lights"
int LEDbrightness;
int photocellReading;

/*****Temperature sensor*****/
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9
OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire);  // Pass our oneWire reference to Dallas Temperature.
DeviceAddress insideThermometer, outsideThermometer;  // arrays to hold device addresses
float temperature;
int heating = A5;  // Pin for the "heating"

/*****Distance/Motion sensor*****/
int echoPin =  12; // Digital pin for to pin Echo of the Distance/Motion sensor
int trigPin = 13; // Digital pin for to pin Trig of the Distance/Motion sensor

/*****Door lock*****/
int lock = A4; // Digital pin to lock the door

/*****Buzzer*****/
int buzzer = A2;  // Pin for the buzzer
int i = 1;
int b = 0;
int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};

/*****Settings*****/
float desiredTemperature = 22;  // Default temperature setting
float initTemp;  // To check if settings changed
int initLight; // To check if settings changed
int lightThreshold = 20;  // Default value for lights to turn on and curtains to close
int initDistance; // Check initial distance
int btnHoldCounter = 1; // To count lenght of button hold when alarm is ringing
int code = 123; // Code to turn off alarm
unsigned long seconds;  // To be able to send the temperature and light every second
unsigned long screenUpdate; // Only update screen every 500ms
boolean alarmEnabled = false;  // State of the alarm
boolean selected = false; // To use when changing settings
boolean doorLocked = false; // State of the door lock
boolean alarmRinging = false; // To alert when an intruder is detected


//float value1;
//float value2;
//int thermoresistor = 0;

/*****Setup & Loop*****/
void setup() {
  BTserial.begin(9600); // Bluetooth at baud 9600 for talking to the node server
  Serial.begin(4800); // Default Serial on Baud 4800 for printing out some messages in the Serial Monitor
  while(!Serial || !BTserial);  // Wait for communications to be running

  // Start up the temperature sensor library
  sensors.begin();
  // locate devices on the bus
  Serial.println("Locating temp sensor devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // Must be called before search()
  oneWire.reset_search();
  // assigns the first address found to insideThermometer
  if (!oneWire.search(insideThermometer)) Serial.println("Unable to find address for insideThermometer");
  // set the resolution to 9 bit per device
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  
  // Setup the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Starting...");

  // Set pinmodes
  pinMode(LEDpin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(lock, OUTPUT);
  pinMode(heating, OUTPUT);

  // Read initial sensor values
  // Update light value and turn on light if necessary
  lightHandler();
  // Update temperature value and turn on heating if necessary
  temperatureHandler();

  seconds = millis();
  screenUpdate = seconds;
}

void loop() {
  // Handle incoming bluetooth messages
  bluetoothHandler();
  
  // Update every 500ms
  if(millis()-screenUpdate>=500){
    //Serial.println("Updating screen");
    // Make alarm go off
    if(alarmRinging){
      //Serial.println("TUTATUTATUTA");
      // Sound buzzer alarm
      soundAlarm();

      //Detect switching off of alarm via buttons
      int buttonState = analogRead(buttons);
      // No button pressed
      if(buttonState>=900){
          btnHoldCounter = 1;
      }
      // Select button pressed
      if(buttonState>=700 && buttonState<900){
        //Serial.print("Holding select");
        //Serial.println(btnHoldCounter);
        btnHoldCounter++;
        // When button has been held for 10s(20*500ms) the alarm can be turned off
        if(btnHoldCounter>=20){
          alarmRinging = false;
          alarmEnabled = false;
          noTone(buzzer);
          btnHoldCounter = 1;
          BTserial.println("Alarm:disabled");
        }        
      }
    }
    // Only show screens when alarm is not going off
    else{
      buttonHandler();
    }
    screenUpdate = millis();
  }

  // Check motion
  if(alarmEnabled && !alarmRinging){
    int distance = measureDistance();
    if(((initDistance+0.3*initDistance)<=distance||(initDistance-0.3*initDistance)>=distance)){
      alarmRinging = true;
      // Display message on the LCD screen
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("MOTION DETECTED");
      lcd.setCursor(0,1);
      lcd.print("ALARM!");
    }
  }

  // Lock the door
  if(doorLocked){
    digitalWrite(lock,HIGH);  
  }
  else{
    digitalWrite(lock,LOW);   
  }

  // Every 5s write values to web server
  if(millis()-seconds>=5000){
    
    // Update light value and turn on light if necessary
    lightHandler();
    // Update temperature value and turn on heating if necessary
    temperatureHandler();
    
    //Serial.println("Sending values over bluetooth");
    BTserial.print("Light:");
    BTserial.println(photocellReading);
    // Insert a small delay(50ms) between both messages for the webserver to be able to seperate them
    delay(50); 
    BTserial.print("Temperature:");
    BTserial.println(temperature);
    seconds = millis();
  }

  //Debug purposes
  //Serial.println(screenState);
  /*if(BTserial.available()){
    Serial.println("ble:");
    String msg = BTserial.readStringUntil('\n');
    Serial.println(msg);    
  }*/
  //BTserial.println("test");
}


/*****Functions*****/
void temperatureHandler(){
  // Send the command to get temperatures
  sensors.requestTemperatures();
  temperature = sensors.getTempC(insideThermometer);
  if(temperature == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }

  // Check if heating needs to be turned on
  // Using desiredTemperature+-1 prevents constant switching on and off of heating when the temperature is around the threshold value (Hysteresis)
  if(temperature>=desiredTemperature+1){
    // If the temperature is 1°C above the desired temperature the heating is turned off
    digitalWrite(heating, LOW);  
  }
  else if(temperature<=desiredTemperature-1){
    // If the temperature is 1°C less than the desired temperature the heating is turned on
    digitalWrite(heating, HIGH);
  }
}

void soundAlarm(){
  tone(buzzer,tones[i]);
  if(i>=9){
    b = 1;
  }  
  else if(i<=0){
    b = 0; 
  }

  if(b==1){
    i--;
  }
  else{
    i++;  
  }  
}

void lightHandler(){
  photocellReading = analogRead(photoresistor);
  /*BTserial.print("Light: ");
  BTserial.print(photocellReading);
  BTserial.print("\n");*/

  //Serial.print("Light: ");
  //Serial.println(photocellReading);
  // Using lightThreshold+-5 prevents constant switching on and off of lights when the light is around the threshold value (Hysteresis)
  if (photocellReading >= lightThreshold+5){
    digitalWrite(LEDpin, LOW);  //Turn led off
  }
  else if(photocellReading <= lightThreshold-5){
    digitalWrite(LEDpin, HIGH); //Turn led on
  }
}

void photoresistorAutomaticFade(){
  photocellReading = analogRead(photoresistor);  
  Serial.print("Light: ");
  Serial.println(photocellReading);
  
  // LED gets brighter the darker it is at the sensor
  // that means we have to -invert- the reading from 0-1023 back to 1023-0
  int inverted = 1023 - photocellReading;
  //now we have to map 0-1023 to 0-255 since thats the range analogWrite uses
  LEDbrightness = map(inverted, 0, 1023, 0, 255);
  analogWrite(LEDpin, LEDbrightness);
}

int measureDistance(){
  // Clear the trigPin condition
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Set the trigPin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Read the echoPin, return the sound wave travel time in microseconds
  long duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  int distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)

  /*Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");*/

  return distance;
}

void enableAlarm(){
  // Check initial distance when alarm was enabled
  initDistance = measureDistance();
  // Enable alarm
  alarmEnabled = true;
  // Lock door as well
  doorLocked = true;
  // Let the web server know the door is being locked as well
  BTserial.println("Door:yes");
  
  //Serial.print("iDistWhen on: ");
  //Serial.println(initDistance);
}

void buttonHandler(){
  // Clear LCD screen before writing new value
  lcd.clear();
  //check button state
  int buttonState = analogRead(buttons);
  //Serial.println(button);
  
  if(buttonState>=900){  //no btn pressed
    //Serial.println("No button selected");
    if(screenState == "Home"){
      // Print the home screen with the current sensor values
      lcd.setCursor(0,0);
      lcd.print("Temperature:");
      lcd.print(String(temperature, 1));
      lcd.setCursor(0,1);
      lcd.print("Light:");
      lcd.print(photocellReading);
    }
    else if(screenState == "Settings1"){
      // Print the current temperature setting
      lcd.setCursor(0,0);
      lcd.print("Settings");
      lcd.setCursor(0,1);
      lcd.print("Temperature:");
      lcd.print(String(desiredTemperature, 1));
      // Make setting blink when selected
      if(selected){
        delay(250);
        lcd.clear();
        lcd.print("Settings");
        lcd.setCursor(0,1);
        lcd.print("Temperature:");
      }
    }
    else if(screenState == "Settings2"){
      // Print the current light setting
      lcd.setCursor(0,0);
      lcd.print("Settings");
      lcd.setCursor(0,1);
      lcd.print("Light:");
      lcd.print(lightThreshold);
      // Make setting blink when selected
      if(selected){
        delay(250);
        lcd.clear();
        lcd.print("Settings");
        lcd.setCursor(0,1);
        lcd.print("Light:");
      }
    }
    else if(screenState == "Settings3"){
      // Print the current alarm setting
      lcd.setCursor(0,0);
      if(!selected){
        lcd.print("Settings");
        lcd.setCursor(0,1);
        lcd.print("Alarm is ");
        if(alarmEnabled) lcd.print("on");
        else        lcd.print("off");
      }
      // Ask confirmation
      else{
        lcd.setCursor(0,0);
        lcd.print("Settings");
        lcd.setCursor(0,1);
        lcd.print("Turn alarm ");
        if(alarmEnabled){
          lcd.print("off?");
        }
        else{
          lcd.print("on?");
        }
      }
    }
    else if(screenState == "Settings4"){
      // Print the current door lock setting
      lcd.setCursor(0,0);
      if(!selected){
        lcd.print("Settings");
        lcd.setCursor(0,1);
        lcd.print("Door is ");
        if(doorLocked) lcd.print("locked");
        else        lcd.print("open");
      }
      // Ask confirmation
      else{
        lcd.setCursor(0,0);
        lcd.print("Settings");
        lcd.setCursor(0,1);
        if(doorLocked){
          lcd.print("Open door?");
        }
        else{
          lcd.print("Lock door?");
        }
      }
    }
  }
  else if(buttonState>=700){ //select btn pressed
    //Serial.println("select btn pressed");
    if(screenState == "Home"){
      // Switch to settings if pressed while on the home screen
      screenState = "Settings1";
      // Print the current temperature setting
      lcd.setCursor(0,0);
      lcd.print("Settings");
      lcd.setCursor(0,1);
      lcd.print("Temperature:");
      lcd.print(String(desiredTemperature, 1));
    }
    else if(screenState == "Settings1" || screenState == "Settings2" || screenState == "Settings3" || screenState == "Settings4"){
      if(selected){
        // Desired temperature changed
        if(screenState == "Settings1" && initTemp != desiredTemperature){
          // Update webserver
          BTserial.print("SetTemp:");
          BTserial.println(desiredTemperature);
        } 
        // Light threshold changed
        if(screenState == "Settings2" && initLight != lightThreshold){
          // Update webserver
          BTserial.print("SetLight:");
          BTserial.println(lightThreshold);
        } 
      }
      else{
        initTemp = desiredTemperature;  // To check if settings changed
        initLight = lightThreshold;   // To check if settings changed 
      }
      selected = !selected;
    }
  }
  else if(buttonState>=500){ //left btn pressed
    //Serial.println("left btn pressed");
    if(screenState == "Settings1" || screenState == "Settings2" || screenState == "Settings3" || screenState == "Settings4"){
      if(selected){
        // Desired temperature changed
        if(screenState == "Settings1" && initTemp != desiredTemperature){
          // Update webserver
          BTserial.print("SetTemp:");
          BTserial.println(desiredTemperature);
        } 
        // Light threshold changed
        if(screenState == "Settings2" && initLight != lightThreshold){
          // Update webserver
          BTserial.print("SetLight:");
          BTserial.println(lightThreshold);
        } 
      }
      // Go back to the home screen
      screenState = "Home";
      // Turn off the selection
      selected = false;
    }
  }
  else if(buttonState>=300){ //down btn pressed
    //Serial.println("down btn pressed");
    if(screenState == "Settings1" && selected && desiredTemperature>=5){
      desiredTemperature--;
    }
    else if(screenState == "Settings1" && !selected){
      // Go to next Settings screen
      screenState = "Settings2";
    }
    else if(screenState == "Settings2" && selected && lightThreshold>=0){
      lightThreshold--;
    }
    else if(screenState == "Settings2" && !selected){
      // Go to next Settings screen
      screenState = "Settings3";
    }
    else if(screenState == "Settings3" && !selected){
      // Go to next Settings screen
      screenState = "Settings4";
    }
    else if(screenState == "Settings4" && !selected){
      // Go to first Settings screen
      screenState = "Settings1";
    }
  }
  else if(buttonState>=100){ //up btn pressed
    //Serial.println("up btn pressed");
    if(screenState == "Settings1" && selected && desiredTemperature<=40){
      desiredTemperature++;
    }
    else if(screenState == "Settings1" && !selected){
      // Go to last Settings screen
      screenState = "Settings4";
    }
    else if(screenState == "Settings2" && selected && lightThreshold<=100){
      lightThreshold++;
    }
    else if(screenState == "Settings2" && !selected){
      // Go to previous Settings screen
      screenState = "Settings1";
    }
    else if(screenState == "Settings3" && selected){
      //TODO: turn on alarm after 30s(millis+30000)
      // Change alarm state
      alarmEnabled = !alarmEnabled;
      // Print confirmation
      lcd.print("Settings");
      lcd.setCursor(0,1);
      lcd.print("Alarm now ");
      if(alarmEnabled) lcd.print("on");
      else        lcd.print("off");
      // Turn off selection
      selected = false;
      // Return to home screen
      screenState = "Home";
      // Send update to web server
      BTserial.print("Alarm:");
      if(alarmEnabled){
        BTserial.println("yes");
        enableAlarm();
      }
      else                BTserial.println("no");
    }
    else if(screenState == "Settings3" && !selected){
      // Go to previous Settings screen
      screenState = "Settings2";
    }
    else if(screenState == "Settings4" && selected){
      // Change door lock state
      doorLocked = !doorLocked;
      // Print confirmation
      lcd.print("Settings");
      lcd.setCursor(0,1);
      lcd.print("Door now ");
      if(doorLocked) lcd.print("locked");
      else        lcd.print("open");
      // Turn off selection
      selected = false;
      // Return to home screen
      screenState = "Home";
      // Send update to web server
      BTserial.print("Door:");
      if(doorLocked)      BTserial.println("yes");
      else                BTserial.println("no");
    }
    else if(screenState == "Settings4" && !selected){
      // Go to previous Settings screen
      screenState = "Settings3";
    }
  }
  else{ //right btn pressed
    //Serial.println("right btn pressed");
  }  
}

void bluetoothHandler(){
  // Only read when there's a message available
  if(BTserial.available()){
    String msg = BTserial.readStringUntil('\n');
    Serial.println(msg);
    // Check password
    String psswrd = "turnAlarmOff:";
    psswrd.concat(code);
    // If right message and password(123) are displayed the alarm is turned off
    if(msg.indexOf(psswrd)>=0 && alarmRinging){
      Serial.println("Turning off alarm");
      // Stop the ringing of the alarm
      alarmRinging = false;
      noTone(buzzer);
      // Also disable the alarm
      alarmEnabled = false;
      BTserial.println("Alarm:disabled");
    }
    // Read and update new settings values sent by web server
    else if(msg.indexOf("Values:")>=0){       //Values:door_status;alarm_status;light_value;temp_value
      //Read door status
      int idx = msg.indexOf(":");
      int idx2 = msg.indexOf(";");
      String doorStatus = msg.substring(idx+1,idx2);
      //Serial.print("door is:");
      //Serial.println(doorStatus);
      
      //Read alarm status
      msg = msg.substring(idx2+1);
      idx = msg.indexOf(";");
      String alarmStatus = msg.substring(0,idx);
      //Serial.print("alarm is:");
      //Serial.println(alarmStatus);
      
      // Read light value setting
      msg = msg.substring(idx+1);
      idx = msg.indexOf(";");
      String light = msg.substring(0,idx);
      //Serial.print("light is:");
      //Serial.println(light);
      
      // Read temperature value setting
      msg = msg.substring(idx+1);
      idx = msg.indexOf(";");
      String temperature = msg.substring(0,idx);
      //Serial.print("temp is:");
      //Serial.println(temperature);

      // Update values
      // Update door status
      if(doorStatus.indexOf("yes")>=0)  doorLocked = true;
      else if(doorStatus.indexOf("no")>=0)  doorLocked = false;
      else  Serial.println("Error: invalid door status");
      
      // Update alarm status
      //TODO: what if alarm is ringing and then disabled?
      if(alarmStatus.indexOf("yes")>=0)  enableAlarm();
      else if(alarmStatus.indexOf("no")>=0){
        alarmEnabled = false;
        // Turn alarm off as well if it was ringing
        alarmRinging = false;
        noTone(buzzer);        
      }
      else  Serial.println("Error: invalid alarm status");
      
      // Update light threshold
      int tempL = light.toInt();
      if(0<=tempL && tempL<=100)  lightThreshold = tempL;
      else  Serial.println("Error: invalid light value");
      
      // Update desired temperature
      float temp = temperature.toFloat();
      if(5<=temp && temp<=40)  desiredTemperature = temp;
      else  Serial.println("Error: invalid temperature value");
    }
    // Enable scheduled alarm
    else if(msg.indexOf("AutoAlarm")>=0){
      // Enable alarm
      enableAlarm();
    }
    // Trigger alarm because of noise detection
    else if(msg.indexOf("IntruderAlert")>=0 && alarmEnabled){
      // Trigger alarm
      alarmRinging = true;
      // Display message on the LCD screen
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("NOISE DETECTED");
      lcd.setCursor(0,1);
      lcd.print("ALARM!");
    }
    else{
      Serial.println("Bluetooth message error");  
    }
  }
}
