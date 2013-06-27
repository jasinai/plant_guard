#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include "config.h"
#include "WiFlyTwitter.h"

#define TEMP_IN A0
#define HUMID_IN A1
#define VOLTAGE_FLIP_1 4
#define VOLTAGE_FLIP_2 5
#define WATER_BUTTON 2
#define FLIP_TIMER 1000
#define ONBOARD_LED 13

//#define MEASURE_INTERVAL (10) // in seconds
#define MEASURE_INTERVAL (60*15) // in seconds

short watered = 0;
int out = 0;
int temp_value = 0;
float voltage = 0;
uint16_t humid_result = 0;
uint32_t seconds = 0, next_measurement = 0;
uint16_t eeprom_count = 0;
WiFlyTwitter twitter;
char s[160] = "";
uint8_t count = 0;
uint_16t humid_dry = DRY;
uint_16t humid_wet = WET;
uint_16t last_humidity = 0;


void setup() {
  // Set up timer for counting seconds
  TCCR1A = 0;
  TCCR1B = 0;
  // WGM1 <= 0100 (Clear timer when Value == OCR1A)
  TCCR1B |= (1 << WGM12);
  // CS1 <= 101 (Timer Frequency = clk_IO / 1024)
  TCCR1B |= (1 << CS12) | (1 << CS10);
  // Set max value for timer. 15625 * 1024 == 16.000.000 (clk_IO)
  OCR1A = 15625;
  // Enable Timer1 output compare A interrupt
  TIMSK1 |= (1 << OCIE1A);
  pinMode(ONBOARD_LED, OUTPUT);

  Serial.begin(115200);
  pinMode(VOLTAGE_FLIP_1, OUTPUT);
  pinMode(VOLTAGE_FLIP_2, OUTPUT);
  pinMode(HUMID_IN, INPUT);
  twitter.setupWiFly();
  pinMode(WATER_BUTTON, INPUT);
  digitalWrite(WATER_BUTTON, HIGH); // if low, interrupt function is called
  delay(10);
  EIMSK |= (1 << INT0);     // Enable external interrupt INT0
  EICRA |= (1 << ISC01);    // Trigger INT0 on falling edge
  EIFR = (1 << INTF0);      // Reset Interrupt flag
  sei(); // library function for interrrupt enabling
  //attachInterrupt(0, waterButton, FALLING);
  //twitter.post("I'm alive");

  // Sleep, mode = "idle" (000) und Sleep Enable = 1
  SMCR = 1;

  // Reset values that are stored in EEPROM by setting the value counter to 0.
  if (false)
  {
    EEPROM.write(1022, 0);
    EEPROM.write(1023, 0);
  }

  // Number of values is stored as 16 bit (little endian) value in EEPROM
  eeprom_count = (EEPROM.read(1022)) | (EEPROM.read(1023) << 8);

  // Read out all values stored in EEPROM
  if (true) {
    delay(1000);
    Serial.print("EEPROM contains ");
    Serial.print(eeprom_count);
    Serial.println(" values:");
    for (uint16_t i = 0; i < eeprom_count; i++) {
      Serial.print(EEPROM.read(i));
      Serial.print(" ");
    }
    Serial.println();
  }

  next_measurement = seconds;
}

void setSensorPolarity(boolean flip){
  if(flip){
    digitalWrite(VOLTAGE_FLIP_1, HIGH);
    digitalWrite(VOLTAGE_FLIP_2, LOW);
  } else {
    digitalWrite(VOLTAGE_FLIP_1, LOW);
    digitalWrite(VOLTAGE_FLIP_2, HIGH);
  }
}

// humidity sensing
uint16_t measure_humidity()
{
  uint16_t humid_value_1, humid_value_2;
  setSensorPolarity(true);
  delay(FLIP_TIMER);
  humid_value_1 = analogRead(HUMID_IN);
  delay(FLIP_TIMER);
  setSensorPolarity(false);
  delay(FLIP_TIMER);
  humid_value_2 = 1023 - analogRead(HUMID_IN);
  return (humid_value_1 + humid_value_2) / 2;
}

float measure_temperature()
{
    // temperature sensing
    uint16_t temp_value = analogRead(TEMP_IN);
    //TODO avoid float?
    return (temp_value * 5.0 * 10.0) / 1023.0;
}


ISR(INT0_vect) // WATER_BUTTON is low (trigger on low because it was set to HIGH before)
{
  watered++;
}

// Timer1 compare match interrupt. Will be called every second.
ISR(TIMER1_COMPA_vect)
{
  seconds++;
  digitalWrite(ONBOARD_LED, seconds & 1 ? HIGH : LOW);
}

void loop()
{
  if (seconds >= next_measurement) {
    next_measurement += MEASURE_INTERVAL;
    Serial.print("Measuring... ");

    last_humidity = humid_result;
    humid_result = measure_humidity();
    if(last_humidity > humid_result && last_humidity - humid_result > DELTA_HUMIDITY){ // plant was most likely watered
      humid_dry = last_humidity;
      humid_wet = humid_result;
    }
    //voltage = measure_temperature();
    Serial.println(humid_result);
    // output
    //Serial.println(voltage);

    // Scale 10 bit value to 8 bits, save in EEPROM and increase counter.
    if (eeprom_count < 1022)
    {
      EEPROM.write(eeprom_count, (uint8_t) (humid_result / 4));
      eeprom_count++;
      EEPROM.write(1022, eeprom_count & 0xFF); // Low byte
      EEPROM.write(1023, (eeprom_count >> 8) & 0xFF); // High byte
    }

    sprintf(s+strlen(s), "%d ", humid_result);
    count++;
    if(count == 24)
    {
      Serial.println(twitter.post(s));
      count = 0;
      *s = '\0';
    }
    //unsigned long tmp = 1000UL * 60UL * 15UL - (millis() - timestamp);
    //timestamp = millis();
    //Serial.println(tmp);
    //delay(tmp);
  }

  if(watered)
  {
    Serial.print("Watered! Tweeting... ");
    Serial.println(twitter.post("Test"));
    //Serial.println(watered);
    watered = 0;
  }

  // Power down the microcontroller.
  // Will wake up every second due to Timer1 compare match.
  //asm("sleep\n");
  delay(100);
}
