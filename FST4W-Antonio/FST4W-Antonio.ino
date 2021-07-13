// Huge thanks to https://github.com/f4goh/FST4W for the original FST4W work!

#include <Arduino.h>

#include <JTEncode.h>
#include <FS.h>
#include <Wire.h>
#include <RTClib.h>
#include <si5351.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <rs_common.h>
#include <int.h>
#include <string.h>
#include "Wire.h"
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include "SSD1306Wire.h"
#include "MyFont.h"

#include "credentials.h"

// Global defines
#define PTT_PIN 14 // D5

// Digital mode properties
#define FST4W_TONE_SPACING  146 // ~1.46 Hz
#define FST4W_DELAY         683 // Delay value for FST4W
// #define FST4W_DEFAULT_FREQ  14097050UL  // 20 meter band for testing
#define FST4W_DEFAULT_FREQ  473450UL // 472 KHz band (630m band)! Tune receiver to 472.000 KHz USB!
#define FST4W_MODE          0
#define FSTW4_INTERVAL      120
#define FST4W_SYMBOL_COUNT  160

// fst4sim "IK1HGI JN45 20" 60 1500 0.0 0.1 1.0 10 -15 F
const uint8_t  FST4Wsymbols[FST4W_SYMBOL_COUNT] = { 0, 1, 3, 2, 1, 0, 2, 3, 3, 0, 3, 3, 0, 3, 1, 0, 0, 2, 2, 1, 3, 1, 3, 0, 3, 1, 0, 1, 0, 3, 1, 3, 0, 0, 3, 0, 1, 1, 2, 3, 1, 0, 3, 2, 0, 1, 0, 3, 1, 2, 1, 2, 3, 2, 1, 1, 0, 0, 1, 3, 3, 1, 3, 0, 0, 2, 3, 1, 2, 0, 3, 0, 2, 2, 1, 2, 0, 1, 3, 2, 1, 0, 2, 3, 0, 3, 0, 3, 2, 2, 0, 1, 0, 2, 0, 1, 0, 0, 2, 2, 3, 1, 3, 0, 2, 1, 1, 3, 0, 1, 0, 3, 2, 2, 2, 3, 1, 0, 3, 2, 0, 1, 0, 0, 3, 2, 1, 2, 0, 2, 2, 0, 3, 0, 0, 1, 3, 3, 3, 0, 0, 0, 1, 3, 2, 1, 1, 0, 0, 2, 0, 2, 0, 1, 3, 2, 1, 0, 2, 3 };

enum OperatingModes
{
  MODE_FST4W,
};

const String operatingModeTexts[] = {
  "FST4W",
};

// Class instantiations
boolean txEnabled = false;
SSD1306Wire display(0x3c, SDA, SCL);
RTC_DS3231 rtc;
Si5351 si5351;
DateTime dt;
JTEncode jtencode;
unsigned long freq;
uint8_t tx_buffer[255];  // NOTE
int rtc_lost_power = 0;
int vfo_ok = 1;
const int ledPin = LED_BUILTIN;
enum modes
{
  FST4W,
};
enum modes mode = FST4W; // Note
OperatingModes operatingMode = MODE_FST4W;   // default op mode is CW
uint64_t frequency = FST4W_DEFAULT_FREQ * 100ULL;
int32_t si5351CalibrationFactor = 16999;  // si5351 calibration factor
uint8_t symbolCount;
uint16_t toneDelay, toneSpacing;

// Loop through the string, transmitting one character at a time.
void jtTransmitMessage()
{
  uint8_t i;

  Serial.println("TX!");
  // Reset the tone to the base frequency and turn on the output
  si5351.set_clock_pwr(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK0, 1);
  txEnabled = true;
  updateDisplay();
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PTT_PIN, HIGH);// Note

  for (i = 0; i < symbolCount; i++)
  {
    si5351.set_freq(frequency + (FST4Wsymbols[i] * toneSpacing), SI5351_CLK0);
    delay(toneDelay);
  }

  // Turn off the output
  si5351.set_clock_pwr(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK0, 0);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_BUILTIN, HIGH);
  txEnabled = false;
  updateDisplay();

}

String getTemperature()
{
  return String(rtc.getTemperature());
}

String getTime()
{
  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);

  return date;
}

// debug helper
void led_flash()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
}

void set_mode()
{
  frequency = FST4W_DEFAULT_FREQ * 100ULL;;
  symbolCount = FST4W_SYMBOL_COUNT;
  toneSpacing = FST4W_TONE_SPACING;
  toneDelay = FST4W_DELAY;
  operatingMode = MODE_FST4W;
  Serial.println("Getting ready for FST4W...");
}

void sync_time()
{
  const long utcOffsetInSeconds = (5.5 * 3600);
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

  WiFi.hostname("beacon");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println(WiFi.localIP());
  timeClient.begin();


  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  rtc.adjust(now);
  led_flash();
}


void updateDisplay()
{
  display.clear();
  display.setFont(Roboto_Mono_Thin_16);
  display.drawString(0, 0, String((double)frequency / 100000000, 6U) + "MHz");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 20, "Mode: Standalone Beaon");
  display.drawString(0, 30, "OpMode: FST4W");
  display.drawString(0, 50, "TxEnabled: " + String(txEnabled ? "true" : "false"));
  display.display();
}

void setup()
{
  int ret = 0;
  char date[10] = "hh:mm:ss";

  mode == FST4W;
  freq = FST4W_DEFAULT_FREQ;

  // Safety first!
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW);

  // Setup serial and IO pins
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // OFF
  delay(2000);
  Serial.println("\n...");

  // Initialize the Si5351
  ret = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, si5351CalibrationFactor);
  if (ret != true) {
    Serial.flush();
    vfo_ok = 0;
  }
  si5351.set_clock_pwr(SI5351_CLK0, 0); // safety first
  Serial.print("Si5351 init status (should be 1 always) = ");
  Serial.println(ret);

  // Initialize the rtc
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    Serial.flush();
    abort();
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, abort!?");
    Serial.flush();
    rtc_lost_power = 1;
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // hack!
    // abort();  // NOTE
  }
  rtc.disable32K();
  rtc.now().toString(date);

  // Print status
  Serial.print("Current time is = ");
  Serial.println(date);
  Serial.print("Temperature is: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  // Set CLK0 output
  si5351.set_freq(freq * 100, SI5351_CLK0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // Set for maximum power
  // delay(10000); // Keep TX on for 5 seconds for tunining purposes.
  si5351.set_clock_pwr(SI5351_CLK0, 0); // Disable the clock initially

  // Note
  set_mode();

  // Sanity checks
  if (!vfo_ok) {
    Serial.println("Check VFO connections!");
    led_flash();
    delay(50);
  }
  if (rtc_lost_power) {
    Serial.println("Check and set RTC time!");
    led_flash();
    delay(50);
  }

  // Initialising the UI
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  updateDisplay();

  digitalWrite(ledPin, HIGH); // OFF
}

// main loop
void loop()
{
  char c;

  if (Serial.available() > 0) {
    c = Serial.read();
    Serial.println(c);
    if (c == 't') {
      jtTransmitMessage();
    }
    if (c == 's') {
      jtTransmitMessage();
    }

  }

  dt = rtc.now();
  if (dt.second() == 0 && (dt.minute() % 2) == 0 && mode == FST4W) {
    jtTransmitMessage();
  }
  delay(10);
}

// end of loop
