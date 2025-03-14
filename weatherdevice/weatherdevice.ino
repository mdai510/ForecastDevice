#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

//network information
const char* ssid = "1987 Toyota Corolla GT-S";
const char* password = "hanselnina123";
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

struct weather_var{
  long dt;
  float temp; //in Kelvin
  int humidity;
  float wind_speed;
  const char* description;
  const char* icon;
};

struct weather_var cur_weather;

// loop variables
unsigned long last_call = 0; //millis() of last API call
unsigned long delay_mils = 1800 * 1000; //delay between weather api calls

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
  cur_weather.dt = doc["current"]["dt"];
  cur_weather.temp = doc["current"]["temp"];
  cur_weather.humidity = doc["current"]["humidity"];
  cur_weather.wind_speed = doc["current"]["wind_speed"];
  cur_weather.description = doc["current"]["weather"]["description"];
  cur_weather.icon = doc["current"]["weather"]["icon"];
  return 1;
}

void setup() {
  Serial.begin(115200);
  //Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  WiFi.enableIPv6();
  Serial.println("Connected to WiFi.");

  //get delay until IPv6 is valid (it takes some time)
  while (WiFi.globalIPv6().toString() == "::") {
    Serial.println("Obtaining IPv6...");
    delay(500);
  }

  get_location();
}

//display weather/location info 
//update weather each hour
void loop() {  
  //add code to check if wifi and ip are connected

  //if condition fulfilled if time specified in delay_mils has passed
  //Note: millis() overflow is not an issue because both millis and last_call are unsigned longs
  if(millis() - last_call >= delay_mils || last_call == 0){
    //call weather API
    get_weather();
    last_call = millis();
    Serial.println(cur_weather.temp);
  }

  // Serial.println(WiFi.globalIPv6());
  // delay(5000);
}
