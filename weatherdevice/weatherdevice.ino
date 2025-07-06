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

// Stock font and GFXFF reference handle
#define GFXFF 1
#define FF18 &FreeSans12pt7b

#define BUTTON1_PIN 13
#define BUTTON2_PIN 12
#define BUTTON3_PIN 14

//track which button is pressed
int button_state = 1; 

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

//network information
const char* ssid = "NETGEAR75";
const char* password = "vastviolin434";
IPAddress ipv6;

//IP API
String ip_API_call = "http://ip-api.com/json/";
String ip_API_fields = "?fields=country,countryCode,regionName,city,lat,lon";
//Location information from IP API 
const char* country;
const char* ccode;
const char* reg_name;
const char* city;
double lat;
double lon;

//Weather API
String weather_API_key = "ccf6be088f6bb5825ed68cf25da130ce";
String weather_API_call = "https://api.openweathermap.org/data/3.0/onecall?lat=";

// time vars
ESP32Time rtc;
int tmz_offset;

// weather info struct
struct weather_var{
  long dt;
  const char* summary;  //only for daily
  float temp;           //in Kelvin; max temp for daily
  float min_temp;       //only for daily
  int humidity;
  float wind_speed;
  const char* description;
  const char* icon;
};
struct weather_var cur_weather;
struct weather_var hourly_weather[24];
struct weather_var daily_weather[7];

// loop variables
// weather call variables
unsigned long weather_lastcall = 0; //millis() of last API call
int delay_mins = 60;
unsigned long weather_delay_mils = delay_mins * 60 * 1000; //delay between weather api calls
// display loop variables
unsigned long prev_millis_disp = 0;
int disp_state = 0;
int num_disp_states;
bool did_display = false; 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Gets location information from IPv6 Address
String ipLocation() {
  ipv6 = WiFi.globalIPv6();
  Serial.print("WiFi IPv6 Address: ");
  Serial.println(ipv6);

  String ipv6_str = ipv6.toString();
  ip_API_call = ip_API_call + "" + ipv6_str + "" + ip_API_fields;
  //call api
  HTTPClient http;
  http.begin(ip_API_call);
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
void parseIP_JSON(String location_json){
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
void getLocation(){
  //Use IPLocation API to get location
  String location_json = ipLocation();
  while (location_json == "") {
    location_json = ipLocation();
  }
  //parse json
  parseIP_JSON(location_json);
}

//gets weather information from weather API and sets variables accordingly
int getWeather(){
  String weatherAPIFullCall = weather_API_call + "" + lat + "&lon=" + lon + "&units=imperial&exclude=minutely,alerts&appid=" + weather_API_key;
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
  tmz_offset = doc["timezone_offset"];
  //get CURRENT weather
  cur_weather.dt = doc["current"]["dt"];
  cur_weather.temp = doc["current"]["temp"];
  cur_weather.humidity = doc["current"]["humidity"];
  cur_weather.wind_speed = doc["current"]["wind_speed"];
  cur_weather.description = doc["current"]["weather"][0]["description"];
  cur_weather.icon = doc["current"]["weather"][0]["icon"];

  //get weather for next few hours
  JsonArray hrly_data = doc["hourly"];
  for(int i = 0; i < 24; i++){
    hourly_weather[i].dt = hrly_data[i]["dt"];
    hourly_weather[i].temp = hrly_data[i]["temp"];
    hourly_weather[i].humidity = hrly_data[i]["humidity"];
    hourly_weather[i].wind_speed = hrly_data[i]["wind_speed"];
    hourly_weather[i].description = hrly_data[i]["weather"][0]["description"];
    hourly_weather[i].icon = hrly_data[i]["weather"][0]["icon"];
  }

  JsonArray daily_data = doc["daily"];
  for(int i = 0; i < 7; i++){
    daily_weather[i].dt = daily_data[i+1]["dt"];
    daily_weather[i].summary = daily_data[i+1]["summary"];
    daily_weather[i].temp = daily_data[i+1]["temp"]["max"];
    daily_weather[i].min_temp = daily_data[i+1]["temp"]["min"];
    daily_weather[i].humidity = daily_data[i+1]["humidity"];
    daily_weather[i].wind_speed = daily_data[i+1]["wind_speed"];
    daily_weather[i].description = daily_data[i+1]["weather"][0]["description"];
    daily_weather[i].icon = daily_data[i+1]["weather"][0]["icon"];
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

String getFutureTime(int future_hrs, bool disp_time){
  ESP32Time temprtc = rtc;
  temprtc.offset = tmz_offset + future_hrs*3600;
  if(disp_time) return temprtc.getTime("%a %Y-%m-%d %H:%M");
  else return temprtc.getTime("%a %Y-%m-%d");
  
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  //setup buttons
  pinMode(BUTTON1_PIN, INPUT);
  pinMode(BUTTON2_PIN, INPUT);
  pinMode(BUTTON3_PIN, INPUT);

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
  Serial.println("Obtaining IPv6...");
  while (WiFi.globalIPv6().toString() == "::") {}

  getLocation();
  getWeather();
  weather_lastcall = millis();

  //this is dumb lol... but works
  ESP32Time rtc1(tmz_offset);
  rtc = rtc1;
  rtc.setTime(cur_weather.dt);
  WiFi.disconnect();
}

//continuously display weather/location info 
//update weather each hour
void loop() {  
  unsigned long cur_millis = millis();
  unsigned long del_interval;

  //runs once every x time specified in weather_delay_mils
  //note: millis() overflow is not an issue because both millis and weather_lastcall are unsigned longs
  if(cur_millis - weather_lastcall >= weather_delay_mils){
    //call weather API
    wifiConnect();
    getWeather();
    WiFi.disconnect();
    weather_lastcall = cur_millis;
  }

  if(digitalRead(BUTTON1_PIN)){
    button_state = 1;
    did_display = false;
    Serial.println("BUTTON 1 PRESS");
  }
  else if(digitalRead(BUTTON2_PIN)){
    button_state = 2;
    num_disp_states = 3;
    did_display = false;
    disp_state = 0;
    Serial.println("BUTTON 2 PRESS");
  }
  else if(digitalRead(BUTTON3_PIN)){
    button_state = 3;
    num_disp_states = 2;
    did_display = false;
    disp_state = 0;
    Serial.println("BUTTON 3 PRESS");
  }
  else{}

  //TODO: display current, hourly, and daily weather based on button pressed

  if(button_state == 1 && !did_display){
    did_display = true;
    del_interval = 60*1000;
    tft.fillScreen(TFT_BLACK);   // Clear screen
    tft.drawString(rtc.getTime("%a %Y-%m-%d %H:%M"), 0, 0, GFXFF);
    tft.drawString("Current Temp: " + (String)cur_weather.temp + "F", 0, 30, GFXFF);
    tft.drawString((String)cur_weather.description, 0, 60, GFXFF);
    tft.drawString("Humidity " + (String)cur_weather.humidity + "%", 0, 90, GFXFF);
    tft.drawString("Wind Speed: " + (String)cur_weather.wind_speed + "m/s", 0, 120, GFXFF);
  }
  else if(button_state == 2 && !did_display){ //ensures content is only displayed one time per interval to prevent flickering
    did_display = true; 
    //switch cases enable cycling between different things to display and different time intervals for each display
    switch(disp_state){ 
      case 0:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString(getFutureTime(1, true), 0, 0, GFXFF);
        tft.drawString("Temp in 1 hrs: " + (String)hourly_weather[0].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[0].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[0].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[0].wind_speed + "m/s", 0, 120, GFXFF); 
        break;
      case 1:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString(getFutureTime(2, true), 0, 0, GFXFF);
        tft.drawString("Temp in 2 hrs: " + (String)hourly_weather[1].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[1].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[1].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[1].wind_speed + "m/s", 0, 120, GFXFF);  
        break;
      case 2:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString(getFutureTime(3, true), 0, 0, GFXFF);
        tft.drawString("Temp in 3 hrs: " + (String)hourly_weather[2].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[2].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[2].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[2].wind_speed + "m/s", 0, 120, GFXFF);   
        break;
    }
  }
  else if(button_state == 3 && !did_display){
    did_display = true;
    switch(disp_state){
      case 0:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString("Today: " + getFutureTime(0, false), 0, 0, GFXFF);
        tft.drawString("Max Temp: " + (String)daily_weather[0].temp + "F", 0, 30, GFXFF);
        tft.drawString("Min Temp: " + (String)daily_weather[0].min_temp + "F", 0, 60, GFXFF);
        tft.drawString((String)daily_weather[0].summary, 0, 90, GFXFF);
        tft.drawString("Humidity " + (String)daily_weather[0].humidity + "%", 0, 120, GFXFF);
        tft.drawString("Wind Speed: " + (String)daily_weather[0].wind_speed + "m/s", 0, 150, GFXFF); 
        break;
      case 1:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);   // Clear screen
        tft.drawString("Tmrw: " + getFutureTime(24, false), 0, 0, GFXFF);
        tft.drawString("Max Temp: " + (String)daily_weather[1].temp + "F", 0, 30, GFXFF);
        tft.drawString("Min Temp: " + (String)daily_weather[1].min_temp + "F", 0, 60, GFXFF);
        tft.drawString((String)daily_weather[1].summary, 0, 90, GFXFF);
        tft.drawString("Humidity " + (String)daily_weather[1].humidity + "%", 0, 120, GFXFF);
        tft.drawString("Wind Speed: " + (String)daily_weather[1].wind_speed + "m/s", 0, 150, GFXFF); 
        break;
    }
  }
  
  if(cur_millis - prev_millis_disp >= del_interval){
    prev_millis_disp = cur_millis;
    if(button_state == 2 || button_state == 3) disp_state = (disp_state + 1) % num_disp_states;
    did_display = false;
  }
}