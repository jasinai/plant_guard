#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include "config.h"
#include "WiFlyTwitter.h"

#define TEMP_IN A0
#define HUMID_IN A1
#define FILL_IN A2

#define VOLTAGE_FLIP_1 4
#define VOLTAGE_FLIP_2 5
#define WATER_VALVE 7
#define BUTTON_PIN 2
#define FLIP_TIMER 1000
#define ONBOARD_LED 13
#define EEPROM_COUNTER_ADDRESS 1022

// Arbitrary constants for thresholds, adjust as necessary
#define DRY 100
#define WET 900
#define DELTA_HUMID 50
#define TOO_COLD 5
#define TOO_HOT 30


//#define MEASURE_INTERVAL (10) // in seconds
#define MEASURE_INTERVAL (60*10) // in seconds

#define ONLINE false

short watered = 0;
uint8_t button_pressed = 0;
int out = 0;
int temp_value = 0;
uint8_t temperature = 0;
uint16_t humid_result = 0;
uint32_t seconds = 0, next_measurement = 0;
uint16_t eeprom_count = 0;
WiFlyTwitter twitter;
char s[160] = "";
uint8_t count = 0;
uint16_t humid_dry = DRY;
uint16_t humid_wet = WET;
uint16_t last_humidity = 0;

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

  Serial.begin(115200);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(VOLTAGE_FLIP_1, OUTPUT);
  pinMode(VOLTAGE_FLIP_2, OUTPUT);
  pinMode(HUMID_IN, INPUT);
  pinMode(WATER_VALVE, OUTPUT);
  digitalWrite(WATER_VALVE, LOW);

  // Button for accessing the data
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(10);
  EIMSK |= (1 << INT0);     // Enable external interrupt INT0
  EICRA |= (1 << ISC01);    // Trigger INT0 on falling edge
  EIFR = (1 << INTF0);      // Reset Interrupt flag
  sei(); // library function for interrrupt enabling

  if (ONLINE) twitter.setupWiFly();
  //if (ONLINE) twitter.post("I'm alive");

  set_sleep_mode(SLEEP_MODE_IDLE);

  // Take a measurement as soon the loop starts
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

uint8_t water_fill_level()
{
  uint16_t water;
  water = analogRead(FILL_IN);
  water -= 1023*4.5/5.0; // 920-1023 --> 0-83
  return (uint8_t)water;
}

boolean is_water_high(uint8_t water) {
  return (water >= 55);
}

boolean is_water_middle(uint8_t water) {
  return (water >= 30);
}


float measure_temperature()
{
    // temperature sensing
    uint16_t temp_value = analogRead(TEMP_IN);
    //TODO avoid float?
    return (uint8_t) (temp_value * 5.0 * 10.0) / 1023.0;
}

void read_out_eeprom() {
  uint16_t count = eeprom_read_uint16(EEPROM_COUNTER_ADDRESS);
  Serial.print("EEPROM contains ");
  Serial.print(count);
  Serial.println(" values:");
  for (uint16_t i = 0; i < count; i++) {
    Serial.println(EEPROM.read(i));
  }
}

void eeprom_write_uint16(uint16_t address, uint16_t value) {
  EEPROM.write(address, value & 0xFF); // Low byte
  EEPROM.write(address + 1, (value >> 8) & 0xFF); // High byte
}

uint16_t eeprom_read_uint16(uint16_t address) {
  return (EEPROM.read(address)) | (EEPROM.read(address + 1) << 8);
}


ISR(INT0_vect) // BUTTON_PIN is low (trigger on low because it was set to HIGH before)
{
  button_pressed = 1;
}

// Timer1 compare match interrupt. Will be called every second.
ISR(TIMER1_COMPA_vect)
{
  seconds++;
}

void loop()
{
  // Toggle LED every second so that we know the program is not stuck somewhere
  digitalWrite(ONBOARD_LED, seconds & 1 ? HIGH : LOW);

  if (seconds >= next_measurement) {
    next_measurement += MEASURE_INTERVAL;
    Serial.print("Measuring... ");

    last_humidity = humid_result;
    humid_result = measure_humidity();
    if(humid_result > DRY - DELTA_HUMID){ // THIS IS WATAAAAAAAAA!
      digitalWrite(WATER_VALVE, HIGH);
      delay(3000);
      digitalWrite(WATER_VALVE, LOW);
      //watered++;
    }
    
    if(last_humidity > humid_result && last_humidity - humid_result > DELTA_HUMID){ // plant was most likely watered
      humid_dry = last_humidity;
      humid_wet = humid_result;
    }
    
    temperature = measure_temperature();
    char msg[141];
    if(temperature <= TOO_COLD){
      snprintf(msg, 141, "Mir ist kalt! Stell mich an einen wärmeren Ort. Aktuelle Temperatur: %u", temperature);
      msg[140] = '\0';  // snprintf doesn't write \0 if string is too long.
      if (ONLINE) Serial.println(twitter.post(msg));
    }
    else if(temperature >= TOO_HOT){
      snprintf(msg, 141, "Puh, ist das warm! Ein bisschen Schatten wäre nicht schlecht. Aktuelle Temperatur: %u", temperature);
      msg[140] = '\0';  // snprintf doesn't write \0 if string is too long.
      if (ONLINE) Serial.println(twitter.post(msg));
    }
    Serial.println(humid_result);
    // output
    //Serial.println(voltage);

    // Store humidity values (scaled to 8 bit) in EEPROM
    eeprom_count = eeprom_read_uint16(EEPROM_COUNTER_ADDRESS);
    if (eeprom_count < EEPROM_COUNTER_ADDRESS)
    {
      EEPROM.write(eeprom_count, (uint8_t) (humid_result >> 2));
      eeprom_count++;
      eeprom_write_uint16(EEPROM_COUNTER_ADDRESS, eeprom_count);
    }

    sprintf(s+strlen(s), "%d ", humid_result);
    count++;
    if(count == 24)
    {
      if (ONLINE) Serial.println(twitter.post(s));
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
    Serial.println("Watered! Tweeting...");
    if (ONLINE) Serial.println(twitter.post("Test"));
    //Serial.println(watered);
    watered = 0;
  }

  if (button_pressed)
  {
    button_pressed = 0;
    read_out_eeprom();

    uint8_t i = 0, steps = 200;
    for (i = 0; i < steps; i++)
    {
      if (digitalRead(BUTTON_PIN) == HIGH) break; // Escape when button is released.
      delay(10);
    }
    if (i == steps) // The button was pressed during the whole loop.
    {
      Serial.println("Button was held for more than 2 seconds, will now erase EEPROM.");
      eeprom_write_uint16(EEPROM_COUNTER_ADDRESS, 0);
    }
  }

  // Power down the microcontroller.
  // Will wake up every second due to Timer1 compare match.
  //asm("sleep\n");
  //delay(100);
  sleep_mode();
}
