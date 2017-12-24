#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <ArduinoOTA.h>
#include <OneWire.h>

#define ZONE_CT 4
#define TOPIC_BASE "717"

#define DS1820Pin 10  //GPIO10
OneWire ds(DS1820Pin);

// IP or dns name of mqtt server
const char* mqtt_server = "oregon.redwinos.com";

// don't bother
int LED = D0;
//int wifiResetPin = 10;  //GPIO10
// Don't use D10, interferes with Serial transmit.
int waterPin  = D9;
int boilerFaultPin = D1;
int zonePin[] = {D2, D5, D6, D7};

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
  Serial.println("Starting up...");

  // Set up WiFi manager
  setupWifi();
  
  // Set up MQTT client
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
  Serial.println("MQTT is set up");

  
  pinMode(waterPin,INPUT_PULLUP);
  pinMode(boilerFaultPin, INPUT_PULLUP);
  pinMode(DS1820Pin, INPUT_PULLUP);
    
  waterBounce.attach(waterPin);
  boilerFaultBounce.attach(boilerFaultPin);
  for (i=0; i<=ZONE_CT-1 ; i++) {
    pinMode(zonePin[i], INPUT_PULLUP);
    zoneBounce[i].attach(zonePin[i]);
  }
  Serial.println("Pins are set up");
  
  setupOTA();
  
}

void setupOTA() {
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
  Serial.println("OTA is set up");
}

void setupWifi() {
  delay(10);
  WiFiManager wifiManager;
  //pinMode(wifiResetPin, INPUT_PULLUP);
  // force true, skip network reset
  if ( true ) {      //  digitalRead(wifiResetPin) ) {
    Serial.println("Auto-connect network");
    wifiManager.autoConnect("HotMess");
    Serial.println("Autoconnect success!");
  } else {
    //GPIO "resetNetworkPin" grounded
    Serial.println("Force network reconfig");
    ESP.eraseConfig();
    wifiManager.startConfigPortal("HotMess");
    Serial.println("Network reconfig success!");
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
      for (i=0 ; i<50 ; i++) {
        digitalWrite(BUILTIN_LED, LOW);
        delay(100);
        // make sure we can do updates while trying to connect to MQTT broker
        Serial.println("OTA Handler");
        ArduinoOTA.handle();
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
    snprintf (msg, 75, "hello world %ld", value);
    snprintf(topic, 75, "%s/hello/boiler", TOPIC_BASE);
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
  sendBoiler();
  sendTemp();
}

void sendBoiler(void) {
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

void sendTemp() {
    double tempurature = getTemp();
    dtostrf(tempurature, 5,2, msg);
    Serial.print("Tempurature: ");
    Serial.println(msg);
    snprintf(topic, 50, "%s/status/boiler/temp", TOPIC_BASE);
    client.publish(topic,msg);
}


double getTemp(void) {  
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  char buf[100];
  
  while ( ds.search(addr)) {
  
    Serial.print("ROM = ");
    Serial.println(topic);
    if (OneWire::crc8(addr, 7) != addr[7]) {
        Serial.println("CRC is not valid!");
        return 998;
    }
 
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        Serial.println("  Chip = DS18S20");  // or old DS1820
        type_s = 1;
        break;
      case 0x28:
        //Serial.println("  Chip = DS18B20");
        type_s = 0;
        break;
      case 0x22:
        Serial.println("  Chip = DS1822");
        type_s = 0;
        break;
      default:
        Serial.println("Device is not a DS18x20 family device.");
        return 997;
    } 

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
    delay(1000);     // maybe 750ms is enough, maybe not
    // we might do a ds.depower() here, but the reset will take care of it.
  
    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad

    //Serial.print("  Data = ");
    //Serial.print(present, HEX);
    //Serial.print(" ");
    for ( i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
      //Serial.print(data[i], HEX);
      //Serial.print(" ");
    }
    //Serial.print(" CRC=");
    //Serial.print(OneWire::crc8(data, 8), HEX);
    //Serial.println();

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    celsius = (float)raw / 16.0;
    fahrenheit = celsius * 1.8 + 32.0;
    Serial.print("  Temperature = ");
    Serial.print(celsius);
    Serial.print(" Celsius, ");
    Serial.print(fahrenheit);
    Serial.println(" Fahrenheit");
  }
  Serial.println("No more addresses.");
  Serial.println();
  ds.reset_search();
  delay(250);
  return fahrenheit;
}

unsigned long int nextMillis = 0;
#define shortBlink 50
#define longBlink 4950
void blink() {
  if ( millis()  > nextMillis ) {
    digitalWrite( BUILTIN_LED, !digitalRead(BUILTIN_LED));
    nextMillis = millis() + (digitalRead(BUILTIN_LED) ? longBlink : shortBlink);
  }
}

