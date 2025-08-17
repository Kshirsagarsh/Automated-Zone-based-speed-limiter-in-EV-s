//Library
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "SPI.h"
#define Buzzer 4
#define LED 5

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin assignments
const int motorPin1 = 7; // IN1
const int motorPin2 = 8; // IN2
const int enablePin = 6; // ENA (PWM)
const int incButtonPin = 3; // Increase speed button
const int decButtonPin = 2; // Decrease speed button

// Motor speed variables
int motorSpeed = 0;  // Initial speed (0 to 255)
int prevMotorSpeed = -1; // To track changes in speed
int zone = 0; // Current zone status (0 = No zone, 1 = School zone)
int prevZone = -1;  // To track changes in zone
const int speedIncrement = 2;  // Speed step increment
const int speedLimit = 60;    // Speed limit when in the school zone

int ReceivedMessage[1] = {0}; // Used to store value received by the NRF24L01
int lastReceivedMessage = -1;  // To track the last received message state
RF24 radio(9, 10); // NRF24L01 SPI pins. Pin 9 and 10 on the Nano
const uint64_t pipe = 0xE6E6E6E6E6E6; // Needs to be the same for communicating between 2 NRF24L01

// For debouncing the buttons
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // 50ms debounce time

// Timing for gradual speed reduction
unsigned long lastSpeedAdjustmentTime = 0;
const unsigned long speedAdjustmentDelay = 100;  // Adjust speed every 100ms

// Buzzer control variables
bool buzzerTriggered = false;  // To track if the buzzer has been triggered
unsigned long buzzerStartTime = 0;
const unsigned long buzzerDuration = 500;  // Buzzer beep duration (500ms)

// Timing for zone timeout
unsigned long lastMessageTime = 0;  // To track the last received message time
const unsigned long zoneTimeout = 3000; // 3 seconds timeout for zone

// Timing for speed adjustment delay
unsigned long zoneEntryTime = 0;   // To track when we entered the zone
bool canAdjustSpeed = false;        // Flag to indicate if we can start adjusting speed

void setup()
{
  // Initialize the LCD
  lcd.init();
  lcd.backlight();

  // Set motor control pins as output
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(enablePin, OUTPUT);

  // Set button pins as input with pull-up resistors
  pinMode(incButtonPin, INPUT_PULLUP);
  pinMode(decButtonPin, INPUT_PULLUP);

  // Set motor direction (forward)
  digitalWrite(motorPin1, HIGH);
  digitalWrite(motorPin2, LOW);

  // Display initial speed on the LCD
  updateLCD();

  // NRF24L01 receiver setup
  radio.begin(); // Start the NRF24L01
  radio.openReadingPipe(1, pipe); // Get NRF24L01 ready to receive
  radio.startListening(); // Listen to see if information received
  pinMode(Buzzer, OUTPUT);
  pinMode(LED, OUTPUT);
}

void loop()
{
  // Button debouncing and handling
  if (digitalRead(incButtonPin) == LOW && (millis() - lastDebounceTime) > debounceDelay)
  {
    lastDebounceTime = millis();  // Reset debounce timer
    increaseSpeed();  // Increase motor speed, but only if not limited by the zone
  }

  if (digitalRead(decButtonPin) == LOW && (millis() - lastDebounceTime) > debounceDelay)
  {
    lastDebounceTime = millis();  // Reset debounce timer
    decreaseSpeed();  // Decrease motor speed
  }

  // Set the motor speed using PWM
  analogWrite(enablePin, motorSpeed);

  // NRF24L01 radio receiver logic
  if (radio.available())
  {
    radio.read(ReceivedMessage, 1);  // Read information from the NRF24L01

    if (ReceivedMessage[0] == 111 && lastReceivedMessage != 111)
    { 
      // Only trigger if the message is received for the first time (state change)
      digitalWrite(Buzzer, HIGH);   // Turn on the buzzer
      digitalWrite(LED, HIGH);      // Turn on the LED
      buzzerTriggered = true;       // Mark buzzer as triggered
      buzzerStartTime = millis();   // Record the time when the buzzer was triggered
      zone = 1; // School zone
      lastReceivedMessage = 111;    // Update the last received message
      lastMessageTime = millis();    // Reset the last message time
      zoneEntryTime = millis();      // Record the time when entering the zone
      canAdjustSpeed = false;        // Reset the adjustment flag
    } 
    else if (ReceivedMessage[0] != 111 && lastReceivedMessage == 111)
    {
      // Turn off the buzzer and LED if the message changes
      digitalWrite(LED, LOW);       // Turn off the LED
      zone = 0; // No zone
      lastReceivedMessage = ReceivedMessage[0];  // Update the last received message
      canAdjustSpeed = false;        // Reset the adjustment flag when leaving the zone
    }

    // Update the last message time if a valid message was received
    lastMessageTime = millis();
  } 
  else
  {
    // If no message is received, check for timeout
    if (millis() - lastMessageTime >= zoneTimeout && zone == 1)
    {
      // If in the school zone and timeout has occurred, turn off the zone
      zone = 0; // No zone
      digitalWrite(LED, LOW);  // Turn off LED
      lastReceivedMessage = -1; // Reset last received message
      canAdjustSpeed = false;    // Reset the adjustment flag
    }
  }

  // Check if we can start adjusting speed
  if (zone == 1)
  {
    // Check if 2 seconds have passed since entering the zone
    if (millis() - zoneEntryTime >= 5000)
    {
      canAdjustSpeed = true; // Allow speed adjustment
    }
  }

  // Gradually decrease speed to 100 if it's higher and allowed
  if (canAdjustSpeed && motorSpeed > speedLimit && (millis() - lastSpeedAdjustmentTime) > speedAdjustmentDelay)
  {
    motorSpeed -= speedIncrement;  // Reduce speed gradually
    if (motorSpeed < speedLimit)
    {
      motorSpeed = speedLimit;  // Ensure it doesn't go below the limit
    }
    lastSpeedAdjustmentTime = millis();  // Reset timing for next adjustment
  }

  // Check if buzzer needs to be turned off after the duration
  if (buzzerTriggered && (millis() - buzzerStartTime) >= buzzerDuration)
  {
    digitalWrite(Buzzer, LOW);     // Turn off the buzzer
    buzzerTriggered = false;       // Reset the trigger
  }

  // Only update the LCD if there is a change in motorSpeed or zone
  if (motorSpeed != prevMotorSpeed || zone != prevZone)
  {
    updateLCD();
    prevMotorSpeed = motorSpeed; // Update the previous motor speed
    prevZone = zone;             // Update the previous zone
  }
}

// Function to increase motor speed
void increaseSpeed()
{
  // Allow increasing speed up to 100 km/h in the school zone
  if (zone == 1 && motorSpeed >= speedLimit)
  {
    return; // Prevent speed increase if it's already 100 or more in the school zone
  }
  
  if (motorSpeed < 254)
  {
    motorSpeed += speedIncrement;
  }
}

// Function to decrease motor speed
void decreaseSpeed()
{
  if (motorSpeed > 0)
  {
    motorSpeed -= speedIncrement;
  }
}

// Function to update the LCD with the current speed and zone status
void updateLCD()
{
  lcd.clear(); // Clear only once when updating
  lcd.setCursor(0, 0);
  lcd.print("Speed:");
  lcd.setCursor(6, 0);
  lcd.print(motorSpeed/2);
  lcd.setCursor(11, 0);
  lcd.print("kmph");
  lcd.setCursor(0, 1); 
  if (zone == 1)
  {
    lcd.print("School Zone ");
  }
  else
  {
    lcd.print("No Zone");
  }
}
