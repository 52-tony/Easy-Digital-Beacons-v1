# https://github.com/arduino/arduino-cli/releases

port := $(shell python3 board_detect.py)

default:
	arduino-cli compile --fqbn=esp8266:esp8266:d1_mini_clone FST4W-Antonio
	arduino-cli compile --fqbn=esp8266:esp8266:d1_mini_clone Easy-Digital-Beacons-v4

upload:
	@# echo $(port)
	arduino-cli compile --fqbn=esp8266:esp8266:d1_mini_clone Easy-Digital-Beacons-v4
	arduino-cli -v upload -p "${port}" --fqbn=esp8266:esp8266:d1_mini_clone Easy-Digital-Beacons-v4

install_platform:
	arduino-cli config init
	arduino-cli core update-index
	arduino-cli core install esp8266:esp8266

deps:
	arduino-cli lib install "ESP8266 and ESP32 OLED driver for SSD1306 displays"
	arduino-cli lib install "Etherkit JTEncode"
	arduino-cli lib install "Etherkit Si5351"
	arduino-cli lib install "RTClib"
	arduino-cli lib install "Time"
	arduino-cli lib install "NTPClient"
	arduino-cli lib install "uEEPROMLib"
	# arduino-cli lib install "ESPAsyncWebServer"
	# arduino-cli lib install "ESPAsyncTCP"


install_arduino_cli:
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
