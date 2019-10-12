# ESP32 Weather Station

forked from [ThingPulse/esp8266-weather-station-color](https://github.com/ThingPulse/esp8266-weather-station-color)

modified to 
- support Adafruit HUZZAH32 in combination with TFT FeatherWing - 2.4" (no touch screen support)
- Openweather only
- local sensors (BME280 and SPS30)
- uploading sensor data to OpenSenseMap


## Hardware Requirements

[HUZZAH32](https://www.adafruit.com/product/3405)

[TFT FeatherWing - 2.4](https://www.adafruit.com/product/3315) 

[BME280 Sensor breakout - I used the GY-BME280 from az-delivery] 

[Sensirion SPS30 PM Sensor] (https://www.sensirion.com/en/environmental-sensors/particulate-matter-sensors-pm25/)

[3d printed enclosure, remixed to fit the HW](https://www.thingiverse.com/thing:3894654)

## Setup

You can pretty much follow the guide from Adafruit here: [Adafruit Wifi Weather Station](https://learn.adafruit.com/wifi-weather-station-with-tft-display/overview) and use the code provided here.

Basically the following things are to be setup once you are able to compile the code:

Add your individual information in the settings_private.h:
- Wifi credential (you can configure 2 networks)
- Openweather account data, desired city, language
- OpenSenseMap account data (sensebox ID and sensor IDs)

Have fun!




