#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define BLUE_LED_PIN  8
#define NEOPIXEL_PIN  2

#define BLUE_BLINK_INTERVAL  150
#define NEO_COLOR_INTERVAL   30

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

unsigned long prevBlueLed = 0;
unsigned long prevNeoColor = 0;
bool blueLedOn = false;
uint16_t neoHue = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("YAY! It's working!");

  pinMode(BLUE_LED_PIN, OUTPUT);

  pinMode(NEOPIXEL_PIN, OUTPUT);
  delay(10);
  neo.begin();
  neo.setBrightness(120);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(30);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(6, 22, "YAY");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 36, "It's working!");
  u8g2.sendBuffer();
}

void loop() {
  unsigned long now = millis();

  if (now - prevBlueLed >= BLUE_BLINK_INTERVAL) {
    prevBlueLed = now;
    blueLedOn = !blueLedOn;
    digitalWrite(BLUE_LED_PIN, blueLedOn ? LOW : HIGH);
  }

  if (now - prevNeoColor >= NEO_COLOR_INTERVAL) {
    prevNeoColor = now;
    neoHue += 512;
    uint32_t color = neo.gamma32(neo.ColorHSV(neoHue, 255, 255));
    neo.setPixelColor(0, color);
    neo.show();
  }
}
