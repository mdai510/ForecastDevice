#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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
const char* ssid = "hansel";
const char* password = "40435lajolla";
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

const char* ntpServer = "pool.ntp.org";
const long daylightOffset = 0;
long tmzOffset;

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
unsigned long last_call = 0; //millis() of last API call
int delay_mins = 60;
unsigned long delay_mils = delay_mins * 60 * 1000; //delay between weather api calls

//get date and time from NTP Client
String getDateTimeStr(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "ERROR OBTAINING TIME";
  }
  
  char buf[80];

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M", &timeinfo);
  printf("%s\n", buf);
  return buf;
}

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
  } else {
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

void setup() {
  /*
  lcd.clear();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("current temp: "); */

  //setup tft
  tft.begin();
  tft.setRotation(1);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.setFreeFont(FF18);       // Select the font

  tft.drawString("Starting...", 0, 30, GFXFF);
  delay(1000);

  Serial.begin(115200);
  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.drawString("Connecting to WiFi...", 0, 30, GFXFF);
  //Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  WiFi.enableIPv6();
  Serial.println("Connected to WiFi.");

  //get delay until IPv6 is valid (it takes some time)
  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.drawString("Obtaining IPv6...", 0, 30, GFXFF);
  while (WiFi.globalIPv6().toString() == "::") {
    Serial.println("Obtaining IPv6...");
    delay(500);
  }

  get_location();
  get_weather();
  last_call = millis();
  Serial.println(cur_weather.temp);

  configTime(tmzOffset, daylightOffset, ntpServer);
}

//display weather/location info 
//update weather each hour
void loop() {  
  // lcd.setCursor(0, 1);
  // lcd.print(cur_weather.temp);

  //add code to check if wifi and ip are connected

  //if condition fulfilled if time specified in delay_mils has passed
  //Note: millis() overflow is not an issue because both millis and last_call are unsigned longs
  if(millis() - last_call >= delay_mils){
    //call weather API
    get_weather();
    last_call = millis();
    // Serial.println(cur_weather.temp);
    // Serial.println(hourly_weather[23].temp);
  }

  tft.fillScreen(TFT_BLACK);   // Clear screen
  tft.drawString(getDateTimeStr(), 0, 0, GFXFF);
  tft.drawString("Current Temp: " + (String)cur_weather.temp + "F", 0, 30, GFXFF);
  tft.drawString((String)cur_weather.description, 0, 60, GFXFF);
  tft.drawString("Humidity " + (String)cur_weather.humidity + "%", 0, 90, GFXFF);
  tft.drawString("Wind Speed: " + (String)cur_weather.wind_speed + "m/s", 0, 120, GFXFF);
  delay(1000);
}
