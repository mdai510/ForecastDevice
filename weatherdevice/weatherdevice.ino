#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ESP32Time.h>

//for display
#include "SPI.h"
#include "TFT_eSPI.h"
// #include <LiquidCrystal.h>
//LCD pins
// const int rs = 12, en = 14, d4 = 5, d5 = 4, d6 = 2, d7 = 18;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
// Stock font and GFXFF reference handle
#define GFXFF 1
#define FF18 &FreeSans12pt7b

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

//network information
const char* ssid = "NETGEAR75";
const char* password = "vastviolin434";
IPAddress ipv6;

//IP API
String ipAPICall = "http://ip-api.com/json/";
String ipAPIFields = "?fields=country,countryCode,regionName,city,lat,lon";

//Location information from IP API 
const char* country;
const char* ccode;
const char* reg_name;
const char* city;
double lat;
double lon;

//Weather API
String weatherAPIKey = "ccf6be088f6bb5825ed68cf25da130ce";
String weatherAPICall = "https://api.openweathermap.org/data/3.0/onecall?lat=";

// time vars
const char* ntpServer = "pool.ntp.org";
const long daylightOffset = 0;
ESP32Time rtc;
int tmzOffset;

// weather info struct
struct weather_var{
  long dt;
  float temp; //in Kelvin
  int humidity;
  float wind_speed;
  const char* description;
  const char* icon;
};
struct weather_var cur_weather;
struct weather_var hourly_weather[24];

// loop variables
// weather call variables
unsigned long weather_lastcall = 0; //millis() of last API call
int delay_mins = 60;
unsigned long weather_delay_mils = delay_mins * 60 * 1000; //delay between weather api calls
// display loop variables
unsigned long prev_millis_disp = 0;
int disp_state = 0;
int num_disp_states = 3;
bool did_display = false; 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wifi_disconnected() {
  Serial.println("WiFi Disconnected...");
  WiFi.begin(ssid, password);
  delay(1000);
}

//Gets location information from IPv6 Address
String ip_location() {
  ipv6 = WiFi.globalIPv6();
  Serial.print("WiFi IPv6 Address: ");
  Serial.println(ipv6);

  String ipv6Str = ipv6.toString();
  ipAPICall = ipAPICall + "" + ipv6Str + "" + ipAPIFields;
  //call api
  HTTPClient http;
  http.begin(ipAPICall);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Returned Payload: ");
    Serial.println(payload);
    return payload;
  } 
  else{
    Serial.println("Error on HTTP Request.");
    return "";
  }
  http.end();
}

//Parses json information from IP API 
void parse_ip_json(String location_json){
  JsonDocument doc;
  deserializeJson(doc, location_json);
  country = doc["country"];
  ccode = doc["countryCode"];
  reg_name = doc["regionName"];
  city = doc["city"];
  lat = doc["lat"];
  lon = doc["lon"];
}

//calls location functions
void get_location(){
  //Use IPLocation API to get location
  String location_json = ip_location();
  while (location_json == "") {
    location_json = ip_location();
  }
  //parse json
  parse_ip_json(location_json);
}

//gets weather information from weather API and sets variables accordingly
int get_weather(){
  String weatherAPIFullCall = weatherAPICall + "" + lat + "&lon=" + lon + "&units=imperial&exclude=minutely,alerts&appid=" + weatherAPIKey;
  String weather_json;

  HTTPClient http;
  http.begin(weatherAPIFullCall);
  int httpCode = http.GET();
  if(httpCode > 0){
    weather_json = http.getString();
    Serial.print("Returned Payload: ");
    Serial.println(weather_json);
  }
  else{
    Serial.println("Error on HTTP Request.");
    return 0;
  }

  JsonDocument doc;
  deserializeJson(doc, weather_json);
  tmzOffset = doc["timezone_offset"];
  //get CURRENT weather
  cur_weather.dt = doc["current"]["dt"];
  cur_weather.temp = doc["current"]["temp"];
  cur_weather.humidity = doc["current"]["humidity"];
  cur_weather.wind_speed = doc["current"]["wind_speed"];
  cur_weather.description = doc["current"]["weather"][0]["description"];
  cur_weather.icon = doc["current"]["weather"][0]["icon"];

  //get weather for next few hours
  JsonArray hrly_data= doc["hourly"];

  for(int i = 0; i < 24; i++){
    hourly_weather[i].dt = hrly_data[i]["dt"];
    hourly_weather[i].temp = hrly_data[i]["temp"];
    hourly_weather[i].humidity = hrly_data[i]["humidity"];
    hourly_weather[i].wind_speed = hrly_data[i]["wind_speed"];
    hourly_weather[i].description = hrly_data[i]["weather"][0]["description"];
    hourly_weather[i].icon = hrly_data[i]["weather"][0]["icon"];
  }
  return 1;
}

//connect to wifi
void wifiConnect(){
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {}
  WiFi.enableIPv6();
  Serial.println("Connected to WiFi.");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  //setup tft
  tft.begin();
  tft.setRotation(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.setFreeFont(FF18);       // Select the font

  Serial.begin(115200);

  tft.drawString("Starting...", 0, 30, GFXFF);
  delay(1000);

  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.drawString("Connecting to WiFi...", 0, 30, GFXFF);
  //Connect to WiFi
  wifiConnect();

  //get delay until IPv6 is valid (it takes some time)
  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.drawString("Obtaining IPv6...", 0, 30, GFXFF);
  while (WiFi.globalIPv6().toString() == "::") {
    Serial.println("Obtaining IPv6...");
    delay(500);
  }

  get_location();
  get_weather();
  weather_lastcall = millis();

  //this is dumb lol
  ESP32Time rtc1(tmzOffset);
  rtc = rtc1;
  rtc.setTime(cur_weather.dt);
  WiFi.disconnect();
}

//continuously display weather/location info 
//update weather each hour
void loop() {  
  unsigned long cur_millis = millis();
  unsigned long del_interval;

  //add code to check if wifi and ip are connected

  //runs once every x time specified in weather_delay_mils
  //note: millis() overflow is not an issue because both millis and weather_lastcall are unsigned longs
  if(cur_millis - weather_lastcall >= weather_delay_mils){
    //call weather API
    wifiConnect();
    get_weather();
    WiFi.disconnect();
    weather_lastcall = cur_millis;
  }

  if(!did_display){ //ensures content is only displayed one time per interval to prevent flickering
    did_display = true; 
    //switch cases enable cycling between different things to display and different time intervals for each display
    switch(disp_state){
      case 0:
        del_interval = 2000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString(rtc.getTime("%a %Y-%m-%d %H:%M"), 0, 0, GFXFF);
        tft.drawString("Current Temp: " + (String)cur_weather.temp + "F", 0, 30, GFXFF);
        tft.drawString((String)cur_weather.description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)cur_weather.humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)cur_weather.wind_speed + "m/s", 0, 120, GFXFF);
        break;
      case 1:
        del_interval = 1000;
        //future weather
        tft.fillScreen(TFT_BLUE);   
        break;
      case 2:
        del_interval = 1000;
        //future weather
        tft.fillScreen(TFT_RED);  
        break;
    }
  }
  
  if(cur_millis - prev_millis_disp >= del_interval){
    prev_millis_disp = cur_millis;
    disp_state = (disp_state + 1) % num_disp_states;
    did_display = false;
  }

}