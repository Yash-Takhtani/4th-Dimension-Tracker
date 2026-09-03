#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

#define TFT_SCLK 10 // labeled SCL on the screen
#define TFT_MOSI 11 // labeled SDA on the screen
#define TFT_RST -1
#define TFT_DC 5
#define TFT_CS 6
// #define TFT_BL -1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600); 

void setup() {
  Serial.begin(115200);
  tft.init(76, 284);
  // tft.setColRowStart(82, 18);
  // tft.setOffsets(82, 18);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("TFT Initialized!");
  tft.setCursor(0,0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();
}

void loop() {
  timeClient.update();
  tft.fillRect(0, 0, 200, 30, ST77XX_BLACK); 
  tft.setCursor(0, 0);
  tft.print(timeClient.getFormattedTime());
  delay(500);
}
