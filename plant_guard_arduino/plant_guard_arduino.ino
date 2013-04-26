
#define TEMP_IN A0
#define HUMID_IN A1
#define VOLTAGE_FLIP_1 2
#define VOLTAGE_FLIP_2 3

#define FLIP_TIMER 1000

int out = 0;
int temp_value = 0;
int humid_value_1 = 0;
int humid_value_2 = 0;
int humid_result = 0;

void setup() {
  Serial.begin(115200);
  pinMode(VOLTAGE_FLIP_1, OUTPUT);
  pinMode(VOLTAGE_FLIP_2, OUTPUT);
  pinMode(HUMID_IN, INPUT);
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

void loop(){
  // humidity sensing
  setSensorPolarity(true);
  delay(FLIP_TIMER);  
  humid_value_1 = analogRead(HUMID_IN);
  delay(FLIP_TIMER);
  setSensorPolarity(false);
  delay(FLIP_TIMER);
  humid_value_2 = 1023 - analogRead(HUMID_IN);
  humid_result = (humid_value_1 + humid_value_2) / 2;
  
  // temperature sensing
  temp_value = analogRead(TEMP_IN);
  //TODO avoid float?
  float voltage = (temp_value * 5.0 * 10.0) / 1023.0;
  
  // output
  //Serial.println(voltage);
  Serial.println(humid_result);
  //delay(1000);
}
