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
  { "03d", w03, sizeof(w03) },
  { "03n", w03, sizeof(w03) },
  { "04d", w04, sizeof(w04) },
  { "04n", w04, sizeof(w04) },
  { "09d", w09, sizeof(w09) },
  { "09n", w09, sizeof(w09) },
  { "10d", w10d, sizeof(w10d) },
  { "10n", w10n, sizeof(w10n) },
  { "11d", w11, sizeof(w11) },
  { "11n", w11, sizeof(w11) },
  { "13d", w13, sizeof(w13) },
  { "13n", w13, sizeof(w13) },
  { "50d", w50, sizeof(w50) },
  { "50n", w50, sizeof(w50) }
};
struct Icon_Data small_icon_table[] = {
  { "01d", ws01d, sizeof(ws01d) },
  { "01n", ws01n, sizeof(ws01n) },
  { "02d", ws02d, sizeof(ws02d) },
  { "02n", ws02n, sizeof(ws02n) },
  { "03d", ws03, sizeof(ws03) },
  { "03n", ws03, sizeof(ws03) },
  { "04d", ws04, sizeof(ws04) },
  { "04n", ws04, sizeof(ws04) },
  { "09d", ws09, sizeof(ws09) },
  { "09n", ws09, sizeof(ws09) },
  { "10d", ws10d, sizeof(ws10d) },
  { "10n", ws10n, sizeof(ws10n) },
  { "11d", ws11, sizeof(ws11) },
  { "11n", ws11, sizeof(ws11) },
  { "13d", ws13, sizeof(ws13) },
  { "13n", ws13, sizeof(ws13) },
  { "50d", ws50, sizeof(ws50) },
  { "50n", ws50, sizeof(ws50) }, 
  { "rain_small", rain_small, sizeof(rain_small) }, 
  { "wind_small", wind_small, sizeof(wind_small) },
  { "sunrise_small", sunrise_small, sizeof(sunrise_small) },
  { "sunset_small", sunset_small, sizeof(sunset_small) }
};
struct Icon_Data tiny_icon_table[] = {
  { "01d", wt01d, sizeof(wt01d) },
  { "01n", wt01n, sizeof(wt01n) },
  { "02d", wt02d, sizeof(wt02d) },
  { "02n", wt02n, sizeof(w02n) },
  { "03d", wt03, sizeof(wt03) },
  { "03n", wt03, sizeof(wt03) },
  { "04d", wt04, sizeof(wt04) },
  { "04n", wt04, sizeof(wt04) },
  { "09d", wt09, sizeof(wt09) },
  { "09n", wt09, sizeof(wt09) },
  { "10d", wt10d, sizeof(wt10d) },
  { "10n", wt10n, sizeof(wt10n) },
  { "11d", wt11, sizeof(wt11) },
  { "11n", wt11, sizeof(wt11) },
  { "13d", wt13, sizeof(wt13) },
  { "13n", wt13, sizeof(wt13) },
  { "50d", wt50, sizeof(wt50) },
  { "50n", wt50, sizeof(wt50) }
};
Icon_Data* icon_table_arr[] = {icon_table, small_icon_table, tiny_icon_table};
const int icon_table_sizes[] = {sizeof(icon_table)/sizeof(icon_table[0]), sizeof(small_icon_table)/sizeof(small_icon_table[0]), sizeof(tiny_icon_table)/sizeof(tiny_icon_table[0])};

//custom fonts and GFXFF reference handle
#define GFXFF 1
#define CF_OSR11 &Open_Sans_Regular_11
#define CF_OSR12 &Open_Sans_Regular_12
#define CF_OSR13 &Open_Sans_Regular_13
#define CF_OSR14 &Open_Sans_Regular_14
#define CF_OSR15 &Open_Sans_Regular_15
#define CF_OSR17 &Open_Sans_Regular_17
#define CF_OSR20 &Open_Sans_Regular_20
#define CF_OSCB11 &Open_Sans_Condensed_Bold_11

//button pins
#define BUTTON1_PIN 13
#define BUTTON2_PIN 12
#define BUTTON3_PIN 14
//track which button is pressed
const int button_pins[] = {BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN};
const int arr_disp_states[] = {5, 2, 0};
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
  long sunrise;         //only for current
  long sunset;          //only for current
  char summary[128];  //only for daily
  float temp;           //in Kelvin; max temp for daily
  float min_temp;       //only for daily
  int humidity;
  float wind_speed;
  int pop;              //probability of percipitation; for hourly and daily
  char description[32];
  char icon[8];
};
struct weather_var cur_weather;
struct weather_var hourly_weather[24];
struct weather_var daily_weather[8];

const String cut_strings[4] = {
  "Expect a day of ",
  "The day will start with ",
  "There will be ",
  "You can expect "
};
const int NUM_CUT_STRINGS = 4;

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
  int tries = 0;
  int httpCode = http.GET();
  while(httpCode <= 0 && tries <= 5){
      Serial.println("Error on HTTP Request.");
      tries++;
      http.begin(weatherAPIFullCall);
      httpCode = http.GET();
  }
  if(tries == 5) {
    Serial.println("Could not get weather data");
    return 0;
  }
  weather_json = http.getString();
  Serial.print("Returned Payload: ");
  Serial.println(weather_json);
  http.end();

  Serial.println("Deserializing weather JSON");
  JsonDocument doc;
  deserializeJson(doc, weather_json);
  tmz_offset = doc["timezone_offset"];
  //get current weather
  cur_weather.dt = doc["current"]["dt"];
  cur_weather.sunrise = doc["current"]["sunrise"] | 0;
  cur_weather.sunset = doc["current"]["sunset"] | 0;
  cur_weather.temp = doc["current"]["temp"];
  cur_weather.humidity = doc["current"]["humidity"];
  cur_weather.wind_speed = doc["current"]["wind_speed"];
  strncpy(cur_weather.description, doc["current"]["weather"][0]["description"] | "", sizeof(cur_weather.description));
  cur_weather.description[0] = cur_weather.description[0] - 32;  //1st letter to uppercase
  strncpy(cur_weather.icon, doc["current"]["weather"][0]["icon"] | "", sizeof(cur_weather.icon));

  //get weather for next few hours
  JsonArray hrly_data = doc["hourly"];
  for (int i = 0; i < 24; i++) {
    hourly_weather[i].dt = hrly_data[i + 1]["dt"];
    hourly_weather[i].temp = hrly_data[i + 1]["temp"];
    hourly_weather[i].humidity = hrly_data[i + 1]["humidity"];
    hourly_weather[i].wind_speed = hrly_data[i + 1]["wind_speed"];
    hourly_weather[i].pop = (int)(hrly_data[i + 1]["pop"].as<float>() * 100);       //pop returns a float btwn 0 and 1 so I turn that into an int btwn 0 and 100
    strncpy(hourly_weather[i].description, hrly_data[i + 1]["weather"][0]["description"] | "", sizeof(hourly_weather[i].description));
    strncpy(hourly_weather[i].icon, hrly_data[i + 1]["weather"][0]["icon"] | "", sizeof(hourly_weather[i].icon));
  }

  //get weather for next few days
  JsonArray daily_data = doc["daily"];
  for (int i = 0; i < 8; i++) {
    daily_weather[i].dt = daily_data[i]["dt"];
    strncpy(daily_weather[i].summary, daily_data[i]["summary"] | "", sizeof(daily_weather[i].summary));
    daily_weather[i].temp = daily_data[i]["temp"]["max"];
    daily_weather[i].min_temp = daily_data[i]["temp"]["min"];
    daily_weather[i].humidity = daily_data[i]["humidity"];
    daily_weather[i].wind_speed = daily_data[i]["wind_speed"];
    daily_weather[i].pop = (int)(daily_data[i]["pop"].as<float>() * 100);       //pop returns a float btwn 0 and 1 so I turn that into an int btwn 0 and 100
    strncpy(daily_weather[i].description, daily_data[i]["weather"][0]["description"] | "", sizeof(daily_weather[i].description));
    strncpy(daily_weather[i].icon, daily_data[i]["weather"][0]["icon"] | "", sizeof(daily_weather[i].icon));
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
  Serial.print("returned future time ");
  Serial.println(temprtc.getTime("%a %Y-%m-%d %H:%M"));
  if (disp_type == 0) return temprtc.getTime("%a %Y-%m-%d %H:%M");  //D.O.W, date, time
  else if (disp_type == 1) return temprtc.getTime("%a %Y-%m-%d");   //D.O.W, date
  else if (disp_type == 2) return temprtc.getTime("%H:%M");         //time
  else if (disp_type == 3) return temprtc.getTime("%H");            //hour
  else if (disp_type == 4) return temprtc.getTime("%a");            //D.O.W
  else return "ERROR: invalid input for future time function";
}

String getOtherTime(long dt, int disp_type){
  time_t rawtime = dt + tmz_offset;  
  struct tm* timeinfo = gmtime(&rawtime);  //convert to struct tm
  char buf[16];

  if (disp_type == 0) {
    strftime(buf, sizeof(buf), "%H:%M", timeinfo);
    return String(buf);
  }
  return "ERROR: invalid input for rtc time function";
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

//prints icon according to name, location, and size
void printIcon(const char* name, int x, int y, int img_type) {
  uint8_t* icon_data = nullptr;
  unsigned long long icon_size = 0;
  int num_icons = icon_table_sizes[img_type];
  
  for (int i = 0; i < num_icons; i++) {
    if(strcmp(icon_table_arr[img_type][i].name, name) == 0){
      icon_data = const_cast<uint8_t*>(icon_table_arr[img_type][i].data);
      icon_size = icon_table_arr[img_type][i].size;
      break;
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

//modes: 0 - hourly weather, break each string up into multiple lines and lower font size if necessary 
//1 - daily weather, lowers font size if necessary
//note: assumes starting font size is 12 Open Sans
//returns: 0 for failure, 1 for no overflow, 2 for overflow
int printHandler(int mode, String to_print, int x, int y){
  int string_len = to_print.length();
  if(mode == 0){
    tft.setFreeFont(CF_OSR12);
    if(string_len > 8){ //split str to words
      String words[5];
      int word_index = 0;
      int word_start_indx = 0;
      for(int i = 0; i < string_len; i++){
        if(to_print[i] == 0x20){
          words[word_index] = to_print.substring(word_start_indx, i);
          word_index++;
          word_start_indx = i+1;
        }
      }
      words[word_index] = to_print.substring(word_start_indx, string_len); //last word
      /*
      Serial.println("PRINT HANDLER OUTPUT");
      for(int i = 0; i < 5; i++){
        Serial.println(words[i]);
      }
      */
      int num_words = word_index+1;
      int offset = 0;
      int line_gap = 11;
      if(num_words >= 3) offset = -3;
      else line_gap = 12;
      Serial.println("NUM WORDS");
      Serial.println(num_words);
      int line_num = 0;
      for(int i = 0; i < num_words; i++){
        int section_len = 0;
        if(line_num >= 2 && num_words >= 4 && i <= 2){
          tft.setFreeFont(CF_OSCB11);
          int cur_indx = to_print.indexOf(words[i]);
          tft.drawString(to_print.substring(cur_indx, cur_indx + 12), x, y+line_num*line_gap+offset, GFXFF);
          i++;
          Serial.println("PRINT CASE 1");
        }
        else if((i < num_words-1) && ((section_len = words[i].length() + words[i+1].length() + 1) <= 9) && num_words >= 3){
          tft.setFreeFont(CF_OSR12);
          tft.drawString(words[i] + " " + words[i+1], x+(int)((9-section_len)*1.5), y+line_num*line_gap+offset, GFXFF);
          i++;
          Serial.println("PRINT CASE 2");
        }
        else if((i < num_words-1) && ((section_len = words[i].length() + words[i+1].length() + 1) <= 10) && num_words >= 3){
          tft.setFreeFont(CF_OSCB11);
          tft.drawString(words[i] + " " + words[i+1], x+(10-section_len)*2, y+line_num*line_gap+offset, GFXFF);
          i++;
          Serial.println("PRINT CASE 3");
        }
        else if(words[i].length() >= 11){
          tft.setFreeFont(CF_OSCB11);
          tft.drawString(words[i].substring(0, 9) + ".", x, y+line_num*line_gap+offset, GFXFF);
          Serial.println("PRINT CASE 4");
        }
        else if(words[i].length() >= 10){
          tft.setFreeFont(CF_OSR11);
          tft.drawString(words[i].substring(0, 8) + ".", x, y+line_num*line_gap+offset, GFXFF);
          Serial.println("PRINT CASE 5");
        }
        else{
          section_len = words[i].length();
          if(section_len == 8) tft.drawString(words[i], x, y+line_num*line_gap+offset, GFXFF);
          else tft.drawString(words[i], x+(int)((9-section_len)*2.5), y+line_num*line_gap+offset, GFXFF);
          Serial.println("PRINT CASE 6");
        }
        line_num++;
      }
      if(line_num >= 1){
        return 2;
      }
      return 1;
    }
    else{
      tft.setFreeFont(CF_OSR13);
      tft.drawString(to_print, x+(int)((9-string_len)*2.5), y, GFXFF); //print no modification
      return 1;
    }
  }
  else if(mode == 1){
    tft.setFreeFont(CF_OSR12);
    for(int i = 0; i < NUM_CUT_STRINGS; i++){ //Shorten and capitalize
      if(to_print.indexOf(cut_strings[i]) != -1){
        to_print = to_print.substring(cut_strings[i].length());
        to_print[0] = to_print[0] - 32;
      }
    }
    string_len = to_print.length();
    if(string_len > 50){
      int break_indx = 0;
      for(int j = 40; j < string_len; j++){
        if(to_print[j] == 0x20){
          break_indx = j;
          break;
        }
      }
      String fst_str = to_print.substring(0, break_indx);
      String scd_str = to_print.substring(break_indx+1, string_len);
      tft.drawString(fst_str, x, y-10, GFXFF);
      tft.drawString(scd_str, x, y+2, GFXFF);
      return 2;
    }
    else{
      tft.drawString(to_print, x, y, GFXFF);
      return 1;
    }
  }
  else if(mode == 2){
    if(string_len >= 22){
      int break_indx = 0;
      for(int i = 17; i < string_len; i++){
        if(to_print[i] == 0x20){
          break_indx = i;
          String fst_str = to_print.substring(0, break_indx);
          String scd_str = to_print.substring(break_indx+1, string_len);
          tft.drawString(fst_str, x, y-7, GFXFF);
          tft.drawString(scd_str, x, y+7, GFXFF);
          return 2;
        }
      }
    }
    tft.drawString(to_print, x, y, GFXFF);
    return 1;
  }
  else{
    Serial.println("ERROR: Invalid mode for print handler");
    return 0;
  }
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

  rtc = ESP32Time(tmz_offset);
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
  int prev_button_state = button_state;
  for(int i = 0; i < num_buttons; i++){
    if(digitalRead(button_pins[i])){
      any_pressed = true;
      if(!button_pressed){
        button_state = i + 1;
        if(button_state <= 2){
          num_disp_states = arr_disp_states[i];
          disp_state = 0;
        }
        did_display = false;
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
    del_interval = 8 * 1000;
    prev_millis_disp = cur_millis;
    int start_indx = disp_state * 5; //cycles btwn 0, 5, 10, 15, 20 

    //top for current weather
    tft.fillScreen(TFT_BLACK);  //clear screen
    tft.drawString("Now:  " + getFutureTime(0, 0), 0, 0, GFXFF);
    Serial.println("CUR TIME");
    Serial.println(rtc.getTime("%a %Y-%m-%d %H:%M"));
    tft.drawLine(0, 17, 320, 17, TFT_GREEN);
    
    printIcon(cur_weather.icon, 180, 2, 0);
    tft.setFreeFont(CF_OSR20);
    tft.drawString((String)cur_weather.temp + "F", 5, 25, GFXFF);
    tft.setFreeFont(CF_OSR14);
    printHandler(2, (String)cur_weather.description, 5, 51);
    tft.setFreeFont(CF_OSR13);
    tft.drawString("Humid. " + (String)cur_weather.humidity + "%", 5, 76, GFXFF);
    printIcon("wind_small", 93, 72, 1);
    tft.drawString(String(cur_weather.wind_speed, 1) + "m/s", 112, 75, GFXFF);
    printIcon("sunrise_small", 168, 71, 1);
    tft.drawString(getOtherTime(cur_weather.sunrise, 0), 202, 75);
    printIcon("sunset_small", 246, 71, 1);
    tft.drawString(getOtherTime(cur_weather.sunset, 0), 280, 75);
    tft.drawLine(0, 94, 320, 94, TFT_GREEN);
    tft.setFreeFont(CF_OSR15);

    //hourly weather cycled; im sure i could loop this but im lazy
    tft.drawString(getFutureTime(start_indx+1, 3) + ":00", 5, 100, GFXFF);
    printIcon(hourly_weather[start_indx].icon, -10, 98, 1);
    tft.drawString((String)hourly_weather[start_indx].temp + "F", 2, 158);
    printHandler(0, (String)hourly_weather[start_indx].description, 1, 174);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx].pop + "%", 25, 209);
    printIcon("rain_small", 10, 205, 1);
    tft.drawString(String(hourly_weather[start_indx].wind_speed, 0) + "m/s", 21, 227);
    printIcon("wind_small", 2, 225, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(64, 94, 64, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+2, 3) + ":00", 74, 100, GFXFF);
    printIcon(hourly_weather[start_indx+1].icon, 59, 98, 1);
    tft.drawString((String)hourly_weather[start_indx+1].temp + "F", 70, 158);
    printHandler(0, (String)hourly_weather[start_indx+1].description, 67, 174);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+1].pop + "%", 90, 209);
    printIcon("rain_small", 75, 205, 1);
    tft.drawString(String(hourly_weather[start_indx+1].wind_speed, 0) + "m/s", 86, 227);
    printIcon("wind_small", 67, 225, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(128, 94, 128, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+3, 3) + ":00", 138, 100, GFXFF);
    printIcon(hourly_weather[start_indx+2].icon, 123, 98, 1);
    tft.drawString((String)hourly_weather[start_indx+2].temp + "F", 134, 158);
    printHandler(0, (String)hourly_weather[start_indx+2].description, 131, 174);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+2].pop + "%", 154, 209);
    printIcon("rain_small", 138, 205, 1);
    tft.drawString(String(hourly_weather[start_indx+2].wind_speed, 0) + "m/s", 149, 227);
    printIcon("wind_small", 130, 225, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(192, 94, 192, 240, TFT_GREEN);

    tft.drawString(getFutureTime(start_indx+4, 3) + ":00", 202, 100, GFXFF);
    printIcon(hourly_weather[start_indx+3].icon, 187, 98, 1);
    tft.drawString((String)hourly_weather[start_indx+3].temp + "F", 198, 158);
    printHandler(0, (String)hourly_weather[start_indx+3].description, 196, 174);
    tft.setFreeFont(CF_OSR12);
    tft.drawString((String)hourly_weather[start_indx+3].pop + "%", 218, 209);
    printIcon("rain_small", 202, 205, 1);
    tft.drawString(String(hourly_weather[start_indx+3].wind_speed, 0) + "m/s", 213, 227);
    printIcon("wind_small", 194, 225, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(256, 94, 256, 240, TFT_GREEN);
    
    //won't print on last cycle (25 hours ahead = out of bounds)
    if(!(start_indx+4 >= 24)){
      tft.drawString(getFutureTime(start_indx+5, 3) + ":00", 266, 100, GFXFF);
      printIcon(hourly_weather[start_indx+4].icon, 251, 98, 1);
      tft.drawString((String)hourly_weather[start_indx+4].temp + "F", 262, 158);
      printHandler(0, (String)hourly_weather[start_indx+4].description, 260, 174);
      tft.setFreeFont(CF_OSR12);
      tft.drawString((String)hourly_weather[start_indx+4].pop + "%", 282, 209);
      printIcon("rain_small", 266, 205, 1);
      tft.drawString(String(hourly_weather[start_indx+4].wind_speed, 0) + "m/s", 277, 227);
      printIcon("wind_small", 258, 225, 1);
      tft.setFreeFont(CF_OSR15);
    }
  }
  else if (button_state == 2 && !did_display) {     //display daily weather for 7 days
    did_display = true;
    del_interval = 15 * 1000;
    prev_millis_disp = cur_millis;
    int start_indx = disp_state * 4;

    //top 
    tft.fillScreen(TFT_BLACK);  //clear screen
    tft.drawString("Now:  " + rtc.getTime("%a %Y-%m-%d %H:%M"), 0, 0, GFXFF);
    tft.drawLine(0, 17, 320, 17, TFT_GREEN);
  
    if(start_indx == 0) tft.drawString("Today:", 2, 25);
    else tft.drawString(getFutureTime(start_indx*24, 4)+":", 2, 25);
    tft.setFreeFont(CF_OSR14);
    printIcon("rain_small", 105, 22, 1);
    tft.drawString((String)daily_weather[start_indx].pop + "%", 120, 24);
    printIcon("wind_small", 162, 22, 1);
    tft.drawString(String(daily_weather[start_indx].wind_speed, 0) + "m/s", 181, 24);
    tft.setFreeFont(CF_OSR17);
    tft.drawString(String(daily_weather[start_indx].temp,0) + "|" + String(daily_weather[start_indx].min_temp,0) + "F", 234, 24);
    tft.setFreeFont(CF_OSR12);
    if(printHandler(1, daily_weather[start_indx].summary, 2 ,60) == 2){
      printIcon(daily_weather[start_indx].icon, 44, 8, 2);
    }
    else printIcon(daily_weather[start_indx].icon, 40, 1, 1);
    printHandler(1, daily_weather[start_indx].summary, 2 ,60);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(0, 73, 320, 73, TFT_GREEN);

    tft.drawString(getFutureTime((start_indx+1)*24, 4)+":", 2, 79);
    tft.setFreeFont(CF_OSR14);
    printIcon("rain_small", 105, 80, 1);
    tft.drawString((String)daily_weather[start_indx+1].pop + "%", 120, 82);
    printIcon("wind_small", 162, 80, 1);
    tft.drawString(String(daily_weather[start_indx+1].wind_speed, 0) + "m/s", 181, 82);
    tft.setFreeFont(CF_OSR17);
    tft.drawString(String(daily_weather[start_indx+1].temp,0) + "|" + String(daily_weather[start_indx+1].min_temp,0) + "F", 234, 82);
    tft.setFreeFont(CF_OSR12);
    if(printHandler(1, daily_weather[start_indx+1].summary, 2 ,115) == 2){
      printIcon(daily_weather[start_indx+1].icon, 44, 64, 2);
    }
    else printIcon(daily_weather[start_indx+1].icon, 40, 57, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(0, 129, 320, 129, TFT_GREEN);

    tft.drawString(getFutureTime((start_indx+2)*24, 4)+":", 2, 135);
    tft.setFreeFont(CF_OSR14);
    printIcon("rain_small", 105, 136, 1);
    tft.drawString((String)daily_weather[start_indx+2].pop + "%", 120, 138);
    printIcon("wind_small", 162, 136, 1);
    tft.drawString(String(daily_weather[start_indx+2].wind_speed, 0) + "m/s", 181, 138);
    tft.setFreeFont(CF_OSR17);
    tft.drawString(String(daily_weather[start_indx+2].temp,0) + "|" + String(daily_weather[start_indx+2].min_temp,0) + "F", 234, 138);
    tft.setFreeFont(CF_OSR12);
    if(printHandler(1, daily_weather[start_indx+2].summary, 2 ,170) == 2){
      printIcon(daily_weather[start_indx+2].icon, 44, 120, 2);
    }
    else printIcon(daily_weather[start_indx+2].icon, 40, 113, 1);
    tft.setFreeFont(CF_OSR15);
    tft.drawLine(0, 184, 320, 184, TFT_GREEN);

    tft.drawString(getFutureTime((start_indx+3)*24, 4)+":", 2, 190);
    tft.setFreeFont(CF_OSR14);
    printIcon("rain_small", 105, 191, 1);
    tft.drawString((String)daily_weather[start_indx+3].pop + "%", 120, 193);
    printIcon("wind_small", 162, 191, 1);
    tft.drawString(String(daily_weather[start_indx+3].wind_speed, 0) + "m/s", 181, 193);
    tft.setFreeFont(CF_OSR17);
    tft.drawString(String(daily_weather[start_indx+3].temp,0) + "|" + String(daily_weather[start_indx+3].min_temp,0) + "F", 234, 193);
    tft.setFreeFont(CF_OSR12);
    if(printHandler(1, daily_weather[start_indx+3].summary, 2 ,226) == 2){
      printIcon(daily_weather[start_indx+3].icon, 44, 175, 2);
    }
    else printIcon(daily_weather[start_indx+3].icon, 40, 168, 1);
    tft.setFreeFont(CF_OSR15);
  } 
  else if (button_state == 3 && !did_display) { //cycles to next screen of whatever is currently displayed
    prev_millis_disp = cur_millis;
    disp_state = (disp_state + 1) % num_disp_states;
    button_state = prev_button_state;
  }

  if (cur_millis - prev_millis_disp >= del_interval) {
    prev_millis_disp = cur_millis;
    disp_state = (disp_state + 1) % num_disp_states;
    did_display = false;
  }
}