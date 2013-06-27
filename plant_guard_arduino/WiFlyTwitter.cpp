#include <WiFlyHQ.h>
#include "WiFlyTwitter.h"
#include "config.h"
#include <SoftwareSerial.h>
uint8_t parseStatus;
int statusCode;
WiFly wiFly;
SoftwareSerial wiFlySerial(FROM_WIFLY_PIN, TO_WIFLY_PIN); //RX PIN3, TX PIN2

    WiFlyTwitter::WiFlyTwitter(){

    }
    WiFlyTwitter::~WiFlyTwitter(){

    }
void WiFlyTwitter::setupWiFly(){
  Serial.println("Setting up WiFly...");

  //char buf[32];
  wiFlySerial.begin(9600);
  if(!wiFly.begin(&wiFlySerial, &Serial)){
    Serial.println("Failed to start wiFly"); 
  }

  wiFly.setJoin(0); // Disable auto join
  wiFly.leave();
  
  //if(!wiFly.isAssociated()){
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
 // }
}

bool WiFlyTwitter::checkStatus(Print *debug)
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

int WiFlyTwitter::wait(Print *debug)
{
  while (checkStatus(debug));
  return statusCode;
}

int WiFlyTwitter::post(const char *msg){
  int status = NULL;
  if(postToTwitter(msg)){
    status = wait(NULL);
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
    status = 303;
  }
  return status;
}

bool WiFlyTwitter::postToTwitter(const char *msg){
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
