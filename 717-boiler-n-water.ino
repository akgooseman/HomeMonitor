#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <ArduinoOTA.h>

#define ZONE_CT 4
#define TOPIC_BASE "717"

// IP or dns name of mqtt server
const char* mqtt_server = "54.149.169.61";

int LED = D0;
int wifiReset = D1;
int waterPin  = D10;
int boilerFaultPin = D3;
int zonePin[] = {D5, D6, D7, D4};

Bounce waterBounce;
Bounce boilerFaultBounce;
Bounce zoneBounce[] = {
  Bounce(zonePin[0], 50),
  Bounce(zonePin[1], 50),
  Bounce(zonePin[2], 50),
  Bounce(zonePin[3], 50),  
};

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
char topic[50];
int value = 0;
int i = 0;           // general use index

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(9600);
  // Set up WiFi manager
  WiFiManager wifiManager;
  wifiManager.autoConnect("HotMess");

  // Set up MQTT client
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(waterPin,INPUT_PULLUP);
  
  waterBounce.attach(waterPin);
  boilerFaultBounce.attach(boilerFaultPin);
  for (i=0; i<=ZONE_CT-1 ; i++) {
    pinMode(zonePin[i], INPUT_PULLUP);
    zoneBounce[i].attach(zonePin[i]);
  }
  
  ArduinoOTA.setHostname("717Boiler");
  ArduinoOTA.onStart([]() {
    Serial.println("Update start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nUpdate dnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    //hostname += String(ESP.getChipId(), HEX);
    snprintf(msg, 75, "%s-%X", TOPIC_BASE, ESP.getChipId());
    if (client.connect(msg)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(TOPIC_BASE, "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for (i=0 ; i<5 ; i++) {
        digitalWrite(BUILTIN_LED, LOW);
        delay(100);
        digitalWrite(BUILTIN_LED, HIGH);
      }
    }
  }
}

char* status="";
void loop() {
  blink();
  ArduinoOTA.handle();
  waterBounce.update();
  boilerFaultBounce.update();
  for (i=0; i<=ZONE_CT-1; i++) {
    zoneBounce[i].update();
  }
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "hello world #%ld", value);
    snprintf(topic, 75, "%s/hello", TOPIC_BASE);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(topic, msg);
    sendStatus();
  }

  if ( waterBounce.fallingEdge() ) {
    snprintf(topic,50,"%s/event/water", TOPIC_BASE);
    Serial.print(topic);
    Serial.println(" Ding!");
    client.publish(topic, "ding");
  }
  if ( waterBounce.risingEdge() ) {
    snprintf(topic,50,"%s/event/water", TOPIC_BASE);
    Serial.print(topic);
    Serial.println(" Dong!");
    client.publish(topic, "dong");
  }
   if ( boilerFaultBounce.fallingEdge() ) {
    snprintf(topic,50,"%s/event/boiler", TOPIC_BASE);
    Serial.print(topic);
    Serial.println(" DOWN!");
    client.publish(topic, "DOWN");
  }
  if ( boilerFaultBounce.risingEdge() ) {
    snprintf(topic,50,"%s/event/boiler", TOPIC_BASE);
    Serial.print(topic);
    Serial.println(" Up!");
    client.publish(topic, "UP");
  }
  for (i=0; i<=ZONE_CT-1; i++) {
    snprintf(topic,50,"%s/event/zone/%ld",TOPIC_BASE, i);
    if ( zoneBounce[i].fallingEdge() ) {
      Serial.print("Zone ");
      Serial.print(i);
      Serial.println(" opening.");
      client.publish(topic, "opening");
    }
    if ( zoneBounce[i].risingEdge() ) {
      Serial.print("Zone ");
      Serial.print(i);
      Serial.println(" closing.");
      client.publish(topic, "closing");
    }
    if ( zoneBounce[i].read() ) {
      status = "On  ";
    } else {
      status = "Off ";
    }
    //Serial.print(status);
  }
  //Serial.println();
}

void sendStatus() {
  // send boiler status on demand
  snprintf(topic, 50, "%s/status/boiler/fault", TOPIC_BASE);
  if ( boilerFaultBounce.read() == true ) {
    client.publish(topic,"FAULTED");
  } else {
    client.publish(topic,"OK");
  }

  for (i=0 ; i<=ZONE_CT-1 ; i++) {
    snprintf(topic,50,"%s/status/zone/%ld", TOPIC_BASE, i);
    if ( zoneBounce[i].read() == false ){
      client.publish(topic,"On");
    } else {
      client.publish(topic,"Off");
    }
  }
}

unsigned long int nextMillis = 0;
#define shortBlink 50
#define longBlink 4950
void blink() {
  if ( millis()  > nextMillis ) {
    //if ( digitalRead(BUILTIN_LED) ) {
    //  digitalWrite(BUILTIN_LED, false);
    //  nextMillis = millis() + shortBlink;
    //} else {
    //  digitalWrite(BUILTIN_LED, true);
    //  nextMillis = millis() + longBlink;
    //}
    digitalWrite( BUILTIN_LED, !digitalRead(BUILTIN_LED));
    nextMillis = millis() + (digitalRead(BUILTIN_LED) ? longBlink : shortBlink);
  }
}

