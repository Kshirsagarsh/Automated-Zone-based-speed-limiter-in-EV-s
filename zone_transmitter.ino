#include "nRF24L01.h"
#include "RF24.h"
#include "SPI.h"
#define Switch 2
#define LED_PIN 3
int SentMessage[1] = {000};
RF24 radio(9, 10);
const uint64_t pipe = 0xE6E6E6E6E6E6;
void setup()
{
  pinMode(Switch, INPUT_PULLUP);
  digitalWrite(Switch, HIGH);
  radio.begin();                       // Start the NRF24L01
  radio.openWritingPipe(pipe);         // Get NRF24L01 ready to transmit
  pinMode(LED_PIN, OUTPUT);
}
void loop()
{
  if (digitalRead(Switch) == LOW)      // If switch is ON
  {
    SentMessage[0] = 111;
    radio.write(SentMessage, 1);       // Send pressed data to NRF24L01
    digitalWrite(LED_PIN, HIGH); 
  }
  else
  {
    SentMessage[0] = 000;
    radio.write(SentMessage, 1);       // Send idle data to NRF24L01
    digitalWrite(LED_PIN, LOW);
  }
}