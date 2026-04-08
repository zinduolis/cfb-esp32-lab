#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define BLUE_LED_PIN  8
#define NEOPIXEL_PIN  2

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

int state = 0;
int frame = 0;
unsigned long lastUpdate = 0;
unsigned long prevBlueLed = 0;
bool blueLedOn = false;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Starting Animation loops...");

  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);

  pinMode(NEOPIXEL_PIN, OUTPUT);
  delay(10);
  neo.begin();
  neo.setBrightness(120);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);
}

void printCenter(int y, const char* text) {
  int w = u8g2.getStrWidth(text);
  u8g2.drawStr((72 - w) / 2, y, text);
}

void loop() {
  unsigned long now = millis();

  // Blue LED blink from old sketch
  if (now - prevBlueLed >= 150) {
    prevBlueLed = now;
    blueLedOn = !blueLedOn;
    digitalWrite(BLUE_LED_PIN, blueLedOn ? LOW : HIGH);
  }

  if (now - lastUpdate < 33) return; // ~30 fps update rate for OLED
  lastUpdate = now;

  u8g2.clearBuffer();

  switch(state) {
    case 0: { // Drop and bounce "HI"
      u8g2.setFont(u8g2_font_ncenB10_tr);
      int y = 0;
      if (frame < 6) y = frame * 6; // up to 30
      else if (frame < 9) y = 30 - (frame - 5) * 3; // up to 21
      else if (frame < 12) y = 21 + (frame - 8) * 2; // down to 27
      else y = 26; // settle
      
      printCenter(y, "HI");

      frame++;
      if (frame > 30) { state = 1; frame = 0; }
      break;
    }
    case 1: { // Grow to HELLO
      if (frame < 4) {
        u8g2.setFont(u8g2_font_ncenB12_tr);
        printCenter(26, "HI!");
      } else if (frame < 8) {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        printCenter(27, "HEL");
      } else if (frame < 30) {
        u8g2.setFont(u8g2_font_ncenB18_tr); 
        printCenter(29, "HELLO");
      } else {
        state = 2; frame = 0;
      }
      frame++;
      break;
    }
    case 2: { // Shrink HELLO
      if (frame < 5) {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        printCenter(28, "HELLO");
      } else if (frame < 10) {
        u8g2.setFont(u8g2_font_ncenB12_tr);
        printCenter(27, "HELLO");
      } else if (frame < 20) {
        u8g2.setFont(u8g2_font_ncenB10_tr);
        printCenter(26, "HELLO");
      } else {
        state = 3; frame = 0;
      }
      frame++;
      break;
    }
    case 3: { // Orbit / Spin
      u8g2.setFont(u8g2_font_ncenB10_tr);
      float angle = frame * 0.25;
      int radiusX = 12; // wider orbit
      int radiusY = 6;
      
      int cx = 36;
      int cy = 20;

      int x = cx + cos(angle) * (radiusX);
      int y = cy + sin(angle) * (radiusY) + 5; 

      int w = u8g2.getStrWidth("HELLO");
      u8g2.drawStr(x - w/2, y, "HELLO");

      frame++;
      if (frame > 60) { state = 4; frame = 0; }
      break;
    }
    case 4: { // Drop off screen
      u8g2.setFont(u8g2_font_ncenB10_tr);
      int y = 26 + (frame * 5); // Fall fast
      printCenter(y, "HELLO");
      frame++;
      if (y > 60) { state = 0; frame = 0; delay(500); }
      break;
    }
  }

  u8g2.sendBuffer();

  // Rainbow pulsing LED
  neo.setPixelColor(0, neo.gamma32(neo.ColorHSV((now / 2) % 65535, 255, 120)));
  neo.show();
}
