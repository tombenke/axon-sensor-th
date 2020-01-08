#include <stdio.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoNATS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h>

/* DHT sensor config */
#define DHTPIN  4     // Digital pin connected to the DHT sensor 
#define DHTTYPE DHT22 // DHT 22 (AM2302)

DHT_Unified dht(DHTPIN, DHTTYPE);
//uint32_t delayMS;

/* WiFi config */
/*const char* WIFI_SSID = "SkyNet2";*/
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PSK = "your-wifi-password";

/* NATS config */
const String AXON_ID = "25b95691"; // Unique ID for axon subjects on demo.nats.io that prevents collision
const String DEVICE_ID = "6cfde020-fcd8-493f-9f6c-d8415b4a3fd5"; // Unique device ID
const String TOPIC_UPSTREAM = "axon." + AXON_ID + ".log"; // The prefix and postfix to the subject of incoming messages
const String TOPIC_DOWNSTREAM = "axon." + AXON_ID + ".measure"; // The prefix and postfix to the subject of outgoing messages

WiFiClient client;
NATS nats(
  &client,
/*  "192.168.2.107", NATS_DEFAULT_PORT*/
  "demo.nats.io", NATS_DEFAULT_PORT
);

/*
 * Connect to WiFi network
 */
void connect_wifi() {
  Serial.print("\n\nConnecting to ");
  Serial.println(WIFI_SSID);
  Serial.println();
  Serial.print("Wait for WiFi... ");
    
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
    yield();
  }
  Serial.print("\nWiFi connected: ");
  Serial.println(WiFi.localIP().toString().c_str());
}

/*
 * Do the measurement via the sensor, then send the results to the corresponding channel
 */
void measure(double timestamp, const char*timePrecision) {
  // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  float temperature = event.temperature;
  if (isnan(temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;
  if (isnan(humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }
  
  nats.publishf(TOPIC_UPSTREAM.c_str(),
    "{\"time\":%.0f,\"type\":\"measurement\",\"meta\":{\"timePrecision\":\"%s\"},\"body\":{\"device\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f}}",
    timestamp, timePrecision, DEVICE_ID.c_str(), temperature, humidity);
}

/*
 * Send a response message to the subject given by the `replyTo` parameter,
 * if the request was a synchronous, RPC call.
 * NOTE: Currently not used.
 *
void sendReply(const char* replyTo, const char* response) {
  Serial.print("nats.publish \"");
  Serial.print(response);
  Serial.print(" to ");
  Serial.println(replyTo);
  nats.publish(replyTo, response);
}
*/

/*
 * NATS message handler.
 *
 * Called every time a new message arrives,
 * then calls the corresponding function that do the measurement and manages the response.
 */
void nats_request_handler(NATS::msg msg) {
  Serial.print("data: ");
  Serial.println(msg.data);
  StaticJsonBuffer<1000> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg.data);
  double timestamp = 0.;
  const char* timePrecision = "";
  if(!root.success()) {
    Serial.println("msg.data JSON parsing failed");
  } else {
    timestamp = root["time"];
    timePrecision = root["meta"]["timePrecision"];
  }

  measure(timestamp, timePrecision);
}

/*
 * NATS connection handler
 */
void nats_on_connect() {
  Serial.print("nats connected and subscribe to ");
  Serial.println(TOPIC_DOWNSTREAM.c_str());
  nats.subscribe(TOPIC_DOWNSTREAM.c_str(), nats_request_handler);
}

/*
 * NATS error handler function
 */
void nats_on_error() {
  Serial.print("######## nats error occured ################\n");
}

/*
 * Initialize the DHT sensor
 */
void dhtSetup() {
  // Initialize device.
  dht.begin();
  Serial.println(F("DHTxx Unified Sensor Example"));
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.
  // delayMS = sensor.min_delay / 1000;
}

/*
 * Initial setup of the unit
 */
void setup() {
  /* Setup serial communication for debugging */
  Serial.begin(115200);

  /* Setup wifi connection */
  connect_wifi();

  /* Setup NATS communication */
  nats.on_connect = nats_on_connect;
  nats.on_error = nats_on_error;
  nats.connect();

  /* Setup DHT-22 Thermo and humidity sensor */
  dhtSetup();
}

void loop() {
  /* Check WiFi status, and reconnect if connection would has been lost */
  if (WiFi.status() != WL_CONNECTED) {
    connect_wifi();
  }

  /* Handle incoming NATS messages, and send responses */
  nats.process();
  /*delay(100)*/
  yield(); // Required by NATS library to keep working
}
