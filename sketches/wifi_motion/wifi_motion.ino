#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define BLUE_LED_PIN  8
#define NEOPIXEL_PIN  2

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

uint8_t target_mac[6];
int target_channel = 1;

volatile float ema_rssi = 0;
volatile float ema_var = 0;
volatile unsigned long last_packet_time = 0;

void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  int rssi = pkt->rx_ctrl.rssi;
  
  // payload contains the MAC header
  uint8_t *mac_header = pkt->payload;
  
  // Quick sanity check on length
  if (pkt->rx_ctrl.sig_len < 24) return;

  bool match = false;
  // Check Address 1, 2, 3 in MAC header to see if it belongs to our target AP
  for (int i=0; i<3; i++) {
    if (memcmp(mac_header + 4 + (i*6), target_mac, 6) == 0) {
      match = true;
      break;
    }
  }
  
  if (match) {
    if (ema_rssi == 0) {
      ema_rssi = rssi;
    } else {
      float diff = rssi - ema_rssi;
      ema_rssi += 0.05 * diff;
      ema_var = 0.95 * ema_var + 0.05 * (diff * diff);
    }
    last_packet_time = millis();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Wi-Fi Motion Detector Starting...");

  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH); // OFF

  pinMode(NEOPIXEL_PIN, OUTPUT);
  delay(10);
  neo.begin();
  neo.setBrightness(100);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "SCANNING...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  int n = WiFi.scanNetworks();
  if (n > 0) {
    uint8_t *bssid = WiFi.BSSID((uint8_t)0);
    memcpy(target_mac, bssid, 6);
    target_channel = WiFi.channel((uint8_t)0);
    
    Serial.printf("Locked onto MAC: %02x:%02x:%02x:%02x:%02x:%02x Ch: %d\n",
                  target_mac[0], target_mac[1], target_mac[2],
                  target_mac[3], target_mac[4], target_mac[5],
                  target_channel);
  } else {
    Serial.println("No networks found! Running on default ch 1.");
  }

  // Start Promiscuous Sniffing
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(target_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "LOCKED ON AP");
  u8g2.sendBuffer();
  delay(1000);
}

unsigned long lastDraw = 0;
unsigned long lastFlash = 0;
bool ledState = false;

void loop() {
  unsigned long now = millis();

  // Decay variance if no packets received to simulate calming down
  if (now - last_packet_time > 1000) {
    ema_var *= 0.90;
    last_packet_time = now; // slow down decay loop
  }

  // Calculate Intensity (0-100)
  float variance = ema_var;
  if (variance < 0.5) variance = 0; // noise floor
  if (variance > 15.0) variance = 15.0; // cap maximum intensity
  
  int intensity = (int)((variance / 15.0) * 100.0);
  
  // Flash Lights: Speed intensifies with movement
  int flashInterval = map(intensity, 0, 100, 1000, 30);
  
  if (now - lastFlash >= flashInterval) {
    lastFlash = now;
    ledState = !ledState;
    
    if (ledState) {
      digitalWrite(BLUE_LED_PIN, LOW); // ON (Active LOW)
      
      // Interpolate NeoPixel Color: Green (Stable) -> Yellow/Orange -> Red (Motion)
      int r = intensity * 2.55;
      int g = (100 - intensity) * 2.55;
      neo.setPixelColor(0, neo.Color(r, g, 0));
    } else {
      digitalWrite(BLUE_LED_PIN, HIGH); // OFF
      neo.clear();
    }
    neo.show();
  }
  
  // Update OLED (10FPS to avoid flickering)
  if (now - lastDraw > 100) {
    lastDraw = now;
    u8g2.clearBuffer();
    
    // Status text
    if (intensity > 20) {
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.drawStr(0, 12, "MOVEMENT!");
    } else {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(0, 10, "STABLE");
    }
    
    // Intensity metric
    u8g2.setFont(u8g2_font_5x7_tr);
    char buf[32];
    sprintf(buf, "Int: %d%%", intensity);
    u8g2.drawStr(0, 24, buf);
    
    // Progress Bar showing intensity level
    u8g2.drawFrame(0, 28, 72, 12);
    int barWidth = map(intensity, 0, 100, 0, 70);
    u8g2.drawBox(1, 29, barWidth, 10);
    
    u8g2.sendBuffer();
  }
}
