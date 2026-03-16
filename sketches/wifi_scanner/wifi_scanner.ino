#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#define BOOT_BTN   9
#define NEOPIXEL   2
#define LONG_PRESS 1000
#define DEBOUNCE   180
#define MAX_NETS   30

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL, NEO_GRB + NEO_KHZ800);

struct WifiEntry {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  wifi_auth_mode_t auth;
};

WifiEntry nets[MAX_NETS];
int netCount   = 0;
int currentIdx = 0;

unsigned long btnDownMs   = 0;
bool          btnWasDown  = false;
bool          longHandled = false;

void setLedForRssi(int32_t rssi) {
  uint8_t r, g, b;
  if (rssi > -50) {
    r = 0; g = 180; b = 0;
  } else if (rssi > -70) {
    r = 180; g = 140; b = 0;
  } else {
    r = 200; g = 0; b = 0;
  }
  neo.setPixelColor(0, neo.Color(r, g, b));
  neo.show();
}

int signalBars(int32_t rssi) {
  if (rssi > -50) return 4;
  if (rssi > -60) return 3;
  if (rssi > -70) return 2;
  if (rssi > -80) return 1;
  return 0;
}

void drawSignalIcon(int bars, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (8 - bh);
    if (i < bars)
      u8g2.drawBox(bx, by, 3, bh);
    else
      u8g2.drawFrame(bx, by, 3, bh);
  }
}

void doScan() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(6, 22, "SCANNING...");
  u8g2.sendBuffer();

  neo.setPixelColor(0, neo.Color(0, 0, 80));
  neo.show();

  Serial.println("Starting WiFi scan...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int found = WiFi.scanNetworks(false, true);
  netCount = found > MAX_NETS ? MAX_NETS : found;

  for (int i = 0; i < netCount; i++) {
    nets[i].ssid    = WiFi.SSID(i);
    nets[i].rssi    = WiFi.RSSI(i);
    nets[i].channel = WiFi.channel(i);
    nets[i].auth    = WiFi.encryptionType(i);

    if (nets[i].ssid.length() == 0)
      nets[i].ssid = "(hidden)";

    Serial.printf("  %2d  %-24s  %4d dBm  ch%2d\n",
                  i + 1, nets[i].ssid.c_str(), nets[i].rssi, nets[i].channel);
  }

  WiFi.scanDelete();
  currentIdx = 0;
  Serial.printf("Scan complete: %d networks found.\n", netCount);
}

const char *authLabel(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA+2";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "W2+3";
    default:                        return "???";
  }
}

int centeredX(const char *text) {
  return (72 - u8g2.getStrWidth(text)) / 2;
}

void drawNetwork() {
  if (netCount == 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 16, "NO NETWORKS");
    u8g2.drawStr(4, 28, "PRESS BTN");
    u8g2.drawStr(4, 38, "TO RE-SCAN");
    u8g2.sendBuffer();
    neo.setPixelColor(0, neo.Color(40, 0, 0));
    neo.show();
    return;
  }

  WifiEntry &n = nets[currentIdx];

  char truncSsid[15];
  strncpy(truncSsid, n.ssid.c_str(), 14);
  truncSsid[14] = '\0';

  char rssiStr[12];
  snprintf(rssiStr, sizeof(rssiStr), "%d dBm", n.rssi);

  char infoLine[20];
  snprintf(infoLine, sizeof(infoLine), "Ch%-2d %s", n.channel, authLabel(n.auth));

  char idxStr[10];
  snprintf(idxStr, sizeof(idxStr), "%d/%d", currentIdx + 1, netCount);

  u8g2.clearBuffer();

  // Row 1: SSID
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, truncSsid);

  // Row 2: signal bars + RSSI
  drawSignalIcon(signalBars(n.rssi), 0, 11);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(18, 17, rssiStr);

  // Row 3: channel + auth
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 28, infoLine);

  // Row 4: index right-aligned
  int idxW = u8g2.getStrWidth(idxStr);
  u8g2.drawStr(72 - idxW, 38, idxStr);

  u8g2.sendBuffer();

  setLedForRssi(n.rssi);

  Serial.printf("Showing %d/%d: %s  %d dBm  ch%d\n",
                currentIdx + 1, netCount, n.ssid.c_str(), n.rssi, n.channel);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("WiFi Scanner starting...");

  pinMode(BOOT_BTN, INPUT_PULLUP);

  neo.begin();
  neo.setBrightness(30);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);

  doScan();
  drawNetwork();
}

void loop() {
  bool pressed = (digitalRead(BOOT_BTN) == LOW);
  unsigned long now = millis();

  if (pressed && !btnWasDown) {
    btnDownMs   = now;
    btnWasDown  = true;
    longHandled = false;
  }

  if (pressed && btnWasDown && !longHandled &&
      (now - btnDownMs) >= LONG_PRESS) {
    longHandled = true;
    Serial.println("Long press -> re-scan");
    doScan();
    drawNetwork();
  }

  if (!pressed && btnWasDown) {
    if (!longHandled && (now - btnDownMs) >= DEBOUNCE) {
      currentIdx = (currentIdx + 1) % (netCount > 0 ? netCount : 1);
      drawNetwork();
    }
    btnWasDown = false;
  }

  delay(10);
}
