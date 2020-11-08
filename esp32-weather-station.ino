/** The MIT License (MIT)
    based on the esp8266-weather-station by 2018 by Daniel Eichhorn
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
    See more at https://blog.squix.org
*/

/*****************************
    Important: see settings.h to configure your settings!!!
 * ***************************/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_STMPE610.h>
#include <Adafruit_BME280.h>
#include <sps30.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <JsonListener.h>
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "SparkFun_SCD30_Arduino_Library.h"

#include "settings.h"
#include "settings_private.h"
#include "ArialRounded.h"
#include "weathericons.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"

#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3

#define BACKLIGHT_PWM_FRQ 5000
#define BACKLIGHT_PWM_RES 8
#define BACKLIGHT_PWM_CHN 0
#define BACKLIGHT_PWM_DAY 128
#define BACKLIGHT_PWM_NIGHT 32

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                      ILI9341_YELLOW, // 2
                      0x7E3C
                     };

// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors

ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
simpleDSTadjust dstAdjusted(StartRule, EndRule);
Adafruit_BME280 bme280;
SCD30 CO2Sensor;

void updateWeatherData();
void updateSensorData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawWifiQuality();
void drawCurrentWeather();
void drawForecast(int16_t x, int16_t y);
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
void drawSensorValues();
void sendDataToOpenSenseMap();

const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);

time_t dstOffset = 0;
float temp = 0;
float humidity = 0;
float pressure = 0;
int spsState = 0;
struct sps30_measurement sps30Data;
uint16_t co2 = 0;

void connectWifi() {

    static int network = 0;

    drawProgress(50, "Connecting to WiFi...");

    if (WiFi.status() == WL_CONNECTED) return;

    int i = 0;

    if ((network == 0) || (network == 1)) {

        Serial.println();
        Serial.print("Connecting to WiFi ");
        Serial.print(WIFI_SSID1);

        // start with WiFi 2 office
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID1, WIFI_PASS1);

        for (i = 0; i < 10; i ++) {
            delay(1000);
            Serial.print(".");
            if (WiFi.status() == WL_CONNECTED) {
                network = 1;
                break;
            }
        }
    }

    if ((network == 0) || (network == 2)) {

        // switch networks if still not connected (home)
        if (WiFi.status() != WL_CONNECTED) {

            Serial.println();
            Serial.print("Connecting to WiFi ");
            Serial.print(WIFI_SSID2);

            WiFi.disconnect();
            WiFi.mode(WIFI_STA);
            WiFi.begin(WIFI_SSID2, WIFI_PASS2);

            for (i = 0; i < 10; i ++) {
                delay(1000);
                Serial.print(".");
                if (WiFi.status() == WL_CONNECTED) {
                    network = 2;
                    break;
                }
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected...");
    }
    else {
        Serial.println("No WIFI found...");
        WiFi.disconnect();
    }
}

void setup() {
    int16_t result;

    Serial.begin(115200);

    // Turn TFT backlight on (needs to wired on ESP32)
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);    // LOW to Turn on;

    // configure backlight PWM
    ledcSetup(BACKLIGHT_PWM_CHN, BACKLIGHT_PWM_FRQ, BACKLIGHT_PWM_RES);
    ledcAttachPin(TFT_LED, BACKLIGHT_PWM_CHN);
    ledcWrite(BACKLIGHT_PWM_CHN, BACKLIGHT_PWM_DAY);  // set to day level

    gfx.init();
    gfx.fillBuffer(MINI_BLACK);
    gfx.commit();

    // start BME280
    if (!bme280.begin()) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }

    // set to single sampling and force mode to save power and keep sensor cool
    bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                       Adafruit_BME280::SAMPLING_X1, // temperature
                       Adafruit_BME280::SAMPLING_X1, // pressure
                       Adafruit_BME280::SAMPLING_X1, // humidity
                       Adafruit_BME280::FILTER_OFF,
                       Adafruit_BME280::STANDBY_MS_1000);

    Serial.println("Found BME280 sensor...");

    // setup scd30 CO2 sensor
    if (CO2Sensor.begin() == false)
    {
        Serial.println("SCD sensor probing failed, check wiring!");
        while (1);
    }
    CO2Sensor.setMeasurementInterval(30);
    Serial.println("Found SCD30 sensor...");

    // start SPS30
    while (sps30_probe() != 0) {
        Serial.print("SPS sensor probing failed\n");
        delay(500);
    }
    Serial.println("SPS sensor probing successful...");
    result = sps30_set_fan_auto_cleaning_interval_days(4); // clean all 4 days
    if (result) {
        Serial.print("error setting the auto-clean interval: ");
        Serial.println(result);
    }

    // set RF modem to sleep
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    btStop();
    WiFi.mode(WIFI_OFF);
}

void loop() {

    static int cycle = 0;
    static int task = 1 << UPDATE_TURN_SDS30_ON;
    static unsigned long nextMillis = 0;
    static int lastSecond = 60;

    char *dstAbbrev;
    time_t now;
    struct tm * timeinfo;

    if (millis() >= nextMillis) { // if it is time, perform tasks scheduled
        if (((task >> UPDATE_TURN_SDS30_ON) & 1U))  {

            if (sps30_start_measurement() < 0) {
                Serial.println("Error starting measurement");
            }
            else {
                Serial.println("SPS30 on");
                spsState = 1;
            }

            // next sleep time and tasks
            task &= ~(1UL << UPDATE_TURN_SDS30_ON);
            task |= 1UL << UPDATE_SENSORS;
            if (cycle == 0) task |= 1UL << UPDATE_FORECAST;
            nextMillis = millis() + SDS30_SETTLE_SECS * 1000; // next task in 30s
        }
        else if ((task >> UPDATE_SENSORS) & 1U) {

            task &= ~(1UL << UPDATE_SENSORS);
            ++cycle %= 3;
            spsState = 0;

            updateSensorData();

            // connect to WiFi as we need to fetch some data
            if (WiFi.status() != WL_CONNECTED) {
                connectWifi();
            }

            if (WiFi.status() == WL_CONNECTED) {
                sendDataToOpenSenseMap();

                if ((task >> UPDATE_FORECAST) & 1U)  {
                    updateWeatherData();
                    task &= ~(1UL << UPDATE_FORECAST);
                }
                // disconnect WiFi
                WiFi.disconnect();
                while (WiFi.status() == WL_CONNECTED) delay(500);
                WiFi.mode(WIFI_OFF);

            }
            // shedule next task
            nextMillis = millis() +  (UPDATE_INTERVAL_SECS - SDS30_SETTLE_SECS) * 1000;
            task |= 1UL << UPDATE_TURN_SDS30_ON;
        }

        // set display brightness depending on time
        now = dstAdjusted.time(&dstAbbrev);
        timeinfo = localtime (&now);

        // turn on/off LCD backlight at night
        if ((timeinfo->tm_hour > 20) || (timeinfo->tm_hour < 6)) { // turn off from 21h - 6h
            ledcWrite(BACKLIGHT_PWM_CHN, BACKLIGHT_PWM_NIGHT);
        }
        else {
            ledcWrite(BACKLIGHT_PWM_CHN, BACKLIGHT_PWM_DAY);
        }

    }

    // get current time and check if the display has to be updated
    now = dstAdjusted.time(&dstAbbrev);
    timeinfo = localtime (&now);

    if (timeinfo->tm_sec != lastSecond) {
        lastSecond = timeinfo->tm_sec;
        // update screen
        gfx.fillBuffer(MINI_BLACK);
        drawTime();
        drawWifiQuality();
        drawForecast(0, 0);
        drawCurrentWeather();
        drawSensorValues();
        gfx.commit();
    }

    delay(100); // wait 100ms
    // using a light sleep here will cause the PWM for the display to halt...
    // esp_sleep_enable_timer_wakeup(100000); //100ms
    // int ret = esp_light_sleep_start();
}

void sendDataToOpenSenseMap() {

    drawProgress(70, "Sending data to OSM...");

    // print time in serial log
    char *dstAbbrev;
    time_t now = dstAdjusted.time(&dstAbbrev);
    struct tm * timeinfo = localtime (&now);

    Serial.println("Send data to OSM: " + String(timeinfo->tm_hour) + ":" + String(timeinfo->tm_min) + ":" + String(timeinfo->tm_sec));

    //create JSON object
    const int capacity = JSON_OBJECT_SIZE(15);
    DynamicJsonDocument json(capacity);

    json[SENSOR1_ID] = String(temp, 2);
    json[SENSOR2_ID] = String(pressure, 2);
    json[SENSOR3_ID] = String(humidity, 2);
    json[SENSOR4_ID] = String(sps30Data.mc_10p0, 2);
    json[SENSOR5_ID] = String(sps30Data.mc_4p0, 2);
    json[SENSOR6_ID] = String(sps30Data.mc_2p5, 2);
    json[SENSOR7_ID] = String(sps30Data.mc_1p0, 2);
    json[SENSOR8_ID] = String(co2);

    // create HTTPS request
    WiFiClientSecure client;
    if (!client.connect("api.opensensemap.org", 443, 30000)) {
        Serial.println("Connection failed");
        //ESP.restart();
    }
    else {
        Serial.println("Connected to OSM server");
        client.println("POST https://api.opensensemap.org/boxes/" SENSEBOX_ID "/data HTTP/1.1");
        client.println("Host: api.opensensemap.org");
        client.println("Connection: close");
        client.print("Content-Length: ");
        client.println(measureJson(json));
        client.println("Content-Type: application/json");
        client.println();
        serializeJson(json, client);

        while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") {
                Serial.println("headers received");
                break;
            }
        }

        // if there are incoming bytes available
        // from the server, read them and print them:
        while (client.available()) {
            char c = client.read();
            Serial.write(c);
        }
        Serial.println();
        client.stop();
    }
}

// Update the sensor data
void updateSensorData() {

    // show progress bar
    drawProgress(10, "Updating sensor readings...");

    // get readings from BME280 sensor
    bme280.takeForcedMeasurement();
    delay(100);
    temp = bme280.readTemperature() - 2; // subtract 2 as dirty workaround -> housing heats up due to display
    pressure = bme280.readPressure() / 100;
    humidity = bme280.readHumidity();

    Serial.print("Updated BME280 data: ");
    Serial.print(temp);
    Serial.print(" ");
    Serial.print(pressure);
    Serial.print(" ");
    Serial.println(humidity);

    // get readngs from SDS30 sensor
    int16_t result = 0;
    uint16_t data_ready = 0;
    /*
        result = sps30_start_measurement();
        if (result < 0) {
            Serial.print("Error starting measurement\n");
        }
        delay(1000); //let sensor settle for 5 s
    */
    do {
        result = sps30_read_data_ready(&data_ready);
        if (result < 0) {
            Serial.print("SDS30: Error reading data-ready flag: ");
            Serial.println(result);
        }
        else if (!data_ready)
            Serial.println("SDS30: Data not ready, no new measurement available");
        else
            break;
        delay(100); /* retry in 100ms */
    } while (1);

    result = sps30_read_measurement(&sps30Data);
    if (result < 0) {
        Serial.println("error reading measurement");
    }

    result = sps30_stop_measurement();
    if (result < 0) {
        Serial.print("Error stopping measurement\n");
    }

    Serial.println("Updated SDS30 data: ");
    Serial.print("PM  1.0: ");
    Serial.println(sps30Data.mc_1p0);
    Serial.print("PM  2.5: ");
    Serial.println(sps30Data.mc_2p5);
    Serial.print("PM  4.0: ");
    Serial.println(sps30Data.mc_4p0);
    Serial.print("PM 10.0: ");
    Serial.println(sps30Data.mc_10p0);
    Serial.print("NC  0.5: ");
    Serial.println(sps30Data.nc_0p5);
    Serial.print("NC  1.0: ");
    Serial.println(sps30Data.nc_1p0);
    Serial.print("NC  2.5: ");
    Serial.println(sps30Data.nc_2p5);
    Serial.print("NC  4.0: ");
    Serial.println(sps30Data.nc_4p0);
    Serial.print("NC 10.0: ");
    Serial.println(sps30Data.nc_10p0);

    Serial.print("Typical partical size: ");
    Serial.println(sps30Data.typical_particle_size);

    // get CO2 data from SCD30
    while (!CO2Sensor.dataAvailable()) {
        delay(500);
    }
    co2 = CO2Sensor.getCO2();
    Serial.print("Updated SCD30 data: ");
    Serial.println(co2);
}


// Update the internet based information and update screen
void updateWeatherData() {

    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);

    drawProgress(10, "Updating time...");
    Serial.println("Updating time...");
    configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
    while (!time(nullptr)) {
        Serial.print("#");
        delay(100);
    }
    // calculate for time calculation how much the dst class adds.
    dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - time(nullptr);
    Serial.printf("Time difference for DST: %d\n", dstOffset);

    drawProgress(50, "Updating conditions...");
    Serial.println("Updating conditions...");
    OpenWeatherMapCurrent *currentWeatherClient = new OpenWeatherMapCurrent();
    currentWeatherClient->setMetric(IS_METRIC);
    currentWeatherClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
    currentWeatherClient->updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
    //currentWeatherClient->updateCurrent(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION);
    delete currentWeatherClient;
    currentWeatherClient = nullptr;

    drawProgress(70, "Updating forecasts...");
    Serial.println("Updating forecasts...");
    OpenWeatherMapForecast *forecastClient = new OpenWeatherMapForecast();
    forecastClient->setMetric(IS_METRIC);
    forecastClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
    uint8_t allowedHours[] = {12, 0};
    forecastClient->setAllowedHours(allowedHours, sizeof(allowedHours));
    forecastClient->updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
    //forecastClient->updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION, MAX_FORECASTS);
    delete forecastClient;
    forecastClient = nullptr;

    delay(1000);
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
    gfx.fillBuffer(MINI_BLACK);
    gfx.setColor(MINI_YELLOW);

    gfx.drawString(0, 146, text);
    gfx.setColor(MINI_WHITE);
    gfx.drawRect(10, 168, 240 - 20, 15);
    gfx.setColor(MINI_BLUE);
    gfx.fillRect(12, 170, 216 * percentage / 100, 11);

    gfx.commit();
}

// Draw the clock
void drawTime() {

    char time_str[11];
    char *dstAbbrev;
    time_t now = dstAdjusted.time(&dstAbbrev);
    struct tm * timeinfo = localtime (&now);

    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_WHITE);
    String date = WDAY_NAMES[timeinfo->tm_wday] + " " + MONTH_NAMES[timeinfo->tm_mon] + " " + String(timeinfo->tm_mday) + " " + String(1900 + timeinfo->tm_year);
    gfx.drawString(120, 6, date);

    gfx.setFont(ArialRoundedMTBold_36);

    if (IS_STYLE_12HR) {
        int hour = (timeinfo->tm_hour + 11) % 12 + 1; // take care of noon and midnight
        sprintf(time_str, "%2d:%02d:%02d\n", hour, timeinfo->tm_min, timeinfo->tm_sec);
        gfx.drawString(120, 20, time_str);
    } else {
        sprintf(time_str, "%2d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        gfx.drawString(120, 20, time_str);
    }

    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setFont(ArialMT_Plain_10);
    gfx.setColor(MINI_BLUE);
    if (IS_STYLE_12HR) {
        sprintf(time_str, "%s\n%s", dstAbbrev, timeinfo->tm_hour >= 12 ? "PM" : "AM");
        gfx.drawString(195, 27, time_str);
    } else {
        sprintf(time_str, "%s", dstAbbrev);
        gfx.drawString(195, 27, time_str);  // Known bug: Cuts off 4th character of timezone abbreviation
    }
}

// draws current weather information
void drawCurrentWeather() {
    gfx.setTransparentColor(MINI_BLACK);
    gfx.drawPalettedBitmapFromPgm(0, 55, getMeteoconIconFromProgmem(currentWeather.icon));
    // Weather Text

    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_BLUE);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(235, 65, currentWeather.cityName);

    gfx.setFont(ArialRoundedMTBold_36);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);

    gfx.drawString(232, 78, String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F"));

    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_YELLOW);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(232, 118, currentWeather.description);

}

void drawForecast(int16_t x, int16_t y) {
    drawForecastDetail(x + 10, y + 165, 0);
    drawForecastDetail(x + 95, y + 165, 1);
    drawForecastDetail(x + 180, y + 165, 2);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
    gfx.setColor(MINI_YELLOW);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    time_t time = forecasts[dayIndex].observationTime + dstOffset;
    struct tm * timeinfo = localtime (&time);
    gfx.drawString(x + 25, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");

    gfx.setColor(MINI_WHITE);
    gfx.drawString(x + 25, y, String(forecasts[dayIndex].temp, 1) + (IS_METRIC ? "°C" : "°F"));

    gfx.drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].icon));
    gfx.setColor(MINI_BLUE);
    gfx.drawString(x + 25, y + 60, String(forecasts[dayIndex].rain, 1) + (IS_METRIC ? "mm" : "in"));
}

void drawSensorValues() {
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 250, "Temp:");
    gfx.drawString(10, 265, "Druck:");
    gfx.drawString(10, 280, "rel. LF:");
    gfx.drawString(10, 295, "CO2:");
    //gfx.drawString(10, 295, "SPS30:");
    gfx.drawString(150, 250, "PM 1.0:");
    gfx.drawString(150, 265, "PM 2.5:");
    gfx.drawString(150, 280, "PM 4.0:");
    gfx.drawString(150, 295, "PM 10.0:");

    gfx.setColor(MINI_WHITE);
    gfx.drawString(80, 250, String(temp, 1) + "°C");
    gfx.drawString(80, 265, String(pressure, 1));
    gfx.drawString(80, 280, String(humidity, 0) + " %");
    gfx.drawString(80, 295, String(co2) + "ppm");
    gfx.drawString(210, 250, String(sps30Data.mc_1p0, 1));
    gfx.drawString(210, 265, String(sps30Data.mc_2p5, 1));
    gfx.drawString(210, 280, String(sps30Data.mc_4p0, 1));
    gfx.drawString(210, 295, String(sps30Data.mc_10p0, 1));
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
    int32_t dbm = WiFi.RSSI();
    if (dbm <= -100) {
        return 0;
    } else if (dbm >= -50) {
        return 100;
    } else {
        return 2 * (dbm + 100);
    }
}

void drawWifiQuality() {
    int8_t quality = getWifiQuality();
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(228, 9, String(quality) + "%");
    for (int8_t i = 0; i < 4; i++) {
        for (int8_t j = 0; j < 2 * (i + 1); j++) {
            if (quality > i * 25 || j == 0) {
                gfx.setPixel(230 + 2 * i, 18 - j);
            }
        }
    }
}
