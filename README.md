# Tracker

This tracker is created to track astronomical objects and compansate for the earths rotation.
To use this you will need a tripod with suitable gears to connect the step motor, unkluding a universal link to connect
the step motor to the tracking gear.

Software is for the ESP8266/NodeMCU.

It will require some hardware, including:
* Stepper motor 28BYJ-48
* ULN2003 driver board
* Joystick
* Various cables

Features of the software:
* WiFi connection, either to your local network or fall back to access point mode
* Web interface where most aspects can be configured
* Tracking of astronomical objects
* Using the joystick to manually turn the gear
* Intervalometer, with support for:
  - Specifying exposure time
  - Specifying wait time (pause between pictures)
  - Number of pictures
  - Estimat of finish time
  - Progress indicator
* Separate setup page with WiFi configuration, default track speed and firmware maintenance
* Possibility to use for timelaps videos

The software is still under development, and much more documentation is needed.

I hope to update the respository with:
* Schematics for connecting the components
* Pointers to buying the needed components
* Information on connecting the step motor to the gears of the tripod
* Screenshots and pictures

The tracker currently seem to make it possible to take 60 second long expsures, using a 300mm lens.

The intervalometer has only been tested with a Nikon D7100 camera.

# Hardware

This is a draft drawing, sismilar to my test hardware:

![Schematic](/img/tracker.png)

# Development

Prerequisite:

* https://github.com/earlephilhower/mklittlefs

Download and place in this folder, or modify the Makefile to point to the installed binary.


```sh
brew install arduion-cli
cd ~/
arduino-cli config init
Config file written to: /Users/bjorn/Library/Arduino15/arduino-cli.yaml
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

arduino-cli board listall | grep -i 'nodemcu'

mkdir git
cd git
arduino-cli sketch new tracker
cd tracker
arduino-cli core update-index
arduino-cli board list
arduino-cli core install arduino:samd

arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 .
arduino-cli upload --port /dev/cu.usbserial-0001 --fqbn esp8266:esp8266:nodemcuv2 .

cd ~/git/
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 tracker
arduino-cli upload --port /dev/cu.usbserial-0001 --fqbn esp8266:esp8266:nodemcuv2 tracker

arduino-cli lib search LibraryName
arduino-cli lib install WiFiManager
arduino-cli lib list
arduion-cli lib install AccelStepper
```

