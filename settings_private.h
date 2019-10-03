
// private setting which should not be part of the git repo

// Setup
#define WIFI_SSID1 "SSID1"
#define WIFI_PASS1 "PASS1"
#define WIFI_SSID2 "SSID2"
#define WIFI_PASS2 "PASS2"
#define WIFI_HOSTNAME "My Weather Station"

// OpenSenseMap Settings
#define SENSEBOX_ID "BOX_ID" //senseBox ID
#define SENSOR1_ID "SENSOR_ID" // temp
#define SENSOR2_ID "SENSOR_ID" // pressure
#define SENSOR3_ID "SENSOR_ID" // humidity
#define SENSOR4_ID "SENSOR_ID" // PM10
#define SENSOR5_ID "SENSOR_ID" // PM4
#define SENSOR6_ID "SENSOR_ID" // PM2.5
#define SENSOR7_ID "SENSOR_ID" // PM1

// OpenWeatherMap Settings
// Sign up here to get an API key: https://docs.thingpulse.com/how-tos/openweathermap-key/
String OPEN_WEATHER_MAP_APP_ID = "API_ID";
/*
    Go to https://openweathermap.org/find?q= and search for a location. Go through the
    result set and select the entry closest to the actual location you want to display
    data for. It'll be a URL like https://openweathermap.org/city/2657896. The number
    at the end is what you assign to the constant below.
*/
String OPEN_WEATHER_MAP_LOCATION_ID = "CITY_ID";
String DISPLAYED_CITY_NAME = "CITY";

/*
    Arabic -> ar, Bulgarian -> bg, Catalan -> ca, Czech -> cz, German -> de, Greek -> el,
    English -> en, Persian (Farsi) -> fa, Finnish -> fi, French -> fr, Galician -> gl,
    Croatian -> hr, Hungarian -> hu, Italian -> it, Japanese -> ja, Korean -> kr,
    Latvian -> la, Lithuanian -> lt, Macedonian -> mk, Dutch -> nl, Polish -> pl,
    Portuguese -> pt, Romanian -> ro, Russian -> ru, Swedish -> se, Slovak -> sk,
    Slovenian -> sl, Spanish -> es, Turkish -> tr, Ukrainian -> ua, Vietnamese -> vi,
    Chinese Simplified -> zh_cn, Chinese Traditional -> zh_tw.
*/

String OPEN_WEATHER_MAP_LANGUAGE = "de";
