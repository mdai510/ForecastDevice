#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ESP32Time.h>
//for display
#include "SPI.h"
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include "Weather_Icons.h"  //include all image arrays

//for displaying images
#define MAX_IMAGE_WIDTH 240
PNG png;
int16_t xpos;  //position variables must be global (PNGdec does not handle position coordinates)
int16_t ypos;

struct Icon_Data {
  const char* name;
  const uint8_t* data;
  unsigned long long size;
};

struct Icon_Data icon_table[] = {
  { "01d", w01d, sizeof(w01d) },
  { "01n", w01n, sizeof(w01n) },
  { "02d", w02d, sizeof(w02d) },
  { "02n", w02n, sizeof(w02n) },
  { "03d", w03d, sizeof(w03d) },
  { "03n", w03n, sizeof(w03n) },
  { "04d", w04d, sizeof(w04d) },
  { "04n", w04n, sizeof(w04n) },
  { "09d", w09d, sizeof(w09d) },
  { "09n", w09n, sizeof(w09n) },
  { "10d", w10d, sizeof(w10d) },
  { "10n", w10n, sizeof(w10n) },
  { "11d", w11d, sizeof(w11d) },
  { "11n", w11n, sizeof(w11n) },
  { "13d", w13d, sizeof(w13d) },
  { "13n", w13n, sizeof(w13n) },
  { "50d", w50d, sizeof(w50d) },
  { "50n", w50n, sizeof(w50n) }
};
struct Icon_Data small_icon_table[] = {
  { "01d", ws01d, sizeof(ws01d) },
  { "01n", ws01n, sizeof(ws01n) },
  { "02d", ws02d, sizeof(ws02d) },
  { "02n", ws02n, sizeof(ws02n) },
  { "03d", ws03d, sizeof(ws03d) },
  { "03n", ws03n, sizeof(ws03n) },
  { "04d", ws04d, sizeof(ws04d) },
  { "04n", ws04n, sizeof(ws04n) },
  { "09d", ws09d, sizeof(ws09d) },
  { "09n", ws09n, sizeof(ws09n) },
  { "10d", ws10d, sizeof(ws10d) },
  { "10n", ws10n, sizeof(ws10n) },
  { "11d", ws11d, sizeof(ws11d) },
  { "11n", ws11n, sizeof(ws11n) },
  { "13d", ws13d, sizeof(ws13d) },
  { "13n", ws13n, sizeof(ws13n) },
  { "50d", ws50d, sizeof(ws50d) },
  { "50n", ws50n, sizeof(ws50n) },
  { "rain_small", rain_small, sizeof(rain_small) },
  { "wind_small", wind_small, sizeof(wind_small) }
};
const int num_icons = sizeof(icon_table);
const uint8_t* selected_data = nullptr;
size_t selected_size = 0;

//custom fonts and GFXFF reference handle
#define GFXFF 1
#define CF_OSR11 &Open_Sans_Regular_11
#define CF_OSR12 &Open_Sans_Regular_12
#define CF_OSR14 &Open_Sans_Regular_14
#define CF_OSR15 &Open_Sans_Regular_15

//button pins
#define BUTTON1_PIN 13
#define BUTTON2_PIN 12
#define BUTTON3_PIN 14
//track which button is pressed
const int button_pins[] = {BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN};
const int arr_disp_states[] = {5, 3, 2};
int num_buttons = sizeof(button_pins) / sizeof(button_pins[0]);
int button_state = 1;
bool button_pressed = false;
bool any_pressed;

//use hardware SPI
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
String weather_API_call = "http://api.openweathermap.org/data/3.0/onecall?lat=";

// time vars
ESP32Time rtc;
int tmz_offset;

// weather info struct
struct weather_var {
  long dt;
  const char* summary;  //only for daily
  float temp;           //in Kelvin; max temp for daily
  float min_temp;       //only for daily
  int humidity;
  float wind_speed;
  int pop;       //probability of percipitation; for hourly and daily
  const char* description;
  const char* icon;
};
struct weather_var cur_weather;
struct weather_var hourly_weather[24];
struct weather_var daily_weather[7];

// loop variables
// weather call variables
unsigned long weather_lastcall = 0;  //millis() of last API call
int delay_mins = 60;
unsigned long weather_delay_mils = delay_mins * 60 * 1000;  //delay between weather api calls
// display loop variables
unsigned long prev_millis_disp = 0;
int disp_state = 0;
int num_disp_states = 5;
bool did_display = false;

String beaufort_scale[] = {
  "calm",
  "light air",
  "light breeze",
  "gentle breeze", 
  "mod. breeze",
  "fresh breeze",
  "strong breeze",
  "near gale",
  "fresh gale",
  "strong gale",
  "storm",
  "violent storm",
  "hurricane"
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Gets location information from IPv6 Address
String ipLocation() {
  ipv6 = WiFi.globalIPv6();
  Serial.print("WiFi IPv6 Address: ");
  Serial.println(ipv6);

  String ipv6_str = ipv6.toString();
  ip_API_call = ip_API_call + "" + ipv6_str + "" + ip_API_fields;
  Serial.print("Calling API: ");
  Serial.println(ip_API_call);
  //call api
  HTTPClient http;
  http.begin(ip_API_call);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Returned Payload: ");
    Serial.println(payload);
    return payload;
  } else {
    Serial.println("Error on HTTP Request.");
    return "";
  }
  http.end();
}

//Parses json information from IP API
void parseIP_JSON(String location_json) {
  Serial.println("Deserializing ip JSON");
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
void getLocation() {
  Serial.println("Getting location...");
  //Use IPLocation API to get location
  String location_json = ipLocation();
  while (location_json == "") {
    location_json = ipLocation();
  }
  //parse json
  parseIP_JSON(location_json);
}

String parseWindSpeed(float wind_speed){
  String wind;
  if(wind_speed <= 0.2) wind = "calm";
}

//gets weather information from weather API and sets variables accordingly
int getWeather() {
  Serial.println("Getting weather...");
  String weatherAPIFullCall = weather_API_call + "" + lat + "&lon=" + lon + "&units=imperial&exclude=minutely,alerts&appid=" + weather_API_key;
  Serial.print("Calling API: ");
  Serial.println(weatherAPIFullCall);
  String weather_json;

  HTTPClient http;
  http.begin(weatherAPIFullCall);
  int httpCode = http.GET();
  if (httpCode > 0){
    weather_json = http.getString();
    Serial.print("Returned Payload: ");
    Serial.println(weather_json);
  } 
  else{
    Serial.println("Error on HTTP Request.");
    return 0;
  }
  http.end();

  Serial.println("Deserializing weather JSON");
  JsonDocument doc;
  deserializeJson(doc, weather_json);
  tmz_offset = doc["timezone_offset"];
  //get current weather
  cur_weather.dt = doc["current"]["dt"];
  cur_weather.temp = doc["current"]["temp"];
  cur_weather.humidity = doc["current"]["humidity"];
  cur_weather.wind_speed = doc["current"]["wind_speed"];
  cur_weather.description = doc["current"]["weather"][0]["description"];
  cur_weather.icon = doc["current"]["weather"][0]["icon"];
  Serial.println(cur_weather.icon);

  //get weather for next few hours
  JsonArray hrly_data = doc["hourly"];
  for (int i = 0; i < 24; i++) {
    hourly_weather[i].dt = hrly_data[i + 1]["dt"];
    hourly_weather[i].temp = hrly_data[i + 1]["temp"];
    hourly_weather[i].humidity = hrly_data[i + 1]["humidity"];
    hourly_weather[i].wind_speed = hrly_data[i + 1]["wind_speed"];
    hourly_weather[i].pop = (int)(hrly_data[i + 1]["pop"].as<float>() * 100);
    Serial.println(hourly_weather[i].pop);
    hourly_weather[i].description = hrly_data[i + 1]["weather"][0]["description"];
    hourly_weather[i].icon = hrly_data[i + 1]["weather"][0]["icon"];
  }

  //get weather for next few days
  JsonArray daily_data = doc["daily"];
  for (int i = 0; i < 7; i++) {
    daily_weather[i].dt = daily_data[i + 1]["dt"];
    daily_weather[i].summary = daily_data[i + 1]["summary"];
    daily_weather[i].temp = daily_data[i + 1]["temp"]["max"];
    daily_weather[i].min_temp = daily_data[i + 1]["temp"]["min"];
    daily_weather[i].humidity = daily_data[i + 1]["humidity"];
    daily_weather[i].wind_speed = daily_data[i + 1]["wind_speed"];
    daily_weather[i].pop = (int)(daily_data[i + 1]["pop"].as<float>() * 100);
    daily_weather[i].description = daily_data[i + 1]["weather"][0]["description"];
    daily_weather[i].icon = daily_data[i + 1]["weather"][0]["icon"];
  }
  return 1;
}

//connect to wifi
void wifiConnect() {
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {}
  WiFi.enableIPv6();
  Serial.println("Connected to WiFi.");
}

String getFutureTime(int future_hrs, int disp_type) {
  ESP32Time temprtc = rtc;
  temprtc.offset = tmz_offset + future_hrs * 3600;
  if (disp_type == 0) return temprtc.getTime("%a %Y-%m-%d %H:%M");  //D.O.W, date, time
  else if (disp_type == 1) return temprtc.getTime("%a %Y-%m-%d");   //D.O.W, date
  else if (disp_type == 2) return temprtc.getTime("%H:%M");         //time
  else if (disp_type == 3) return temprtc.getTime("%H");            //hour
  else return "ERROR: invalid input for future time function";
}

// Function stolen from Flash_transparent_PNG.ino example
// pngDraw: Callback function to draw pixels to the display
// This function will be called during decoding of the png file to render each image
// line to the TFT. PNGdec generates the image line and a 1bpp mask.
void pngDraw(PNGDRAW* pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];         // Line buffer for rendering
  uint8_t maskBuffer[1 + MAX_IMAGE_WIDTH / 8];  // Mask buffer

  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

  if (png.getAlphaMask(pDraw, maskBuffer, 255)) {
    // Note: pushMaskedImage is for pushing to the TFT and will not work pushing into a sprite
    tft.pushMaskedImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer, maskBuffer);
  }
}

//prints icon according to name, location, and type (large or small)
void printIcon(const char* name, int x, int y, int img_type) {
  uint8_t* icon_data = nullptr;
  unsigned long long icon_size = 0;

  for (int i = 0; i < num_icons; i++) {
    if(img_type == 0){ //large icon table
      if (strcmp(icon_table[i].name, name) == 0) {
        icon_data = const_cast<uint8_t*>(icon_table[i].data);
        icon_size = icon_table[i].size;
        break;
      }
    }
    else if(img_type == 1){ //small icon table
      if (strcmp(small_icon_table[i].name, name) == 0) {
        icon_data = const_cast<uint8_t*>(small_icon_table[i].data);
        icon_size = small_icon_table[i].size;
        break;
      }
    }
  }

  //if no icon with inpt name found
  if (icon_data == nullptr) {
    Serial.print("ERROR: Icon not found for ");
    Serial.println(name);
    return;
  }

  //draw icon 
  int16_t rc = png.openFLASH(icon_data, icon_size, pngDraw);
  xpos = x;
  ypos = y;
  tft.startWrite();
  rc = png.decode(NULL, 0);
  tft.endWrite();
  Serial.print("Icon Displayed: ");
  Serial.println(name);
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
  tft.fillScreen(TFT_BLACK);  // Clear screen
  tft.setFreeFont(CF_OSR15);  // Select the font

  Serial.begin(115200);
  /*
  tft.drawString("Starting...", 0, 30, GFXFF);
  delay(1000);*/

  tft.fillScreen(TFT_BLACK);  // Clear screen
  tft.drawString("Connecting to WiFi...", 0, 30, GFXFF);
  //Connect to WiFi
  wifiConnect();

  //get delay until IPv6 is valid (it takes some time)
  tft.fillScreen(TFT_BLACK);  // Clear screen
  tft.drawString("Obtaining IPv6...", 0, 30, GFXFF);
  Serial.println("Obtaining IPv6...");
  while (WiFi.globalIPv6().toString() == "::") {}

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Getting Location...", 0, 30, GFXFF);
  getLocation();
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Getting Weather Data...", 0, 30, GFXFF);
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
  if (cur_millis - weather_lastcall >= weather_delay_mils) {
    //call weather API
    wifiConnect();
    getWeather();
    WiFi.disconnect();
    weather_lastcall = cur_millis;
  }

  //handle button presses
  any_pressed = false;
  for(int i = 0; i < num_buttons; i++){
    if(digitalRead(button_pins[i])){
      any_pressed = true;
      if(!button_pressed){
        button_state = i + 1;
        num_disp_states = arr_disp_states[i];
        did_display = false;
        disp_state = 0;
        Serial.print("Button Pressed: ");
        Serial.println(i + 1);
        button_pressed = true;
      }
      break;
    }
  }
  if(!any_pressed && button_pressed) button_pressed = false;
  
  if (button_state == 1 && !did_display) { //display current weather at top of screen and cycle through hourly weather at the bottom
    did_display = true;
    del_interval = 6 * 1000;
    int start_indx = disp_state * 5; //cycles btwn 0, 5, 10, 15, 20 

    //top for current weather
    tft.fillScreen(TFT_BLACK);  //clear screen
    tft.drawString("Now:  " + rtc.getTime("%a %Y-%m-%d %H:%M"), 0, 0, GFXFF);
    tft.drawLine(0, 17, 320, 17, TFT_GREEN);
    tft.drawString((String)cur_weather.temp + "F", 5, 30, GFXFF);
    tft.drawString((String)cur_weather.description, 5, 53, GFXFF);
    tft.drawString("Humidity: " + (String)cur_weather.humidity + "%", 5, 76, GFXFF);
    tft.drawString("Wind: " + (String)cur_weather.wind_speed + "m/s", 160, 76, GFXFF);
    printIcon(cur_weather.icon, 180, 0, 0);
    tft.drawLine(0, 94, 320, 94, TFT_GREEN);

    //hourly weather cycled
    tft.drawString(getFutureTime(start_indx+1, 3) + ":00", 5, 102, GFXFF);
    printIcon(hourly_weather[start_indx].icon, -10, 100, 1);
    tft.drawString((String)hourly_weather[start_indx].temp + "F", 2, 165);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx].description, 1, 180);
    tft.drawString(":" + (String)hourly_weather[start_indx].pop + "%", 25, 204);
    printIcon("rain_small", 10, 200, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(64, 94, 64, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+2, 3) + ":00", 74, 102, GFXFF);
    printIcon(hourly_weather[start_indx+1].icon, 59, 100, 1);
    tft.drawString((String)hourly_weather[start_indx+1].temp + "F", 70, 165);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+1].description, 67, 180);
    tft.drawString(":" + (String)hourly_weather[start_indx+1].pop + "%", 89, 204);
    printIcon("rain_small", 75, 200, 1);
    tft.setFreeFont(CF_OSR11);
    tft.drawString(":" + (String)hourly_weather[start_indx+1].wind_speed + "m/s", 89, 220);
    printIcon("wind_small", 67, 220, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(128, 94, 128, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+3, 3) + ":00", 138, 102, GFXFF);
    printIcon(hourly_weather[start_indx+2].icon, 123, 100, 1);
    tft.drawString((String)hourly_weather[start_indx+2].temp + "F", 134, 165);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+2].description, 131, 180);
    tft.drawString(":" + (String)hourly_weather[start_indx+2].pop + "%", 153, 204);
    printIcon("rain_small", 138, 200, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(192, 94, 192, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+4, 3) + ":00", 202, 102, GFXFF);
    printIcon(hourly_weather[start_indx+3].icon, 187, 100, 1);
    tft.drawString((String)hourly_weather[start_indx+3].temp + "F", 198, 165);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+3].description, 196, 180);
    tft.drawString(":" + (String)hourly_weather[start_indx+3].pop + "%", 217, 204);
    printIcon("rain_small", 202, 200, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(256, 94, 256, 240, TFT_GREEN);
    
    //won't print on last cycle (25 hours ahead = out of bounds)
    if(!(start_indx+4 >= 24)){
      tft.drawString(getFutureTime(start_indx+5, 3) + ":00", 266, 102, GFXFF);
      printIcon(hourly_weather[start_indx+4].icon, 251, 100, 1);
      tft.drawString((String)hourly_weather[start_indx+4].temp + "F", 262, 165);
      tft.setFreeFont(CF_OSR12);
      tft.drawString((String)hourly_weather[start_indx+4].description, 260, 180);
      tft.drawString(":" + (String)hourly_weather[start_indx+4].pop + "%", 281, 204);
      printIcon("rain_small", 266, 200, 1);
      tft.setFreeFont(CF_OSR15);
    }
  } 
  else if (button_state == 2 && !did_display) {  //display daily weather for 7 days
    did_display = true;
    //switch cases enable cycling between different things to display and different time intervals for each display
    switch (disp_state) {
      case 0:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);  // Clear screen
        tft.drawString(getFutureTime(1, 0), 0, 0, GFXFF);
        tft.drawString("Temp in 1 hrs: " + (String)hourly_weather[0].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[0].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[0].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[0].wind_speed + "m/s", 0, 120, GFXFF);
        break;
      case 1:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);  // Clear screen
        tft.drawString(getFutureTime(2, 0), 0, 0, GFXFF);
        tft.drawString("Temp in 2 hrs: " + (String)hourly_weather[1].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[1].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[1].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[1].wind_speed + "m/s", 0, 120, GFXFF);
        break;
      case 2:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);  // Clear screen
        tft.drawString(getFutureTime(3, 0), 0, 0, GFXFF);
        tft.drawString("Temp in 3 hrs: " + (String)hourly_weather[2].temp + "F", 0, 30, GFXFF);
        tft.drawString((String)hourly_weather[2].description, 0, 60, GFXFF);
        tft.drawString("Humidity " + (String)hourly_weather[2].humidity + "%", 0, 90, GFXFF);
        tft.drawString("Wind Speed: " + (String)hourly_weather[2].wind_speed + "m/s", 0, 120, GFXFF);
        break;
    }
  } else if (button_state == 3 && !did_display) {
    did_display = true;
    switch (disp_state) {
      case 0:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);  // Clear screen
        tft.drawString("Today: " + getFutureTime(0, 1), 0, 0, GFXFF);
        tft.drawString("Max Temp: " + (String)daily_weather[0].temp + "F", 0, 30, GFXFF);
        tft.drawString("Min Temp: " + (String)daily_weather[0].min_temp + "F", 0, 60, GFXFF);
        tft.setTextWrap(true, true);
        tft.drawString((String)daily_weather[0].summary, 0, 90, GFXFF);
        tft.setTextWrap(false, false);
        tft.drawString("Humidity " + (String)daily_weather[0].humidity + "%", 0, 120, GFXFF);
        tft.drawString("Wind Speed: " + (String)daily_weather[0].wind_speed + "m/s", 0, 150, GFXFF);
        break;
      case 1:
        del_interval = 3000;
        tft.fillScreen(TFT_BLACK);  // Clear screen
        tft.drawString("Tmrw: " + getFutureTime(24, 1), 0, 0, GFXFF);
        tft.drawString("Max Temp: " + (String)daily_weather[1].temp + "F", 0, 30, GFXFF);
        tft.drawString("Min Temp: " + (String)daily_weather[1].min_temp + "F", 0, 60, GFXFF);
        tft.setTextWrap(true, true);
        tft.drawString((String)daily_weather[1].summary, 0, 90, GFXFF);
        tft.setTextWrap(false, false);
        tft.drawString("Humidity " + (String)daily_weather[1].humidity + "%", 0, 120, GFXFF);
        tft.drawString("Wind Speed: " + (String)daily_weather[1].wind_speed + "m/s", 0, 150, GFXFF);
        break;
    }
  }

  if (cur_millis - prev_millis_disp >= del_interval) {
    prev_millis_disp = cur_millis;
    //if (button_state == 2 || button_state == 3) disp_state = (disp_state + 1) % num_disp_states;
    disp_state = (disp_state + 1) % num_disp_states;
    did_display = false;
  }
}