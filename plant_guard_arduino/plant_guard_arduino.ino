
#define TEMP_IN A0

void setup() {
  Serial.begin(115200);
}

void loop(){
  int temp_value = analogRead(TEMP_IN);
  //TODO avoid float
  float voltage = (temp_value * 5.0 * 10.0) / 1023.0;
  delay(1000);
  Serial.println(voltage);
}
