# Home Manager

## Purpose
A specific solution to the problem of needing to make sure the house
doesn't freeze up while travelling. Also, keep an eye on water use and
possibly water level

## Plan
* NodeMCU watches the end switches on the boiler's zone valves.
* NodeMCU watches for pulses from the newly installed water meter. Each rising edge indicates 0.1 cubic foot of water has passed through the meter. Falling edges indicate the meter is roughly 1/2 way through a 0.1cf pulse cycle, so we have about a .748/2 or .374 gallon detection.
* Hopefully the meter is sufficiently accurate at low flow rates to detect small leaks.
* NodeMCU watches either the NO or NC boiler reset signal.
* Would like to figure out a way to detect when boiler is running.
* Would like to figure out a way to detect when domestic hot water loop is running.
* Detected signals are pushed via MQTT to an Amazon Lightsail instance for processing/storage/user notification and other interesting things to be determined.
* Store raw messages in MySQL database for potential future reprocessing as project develops and for testing.
* Develop a set of RRD databases to store zone valve states, water usage and boiler resets. 
* Use OpenHAB or similar to watch for abnormalities and send notifications. Or redevelop that functionality from scratch.

## Requirements
* Add the ESP8266 Arduino platform
* Add the WiFiManager, PubSubClient, and Bounce2 libraries
