#include <Arduino.h>
#include <SPI.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <elapsedMillis.h>
#include <string>

#include <ota.h>

#include "kaaro_utils.cpp"
#include <Preferences.h>

#include <IRremote.h>

/* 
    STATICS
*/
const char *mqtt_server = "api.akriya.co.in";
const uint16_t WAIT_TIME = 1000;
#define BUF_SIZE 75

String ROOT_MQ_ROOT = "malboro/";
String PRODUCT_MQ_SUB = "ir/";
String MESSAGE_MQ_STUB = "message";
String COUNT_MQ_STUB = "count";
String OTA_MQ_SUB = "ota/";

/* 
    FUNCTION DEFINATIONS
*/

void mqttCallback(char *topic, byte *payload, unsigned int length);
void blasterLoop();
/* 
 REALTIME VARIABLES
*/

String host = "ytkarta.s3.ap-south-1.amazonaws.com"; // Host => bucket-name.s3.region.amazonaws.com
int port = 80;                                       // Non https. For HTTPS 443. As of today, HTTPS doesn't work.
String bin = "/Malboro/ir-recvr/firmware.bin";       // bin file name with a slash in front.

char mo[75];
String msg = "";
String DEVICE_MAC_ADDRESS;
String ssid = "";
String pass = "";

byte mac[6];

int cases = 1;

int fxMode;

uint32_t target_counter = 0;

unsigned long delayStart = 0; // the time the delay started
bool delayRunning = false;
unsigned int interval = 30000;

/*
  HY Variable/Instance creation
*/

WiFiClient wifiClient;
PubSubClient mqttClient(mqtt_server, 1883, mqttCallback, wifiClient);
WiFiManager wifiManager;
WiFiClientSecure client;

elapsedMillis timeElapsed;
Preferences preferences;
#define convertToString(x) #x

int RECV_PIN = 15;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  char *cleanPayload = (char *)malloc(length + 1);
  payload[length] = '\0';
  memcpy(cleanPayload, payload, length + 1);
  msg = String(cleanPayload);
  free(cleanPayload);

  String topics = String(topic);
  Serial.print("From MQTT = ");
  Serial.println(msg);

  String rootTopic = ROOT_MQ_ROOT;
  String readyTopic = ROOT_MQ_ROOT + DEVICE_MAC_ADDRESS;

  String otaTopic = ROOT_MQ_ROOT + OTA_MQ_SUB + DEVICE_MAC_ADDRESS;

  String productMessageTopic = ROOT_MQ_ROOT + PRODUCT_MQ_SUB + MESSAGE_MQ_STUB;
  String productCountTopic = ROOT_MQ_ROOT + PRODUCT_MQ_SUB + COUNT_MQ_STUB;

  String messageTopic = ROOT_MQ_ROOT + MESSAGE_MQ_STUB + '/' + DEVICE_MAC_ADDRESS;
  // String countTopic = ROOT_MQ_ROOT + COUNT_MQ_STUB + DEVICE_MAC_ADDRESS;

  if (topics == otaTopic && msg == "ota")
  {
    Serial.println("Ota Initiating.........");

    OTA_ESP32::execOTA(host, port, bin, &wifiClient);
  }

  else if (topics == "malboro/ota/version")
  {
  }

  if (topics == rootTopic)
  {
    //;;;
  }
  if (topics == productCountTopic)
  {
    Serial.println(msg + " | From count topic");

    preferences.putUInt("target_counter", 0);
  }

  if (topics == productMessageTopic || topics == messageTopic)
  {
    Serial.println(msg + " | From message topic");
    //;;;;
  }
  if (topics == "activate/now")
  {
    Serial.println("Enabling IRin");
    // irrecv.enableIRIn(); // Start the receiver
    Serial.println("Enabled IRin");
  }
}

void reconnect()
{

  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");

      String rootTopic = ROOT_MQ_ROOT;
      String readyTopic = ROOT_MQ_ROOT + DEVICE_MAC_ADDRESS;

      String otaTopic = ROOT_MQ_ROOT + OTA_MQ_SUB + DEVICE_MAC_ADDRESS;

      String productMessageTopic = ROOT_MQ_ROOT + PRODUCT_MQ_SUB + MESSAGE_MQ_STUB;
      String productCountTopic = ROOT_MQ_ROOT + PRODUCT_MQ_SUB + COUNT_MQ_STUB;

      String messageTopic = ROOT_MQ_ROOT + MESSAGE_MQ_STUB + DEVICE_MAC_ADDRESS;
      String countTopic = ROOT_MQ_ROOT + COUNT_MQ_STUB + DEVICE_MAC_ADDRESS;

      String readyMessage = DEVICE_MAC_ADDRESS + " is Ready.";
      mqttClient.publish(readyTopic.c_str(), "Ready!");
      mqttClient.publish(rootTopic.c_str(), readyMessage.c_str());

      mqttClient.subscribe(rootTopic.c_str());
      mqttClient.subscribe(otaTopic.c_str());

      mqttClient.subscribe(productMessageTopic.c_str());
      mqttClient.subscribe(productCountTopic.c_str());

      mqttClient.subscribe(messageTopic.c_str());
      mqttClient.subscribe(countTopic.c_str());
      mqttClient.subscribe("activate/now");
    }

    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}

void setup()
{

  Serial.begin(115200);
  DEVICE_MAC_ADDRESS = KaaroUtils::getMacAddress();
  Serial.println(DEVICE_MAC_ADDRESS);
  WiFi.macAddress(mac);
  preferences.begin("malboro", false);
  pinMode(mb_pin, OUTPUT);
  char str[100];
  sprintf(str, "%d", target_counter);
  String s = str;
  Serial.print("Connecting Wifi: ");
  wifiManager.setConnectTimeout(5);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setWiFiAutoReconnect(true);
  wifiManager.autoConnect("Digital Icon");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);

    ssid = WiFi.SSID();
    pass = WiFi.psk();
  }

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
}

void loop()
{

  wifiManager.process();

  if (WiFi.status() == WL_CONNECTED)
  {

    if (!mqttClient.connected())
    {
      reconnect();
    }
  }
  mqttClient.loop();
  blasterLoop()
  // if (irrecv.decode(&results)
}

const int mb_pin = 16;

void onEdge() {
  digitalWrite(mb_pin, HIGH);
  delayMicroseconds(13);
  digitalWrite(mb_pin, LOW);
  delayMicroseconds(13);
}

void offEdge() {
  digitalWrite(mb_pin, LOW);
  delayMicroseconds(26);
}
void blasterLoop()
{
  for(int i=0;i<7;i++) {
    onEdge();
  }
  for(int i=0;i<10;i++) {
    offEdge();
  }
}
