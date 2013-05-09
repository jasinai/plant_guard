#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFlyHQ.h>

//WIFLY CONFIG
#define SSID "Nerdonia"
#define PASSPHRASE "Ich hab grad keine Zeit."
#define TO_WIFLY_PIN 8
#define FROM_WIFLY_PIN 9
SoftwareSerial wiFlySerial(FROM_WIFLY_PIN, TO_WIFLY_PIN);

//TWITTER CONFIG
#define TWITTER_TOKEN "1379533332-F021XjPx93iLVRGkcdSvb1kcsjKE4FSzwy8pQNw"
#define TWITTER_DELAY 60000
#define LIB_DOMAIN "arduino-tweet.appspot.com"
uint8_t parseStatus;
int statusCode;

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
WiFly wiFly;

void setup() {
  Serial.begin(115200);
  pinMode(VOLTAGE_FLIP_1, OUTPUT);
  pinMode(VOLTAGE_FLIP_2, OUTPUT);
  pinMode(HUMID_IN, INPUT);
  setupWiFly();
  post("I'm alive");
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

void setupWiFly(){
  char buf[32];
  wiFlySerial.begin(9600);
  if(!wiFly.begin(&wiFlySerial, &Serial)){
    Serial.println("Failed to start wiFly"); 
  }
  
  if(!wiFly.isAssociated()){
    Serial.println("Joining network ");
    wiFly.setSSID(SSID);
    wiFly.setPassphrase(PASSPHRASE);
    wiFly.enableDHCP();
    if(wiFly.join()){
      Serial.println("Joined the network ");
    }
    else{
      Serial.println("Failed to join the network");
    }
  }
}

void post(const char *msg){
  if(postToTwitter(msg)){
    int status = wait(NULL);
    if(status == 200){
      Serial.println("Yay");
    }
    else{
      Serial.println("Message could not be sent, error code:");
      Serial.println(status);
    }
  }
  else {
    Serial.println("Connection failed");
  }
}

bool postToTwitter(const char *msg){
  if(wiFly.isConnected()){
    wiFly.close();
  }
  
  if(wiFly.open(LIB_DOMAIN, 80)) {
    Serial.println("Connected");
    parseStatus = 0;
    statusCode = 0;
    wiFly.println("POST http://" LIB_DOMAIN "/update HTTP/1.0");
    wiFly.print("Content-Length: ");
    wiFly.println(strlen(msg)+strlen(TWITTER_TOKEN)+14);
    wiFly.println();
    wiFly.print("token=");
    wiFly.print(TWITTER_TOKEN);
    wiFly.print("&status=");
    wiFly.println(msg);
    return true;
  } else{
    return false;
  }
}


bool checkStatus(Print *debug)
{
  if (!wiFly.isConnected()) {
    wiFly.flush();
    //wiFly.stop();
    return false;
  }
  if (!wiFly.available())
    return true;
  char c = wiFly.read();
  switch(parseStatus) {
  case 0:
    if (c == ' ') parseStatus++; break;  // skip "HTTP/1.1 "
  case 1:
    if (c >= '0' && c <= '9') {
      statusCode *= 10;
      statusCode += c - '0';
    } else {
      parseStatus++;
    }
  }
  return true;
}

int wait(Print *debug)
{
  while (checkStatus(debug));
  return statusCode;
}
