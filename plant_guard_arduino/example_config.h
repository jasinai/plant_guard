//WIFLY CONFIG
#define SSID "SSID"
#define PASSPHRASE "PASSPHRASE"
#define TO_WIFLY_PIN 8 //TX
#define FROM_WIFLY_PIN 9 //RX

//TWITTER CONFIG
#define TWITTER_TOKEN "TWITTERTOKEN"
#define TWITTER_DELAY 60000
#define LIB_DOMAIN "arduino-tweet.appspot.com"

//INITIAL PLANT CONFIG
#define DRY 300
#define WET 100
#define DELTA_HUMID 20
#define TOO_COLD 10 //degree celsius
#define TOO_HOT 30 //degree celsius

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

//#define MEASURE_INTERVAL (10) // in seconds
#define MEASURE_INTERVAL (60*10) // in seconds

#define ONLINE false
#define DEBUG false