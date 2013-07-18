#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include "config.h"
#include "WiFlyTwitter.h"

uint8_t button_pressed = 0;
int out = 0;
int temp_value = 0;
uint8_t temperature = 0;
uint16_t humid_result = 0;
uint16_t last_humidity = 0;
uint16_t humid_dry = DRY, humid_wet = WET;
uint8_t water = 0;
uint32_t seconds = 0, next_measurement = 0;
uint16_t eeprom_count = 0;
WiFlyTwitter twitter;
char s[160] = "";
uint8_t count = 0;

//TEMPERATURE TEXTS
const char *TOO_COLD_TEXT = "Mir ist kalt. Es ist sind %u°C. Mach mal die Heizung an!";
const char *TOO_HOT_TEXT = "Mir ist warm. Es ist sind %u°C. Stell mich woanders hin.";
//WATER/HUMIDITY TEXTS
const char *WATER_EMPTY_TEXT = "Hilfe, mein Wasservorrat ist alle.";
const char *WATER_ALMOST_EMPTY_TEXT = "Mein Wasservorrat geht zur Neige.";
const char *WATERED_TEXT = "Ah, das war erfrischend.";
const char *WATER_FROM_HUMAN_TEXT = "Mensch, gib mir Wasser!";
//RANDOM TEXTS
const char *RANDOM_TEXT_1 = "Mir ist langweilig… komm doch mal vorbei und erzähl mir was!";
const char *RANDOM_TEXT_2 = "Guck mal aus dem Fenster. Ich glaub da passiert was.";
const char *RANDOM_TEXT_3 = "Zu spät geguckt.";
const char *RANDOM_TEXT_4 = "Lebst du noch oder du gießt du schon?";

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

void loop()
{
  // Toggle LED every second so that we know the program is not stuck somewhere
  digitalWrite(ONBOARD_LED, seconds & 1 ? HIGH : LOW);

  if (seconds >= next_measurement) {
    next_measurement += MEASURE_INTERVAL;
    Serial.print("Measuring... ");

    measure_humidity();
    give_water();
    was_watered();
    check_water();
    Serial.println(humid_result);

    measure_temperature();
    Serial.println(temperature);
    twitter_temperature();

    // output
    //Serial.println(voltage);
    if(DEBUG)
    {
      humidity_to_eeprom();
      humidity_to_twitter();
    }
  }

  if (button_pressed)
  {
    print_or_reset_eeprom();
  }

  // Power down the microcontroller.
  // Will wake up every second due to Timer1 compare match.
  //asm("sleep\n");
  //delay(100);
  sleep_mode();
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
void measure_humidity()
{
  uint16_t humid_value_1, humid_value_2;
  setSensorPolarity(true);
  delay(FLIP_TIMER);
  humid_value_1 = analogRead(HUMID_IN);
  delay(FLIP_TIMER);
  setSensorPolarity(false);
  delay(FLIP_TIMER);
  humid_value_2 = 1023 - analogRead(HUMID_IN);
  last_humidity = humid_result;
  humid_result = (humid_value_1 + humid_value_2) / 2;
}

void give_water()
{
  if(humid_result > DRY - DELTA_HUMID){ // THIS IS WATAAAAAAAAA!
    if(!is_water_middle())
    {
      if (ONLINE) Serial.println(twitter.post(WATER_FROM_HUMAN_TEXT));
    }
    else
    {
      digitalWrite(WATER_VALVE, HIGH);
      delay(3000);
      digitalWrite(WATER_VALVE, LOW);
    }
  }
}

void was_watered()
{
  if(last_humidity > humid_result && last_humidity - humid_result > DELTA_HUMID){ // plant was most likely watered
    humid_dry = last_humidity;
    humid_wet = humid_result;
    if (ONLINE) Serial.println(twitter.post(WATERED_TEXT));
  }
}
uint8_t water_fill_level()
{
  uint16_t water;
  water = analogRead(FILL_IN);
  water -= 1023*4.5/5.0; // 920-1023 --> 0-83
  return (uint8_t)water;
}

boolean is_water_high() {
  return (water >= 55);
}

boolean is_water_middle() {
  return (water >= 30);
}

void check_water()
{
  water = water_fill_level();
  if(!is_water_high() && is_water_middle())
  {
    if (ONLINE) Serial.println(twitter.post(WATER_ALMOST_EMPTY_TEXT));
  }
  else if(!is_water_high() && !is_water_middle())
  {
    if (ONLINE) Serial.println(twitter.post(WATER_EMPTY_TEXT));
  }
}

void measure_temperature()
{
    // temperature sensing
    uint16_t temp_value = analogRead(TEMP_IN);
    temperature =  (uint8_t) ((temp_value * 50) / 1023);
}

void twitter_temperature()
{
  if(temperature <= TOO_COLD){
    post_temperature(TOO_COLD_TEXT);
  }
  else if(temperature >= TOO_HOT){
    post_temperature(TOO_HOT_TEXT);
  }
}

void post_temperature(const char *text)
{
  char msg[141];
  snprintf(msg, 141, text, temperature);
  msg[140] = '\0';  // snprintf doesn't write \0 if string is too long.
  if (ONLINE) Serial.println(twitter.post(msg));
  if (DEBUG) Serial.println(msg);
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

void humidity_to_eeprom()
{
  // Store humidity values (scaled to 8 bit) in EEPROM
  eeprom_count = eeprom_read_uint16(EEPROM_COUNTER_ADDRESS);
  if (eeprom_count < EEPROM_COUNTER_ADDRESS)
  {
    EEPROM.write(eeprom_count, (uint8_t) (humid_result >> 2));
    eeprom_count++;
    eeprom_write_uint16(EEPROM_COUNTER_ADDRESS, eeprom_count);
  }
}

void humidity_to_twitter()
{
  sprintf(s+strlen(s), "%d ", humid_result);
  count++;
  if(count == 24)
  {
    if (ONLINE) Serial.println(twitter.post(s));
    count = 0;
    *s = '\0';
  }
}

void print_or_reset_eeprom()
{
  button_pressed = 0;
  if(DEBUG) read_out_eeprom();

  uint8_t i = 0, steps = 200;
  for (i = 0; i < steps; i++)
  {
    if (digitalRead(BUTTON_PIN) == HIGH) break; // Escape when button is released.
    delay(10);
  }
  if (i == steps && DEBUG) // The button was pressed during the whole loop.
  {
    Serial.println("Button was held for more than 2 seconds, will now erase EEPROM.");
    eeprom_write_uint16(EEPROM_COUNTER_ADDRESS, 0);
  }
}
