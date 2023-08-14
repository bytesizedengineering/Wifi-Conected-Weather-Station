#define BUTTON_SELECT 0
#define ANEMOMETER 5
#define RAIN_GAUGE 6
#define BATTERY_CHG 9
#define WIND_VANE A2

#include "config.h"
#include <Wire.h>
#include <Adafruit_MS8607.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Fonts/FreeMono12pt7b.h>
#include <SPI.h>
#include "Adafruit_LC709203F.h" // battery monitor

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Adafruit IO Feeds
AdafruitIO_Feed *windSpeedFeed = io.feed("weather-station.wind-speed");
AdafruitIO_Feed *windDirectionFeed = io.feed("weather-station.wind-direction");
AdafruitIO_Feed *rainfallFeed = io.feed("weather-station.rainfall");
AdafruitIO_Feed *temperatureFeed = io.feed("weather-station.temperature");
AdafruitIO_Feed *humidityFeed = io.feed("weather-station.humidity");
AdafruitIO_Feed *pressureFeed = io.feed("weather-station.pressure");
AdafruitIO_Feed *batteryPercentFeed = io.feed("weather-station.battery-percent");
AdafruitIO_Feed *batteryVoltageFeed = io.feed("weather-station.battery-voltage");
AdafruitIO_Feed *batteryChargeStatusFeed = io.feed("weather-station.battery-charge-status");

// TFT display variables
const int displayWidth = 135;
const int displayHeight = 240;

// Weather meter variables
unsigned long lastTimeDataCollected = 0;
float windSpeed = 0.0;
unsigned long lastTimeWindSpeedMeasured = 0;
String windDirection = "";
unsigned long lastTimeWeatherDataSaved = 0;
float rainfall = 0.0;
unsigned long lastTimeRainfallMeasured = 0;

// MS8607 variables
Adafruit_MS8607 ms8607;
sensors_event_t temperature, humidity, pressure;
float temperatureF = 0.0;

// Battery monitor variables
Adafruit_LC709203F batteryMonitor;
bool batteryChargeStatus = false;
float batteryPercent = 0.0;
float batteryVoltage = 0.0;

// Button variables
unsigned long lastTimeButtonSelectWasPressed = 0;
int selectedValue = 0;

// Interrupt Service Routines
void IRAM_ATTR measureWindSpeed(){
  unsigned long timeLapsed = millis() - lastTimeWindSpeedMeasured;
  //Serial.println(timeLapsed);
  if(timeLapsed > 10){ // software debouncing of anemometer switch
    lastTimeWindSpeedMeasured = millis();
    if(timeLapsed < 1492){
      windSpeed = 1492.0/timeLapsed;
    }
    else
      windSpeed = 0.0;
  }
}

void IRAM_ATTR buttonSelectWasPressed(){
  if(millis() - lastTimeButtonSelectWasPressed > 100){
    lastTimeButtonSelectWasPressed = millis();
    selectedValue++;
    if(selectedValue > 7){
      selectedValue = 0;
    }
  }
}

void IRAM_ATTR updateRainfall(){
  if(millis() - lastTimeRainfallMeasured > 1000){ // software debouncing of rain gauge switch
    lastTimeRainfallMeasured = millis();
    rainfall += 0.011;
  }
}

void setup() {
  Serial.begin(115200);

  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // set up button
  attachInterrupt(BUTTON_SELECT, buttonSelectWasPressed, FALLING);

  // initialize TFT
  tft.init(displayWidth, displayHeight); // Init ST7789 240x135
  tft.setRotation(2); tft.setFont(&FreeMono12pt7b); tft.setCursor(0, 14);
  tft.fillScreen(ST77XX_BLACK);
  tft.println("Weather\nStation");  delay(1000);  tft.fillScreen(ST77XX_BLACK);

  // set up wind vane, anemometer, and rain gauge
  //pinMode(WIND_VANE, INPUT_PULLUP);
  attachInterrupt(ANEMOMETER, measureWindSpeed, FALLING);
  attachInterrupt(RAIN_GAUGE, updateRainfall, FALLING);

  // set up MS8607 temperature, pressure, and humidity sensor
  if (!ms8607.begin()) {
    while (1){
      Serial.println("Failed to find MS8607 chip");
      delay(10);
    }
  }
  Serial.println("MS8607 Found!");
  ms8607.setHumidityResolution(MS8607_HUMIDITY_RESOLUTION_OSR_8b);

  // set up battery monitor
  pinMode(BATTERY_CHG, INPUT);
  if (!batteryMonitor.begin()) {
    while (1){
      Serial.println(F("Couldnt find Adafruit LC709203F\nMake sure a battery is plugged in!"));
      delay(10);
    }
  }

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();
  while(io.status() < AIO_CONNECTED) { // wait for a connection
    Serial.print(".");
    delay(500);
  }
  Serial.println(); // we are connected
  Serial.println(io.statusText());
}

void loop() {
  io.run(); // keep client connected to io.adafruit.com and processes incoming data

  // collect data
  if(millis() - lastTimeDataCollected >= 1000){
    lastTimeDataCollected = millis();
    getWindDirection();
    ms8607.getEvent(&pressure, &temperature, &humidity);
    temperatureF = temperature.temperature*1.8 + 32;
    batteryChargeStatus = !digitalRead(BATTERY_CHG); // BATTERY_CHG is pulled LOW when charging
    batteryPercent = batteryMonitor.cellPercent();
    batteryVoltage = batteryMonitor.cellVoltage();
    if(selectedValue > 0){
      digitalWrite(TFT_BACKLITE, HIGH); // turn on backlite
      tft.fillScreen(ST77XX_BLACK);
      //tft.fillRect(0, 0, displayWidth, displayHeight/2, ST77XX_BLACK);
      tft.setCursor(0,14);
    }
    else{
      digitalWrite(TFT_BACKLITE, LOW); // turn off backlite
    }

    switch(selectedValue){
      case 0:
        Serial.println("standby mode");
        break;
      case 1:
        Serial.print("wind speed: ");       Serial.print(windSpeed);                      Serial.println(" mph");
        tft.setTextColor(ST77XX_WHITE);
        tft.println("wind\nspeed: ");       tft.print(windSpeed);                         tft.println("mph");
        break;
      case 2:
        Serial.print("wind direction: ");   Serial.println(windDirection);
        tft.setTextColor(ST77XX_CYAN);
        tft.println("wind\ndirection: ");   tft.println(windDirection);
        break;
      case 3:
        Serial.print("rainfall: ");         Serial.print(rainfall,3);                     Serial.println("in.");
        tft.setTextColor(ST77XX_BLUE);
        tft.println("rainfall: ");          tft.print(rainfall,3);                        tft.println("in.");
        break;
      case 4:
        Serial.print("temperature: ");      Serial.print(temperatureF);                   Serial.println("*F");
        tft.setTextColor(ST77XX_ORANGE);
        tft.println("temp: ");              tft.print(temperatureF);                      tft.write(0xF8); tft.println("F");
        break;
      case 5:
        Serial.print("humidity: ");         Serial.print(humidity.relative_humidity);     Serial.println("%rH");
        tft.setTextColor(ST77XX_MAGENTA);
        tft.println("humidity: ");          tft.print(humidity.relative_humidity);        tft.println("%rH");
        break;
      case 6:
        Serial.print("pressure: ");         Serial.print(pressure.pressure);              Serial.println("hPa");
        tft.setTextColor(ST77XX_YELLOW);
        tft.println("pressure: ");          tft.print(pressure.pressure);                 tft.println("hPa");
        break;
      case 7:
        if(batteryChargeStatus){
          Serial.print("battery charging - ");
          tft.setTextColor(ST77XX_GREEN);
          tft.println("charging"); tft.println();
        }
        else{
          Serial.print("battery discharging - ");
          tft.setTextColor(ST77XX_RED);
          tft.println("dis-\ncharging"); tft.println();
        }
        Serial.print("battery percent: ");  Serial.print(batteryPercent,1);               Serial.print("% battery voltage: "); Serial.print(batteryVoltage); Serial.println("V");
        tft.println("battery\npercent: ");  tft.print(batteryPercent);                    tft.println("%"); tft.println();
        tft.println("battery\nvoltage: ");  tft.print(batteryVoltage);                    tft.println("V");
    }
  }

  // save weather data to feeds
  if(millis() - lastTimeWeatherDataSaved >= 30000){
    lastTimeWeatherDataSaved = millis();
    windSpeedFeed->save(windSpeed);
    windDirectionFeed->save(windDirection);
    rainfallFeed->save(rainfall);
    temperatureFeed->save(temperatureF);
    humidityFeed->save(humidity.relative_humidity);
    pressureFeed->save(pressure.pressure);
    batteryChargeStatusFeed->save(batteryChargeStatus);
    batteryPercentFeed->save(batteryPercent);
    batteryVoltageFeed->save(batteryVoltage);
  }
}

void getWindDirection(){
  int value = 0;
  for(int i=0; i<100; i++){
    value += analogRead(WIND_VANE);
    delay(10);
  }
  value = value/100;
  //Serial.print("Wind Vane raw value: "); Serial.println(value);
  int directionValues[] = {7672, 3948, 4490, 800, 892, 630, 1780, 1220, 2788, 2368, 6128, 5836, 8191, 8100, 8191, 6840}; // E and SE both had the same max reading of 8191 from ADC. This means that SE wind directions are reported as E wind directions
  String directionWords[] = {"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE"};
  int diff = 10000;
  for(int i=0; i<16; i++){
    if(abs(value - directionValues[i]) < diff){
      diff = abs(value - directionValues[i]);
      windDirection = directionWords[i];
    }
  }
}
