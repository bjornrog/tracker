all: upload

.PHONY: compile upload release

SERIAL_PORT := /dev/cu.usbserial-0001
FQBN := esp8266:esp8266:nodemcuv2
BAUD_RATE := 115200
VERSION_FILE := VERSION
PREV_VERSION := $(shell cat ${VERSION_FILE})
NEXT_VERSION := $(shell expr $(PREV_VERSION) + 1)


#build: setup.html tracker.html
#	./make.sh

compile: tracker.ino
	arduino-cli compile --output-dir ./bin --fqbn $(FQBN) .

upload: compile littlefs.bin
	arduino-cli upload -v --input-dir ./bin --port $(SERIAL_PORT) --fqbn $(FQBN) .
	esptool.py --chip ESP8266 --port $(SERIAL_PORT) --baud $(BAUD_RATE) write_flash -z 0x200000 littlefs.bin
	arduino-cli monitor --port $(SERIAL_PORT) -c baudrate=115200

release:
	echo "#define VERSION $(NEXT_VERSION)" > version.h
	echo "$(NEXT_VERSION)" > VERSION
	make compile
	make littlefs.bin
	mkdir -p release
	cp data/* release/
	cp bin/* release/
	cp littlefs.bin release/

# https://github.com/espressif/esp-idf/blob/v4.3/components/spiffs/spiffsgen.py
# https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/storage/spiffs.html

# pip3 install esptool

# https://github.com/igrr/mkspiffs

# http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html
# So assuming 1 or 3 MB filesystem size

# https://github.com/esp8266/arduino-esp8266fs-plugin/issues/51

# SPIFFS usage in the program:
# http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html

# https://nodemcu.readthedocs.io/en/release/spiffs/#technical-details
spiffs.bin: data/tracker.html data/setup.html data/status.js
	./mkspiffs -c data -b 8192 -p 256 -s 0x100000 spiffs.bin

# 1024000 = 1MB
littlefs.bin: data/tracker.html.gz data/setup.html.gz data/status.js.gz data/def.css.gz data/wifi.json data/setup.js.gz
	./mklittlefs -c data -d 5 -b 8192 -p 256 -s 1024000 littlefs.bin

data/wifi.json: src/wifi.json
	cp src/wifi.json data/wifi.json

data/setup.html.gz: src/setup.html
	gzip src/setup.html -c > data/setup.html.gz

data/setup.js.gz: src/setup.js
	gzip src/setup.js -c > data/setup.js.gz

data/tracker.html.gz: src/tracker.html
	gzip src/tracker.html -c > data/tracker.html.gz

data/status.js.gz: src/status.js
	gzip src/status.js -c > data/status.js.gz

data/def.css.gz: src/def.css
	gzip src/def.css -c > data/def.css.gz


#%.gz: src/%
#	gzip $< -c data/`basename $<`.gz


# https://github.com/earlephilhower/mklittlefs/releases
# LittleFS block size:8192
# LittleFS total bytes:2072576

#    LittleFS.info(info);
#    Serial.print("LittleFS block size:");
#    Serial.println(info.blockSize);
#    Serial.print("LittleFS total bytes:");
#    Serial.println(info.totalBytes);


