
#define TEMP_IN A0
#define HUMID_IN A1

int out = 0;
int temp_value = 0;
int humid_value = 0;

void setup() {
  Serial.begin(115200);
  //pinMode(HUMID_OUT, OUTPUT);
  //pinMode(HUMID_IN, INPUT);
  //digitalWrite(HUMID_OUT, out);
}

void loop(){
  // humidity sensing
  humid_value = analogRead(HUMID_IN);
  
  // temperature sensing
  temp_value = analogRead(TEMP_IN);
  //TODO avoid float?
  float voltage = (temp_value * 5.0 * 10.0) / 1023.0;
  
  // output
  //Serial.println(voltage);
  Serial.println(humid_value);
  delay(1000);
}
