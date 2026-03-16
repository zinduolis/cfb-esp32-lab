#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define BLUE_LED_PIN  8
#define NEOPIXEL_PIN  2
#define BOOT_BTN_PIN  9

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// State variables
bool isStopped = false;
unsigned long lastBtnPress = 0;
bool lastBtnState = HIGH;

// Matrix effect variables
#define NUM_COLUMNS 14
int columns[NUM_COLUMNS];
unsigned long lastMatrixUpdate = 0;

// LED variables
unsigned long flashInterval = 1000;
unsigned long lastFlashTime = 0;
bool ledState = false;
unsigned long lastSpeedIncrease = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Starting Matrix Effect...");

  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  pinMode(NEOPIXEL_PIN, OUTPUT);
  delay(10);
  neo.begin();
  neo.setBrightness(120);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(30);

  // Initialize columns with random negative heights
  for (int i = 0; i < NUM_COLUMNS; i++) {
    columns[i] = random(-40, 0);
  }
}

void loop() {
  unsigned long now = millis();

  // Button logic (debounce 50ms)
  bool btnState = digitalRead(BOOT_BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH && (now - lastBtnPress > 50)) {
    lastBtnPress = now;
    isStopped = !isStopped;
    
    if (isStopped) {
      // Draw STOPPED screen once
      u8g2.clearBuffer();
      // STOPPED in big letters
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.drawStr(8, 18, "STOPPED");
      
      // click to resume in small letters
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(2, 36, "click to resume");
      u8g2.sendBuffer();
      
      // Turn off LEDs
      digitalWrite(BLUE_LED_PIN, HIGH); // HIGH is off for this blue LED (active LOW)
      neo.clear();
      neo.show();
    } else {
      // When resuming, force a matrix update
      lastMatrixUpdate = now;
      lastFlashTime = now;
      u8g2.clearBuffer();
    }
  }
  lastBtnState = btnState;

  if (isStopped) {
    return; // Don't do animations if stopped
  }

  // Speed up logic (every 2 seconds, decrease interval by 50ms, minimum 50ms)
  if (now - lastSpeedIncrease > 2000) {
    lastSpeedIncrease = now;
    if (flashInterval > 50) {
      flashInterval -= 50;
    }
  }

  // Flash LEDs
  if (now - lastFlashTime >= flashInterval) {
    lastFlashTime = now;
    ledState = !ledState;
    
    // Toggle Blue LED (LOW = on, HIGH = off)
    digitalWrite(BLUE_LED_PIN, ledState ? LOW : HIGH);
    
    // Toggle NeoPixel (Green color when ON to match Matrix theme)
    if (ledState) {
      neo.setPixelColor(0, neo.Color(0, 255, 0)); // Green
    } else {
      neo.clear();
    }
    neo.show();
  }

  // Matrix Effect Update (every 50ms)
  if (now - lastMatrixUpdate > 50) {
    lastMatrixUpdate = now;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    
    for (int i = 0; i < NUM_COLUMNS; i++) {
      // Draw head character
      if (columns[i] > -7 && columns[i] < 45) {
        char c = random(33, 127);
        char str[2] = {c, '\0'};
        u8g2.drawStr(i * 5 + 1, columns[i], str);
      }
      
      // Draw tail characters
      for (int j = 1; j < 4; j++) {
        int tailY = columns[i] - (j * 7);
        if (tailY > -7 && tailY < 45) {
          char c = random(33, 127);
          char str[2] = {c, '\0'};
          u8g2.drawStr(i * 5 + 1, tailY, str);
        }
      }
      
      columns[i] += 7; // move down by font height
      if (columns[i] > 50) { // Off screen
        columns[i] = random(-40, 0); // Reset to random top position
      }
    }
    
    u8g2.sendBuffer();
  }
}
