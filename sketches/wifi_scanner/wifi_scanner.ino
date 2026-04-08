#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#define BOOT_BTN       9
#define NEOPIXEL       2
#define LONG_PRESS     1000
#define DEBOUNCE       180
#define MAX_NETS       30
#define FLASH_MS       100   // LED flash interval for open networks

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── Data ────────────────────────────────────────────────────────────

struct WifiEntry {
  String         ssid;
  String         bssid;
  int32_t        rssi;
  uint8_t        channel;
  wifi_auth_mode_t auth;
  bool           hidden;
};

WifiEntry nets[MAX_NETS];
int  netCount    = 0;
int  hiddenCount = 0;
int  currentIdx  = 0;        // 0..netCount-1 = networks, netCount = channel summary

unsigned long btnDownMs   = 0;
bool          btnWasDown  = false;
bool          longHandled = false;

unsigned long lastFlashMs = 0;
bool          ledOn       = false;

// ── Helpers ─────────────────────────────────────────────────────────

const char *authLabel(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:          return "OPEN";
    case WIFI_AUTH_WEP:           return "WEP";
    case WIFI_AUTH_WPA_PSK:       return "WPA";
    case WIFI_AUTH_WPA2_PSK:      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA+2";
    case WIFI_AUTH_WPA3_PSK:      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "W2+3";
    default:                      return "???";
  }
}

int signalQuality(int32_t rssi) {
  if (rssi >= -50) return 100;
  if (rssi <= -100) return 0;
  return 2 * (rssi + 100);       // linear map -100→0%, -50→100%
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
    if (i < bars) u8g2.drawBox(bx, by, 3, bh);
    else          u8g2.drawFrame(bx, by, 3, bh);
  }
}

void sortByRssi() {
  for (int i = 0; i < netCount - 1; i++)
    for (int j = 0; j < netCount - i - 1; j++)
      if (nets[j].rssi < nets[j + 1].rssi) {
        WifiEntry tmp = nets[j];
        nets[j]       = nets[j + 1];
        nets[j + 1]   = tmp;
      }
}

// ── Scan ────────────────────────────────────────────────────────────

void doScan() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(6, 22, "SCANNING...");
  u8g2.sendBuffer();

  neo.setPixelColor(0, neo.Color(0, 0, 80));
  neo.show();

  Serial.println("──── Starting WiFi scan ────");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int found = WiFi.scanNetworks(false, true);   // show_hidden = true
  netCount    = found > MAX_NETS ? MAX_NETS : found;
  hiddenCount = 0;

  for (int i = 0; i < netCount; i++) {
    nets[i].ssid    = WiFi.SSID(i);
    nets[i].bssid   = WiFi.BSSIDstr(i);
    nets[i].rssi    = WiFi.RSSI(i);
    nets[i].channel = WiFi.channel(i);
    nets[i].auth    = WiFi.encryptionType(i);
    nets[i].hidden  = (nets[i].ssid.length() == 0);

    if (nets[i].hidden) {
      hiddenCount++;
      nets[i].ssid = "(hidden)";
    }

    Serial.printf("  %2d  %-20s  %4d dBm (%3d%%)  ch%-2d  %-5s  %s%s\n",
                  i + 1,
                  nets[i].ssid.c_str(),
                  nets[i].rssi,
                  signalQuality(nets[i].rssi),
                  nets[i].channel,
                  authLabel(nets[i].auth),
                  nets[i].bssid.c_str(),
                  nets[i].hidden ? "  [HIDDEN]" : "");
  }

  sortByRssi();
  WiFi.scanDelete();
  currentIdx = 0;

  Serial.printf("Scan done: %d networks (%d hidden)\n", netCount, hiddenCount);
  Serial.println("────────────────────────────");
}

// ── Display: Network Detail ─────────────────────────────────────────

void drawNetwork() {
  if (netCount == 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 16, "NO NETWORKS");
    u8g2.drawStr(4, 28, "PRESS BTN");
    u8g2.drawStr(4, 38, "TO RE-SCAN");
    u8g2.sendBuffer();
    neo.setPixelColor(0, neo.Color(0, 0, 20));
    neo.show();
    return;
  }

  WifiEntry &n = nets[currentIdx];

  // Row 1: SSID (5x7 font, up to 14 chars)
  char truncSsid[15];
  if (n.hidden) {
    snprintf(truncSsid, sizeof(truncSsid), "? %s", n.ssid.c_str());
  } else {
    strncpy(truncSsid, n.ssid.c_str(), 14);
    truncSsid[14] = '\0';
  }

  // Row 2: RSSI + quality
  int qual = signalQuality(n.rssi);
  char rssiStr[16];
  snprintf(rssiStr, sizeof(rssiStr), "%ddB %d%%", n.rssi, qual);

  // Row 3: channel + auth (+ open warning)
  char infoLine[20];
  snprintf(infoLine, sizeof(infoLine), "Ch%-2d %s", n.channel, authLabel(n.auth));

  // Row 4: BSSID last 3 octets + index
  const char *bssid = n.bssid.c_str();
  const char *bssidShort = bssid;
  if (n.bssid.length() > 8)
    bssidShort = bssid + n.bssid.length() - 8;   // "DD:EE:FF"

  char bottomLine[22];
  snprintf(bottomLine, sizeof(bottomLine), "%s %d/%d", bssidShort, currentIdx + 1, netCount);

  // ── Draw ──
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, truncSsid);

  drawSignalIcon(signalBars(n.rssi), 0, 10);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(18, 16, rssiStr);

  u8g2.drawStr(0, 25, infoLine);
  if (n.auth == WIFI_AUTH_OPEN)
    u8g2.drawStr(60, 25, "!!");

  u8g2.drawStr(0, 37, bottomLine);

  u8g2.sendBuffer();

  // LED: steady blue for encrypted, flash handled in loop() for open
  if (n.auth != WIFI_AUTH_OPEN) {
    neo.setPixelColor(0, neo.Color(0, 0, 40));
    neo.show();
  }

  Serial.printf(">> %d/%d: %s  %ddBm (%d%%)  ch%d  %s  BSSID:%s%s\n",
                currentIdx + 1, netCount,
                n.ssid.c_str(), n.rssi, qual, n.channel,
                authLabel(n.auth), n.bssid.c_str(),
                n.hidden ? " [HIDDEN]" : "");
}

// ── Display: Channel Summary ────────────────────────────────────────

void drawChannelSummary() {
  int chCount[14] = {0};
  for (int i = 0; i < netCount; i++) {
    int ch = nets[i].channel;
    if (ch >= 1 && ch <= 13) chCount[ch]++;
  }

  int peak = 1;
  for (int c = 1; c <= 13; c++)
    if (chCount[c] > peak) peak = chCount[c];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);

  // Header
  u8g2.drawStr(0, 6, "CHANNELS");
  char hdr[12];
  snprintf(hdr, sizeof(hdr), "%dN %dH", netCount, hiddenCount);
  u8g2.drawStr(72 - u8g2.getStrWidth(hdr), 6, hdr);

  // Bar chart: 13 bars, 4px wide + 1px gap = 5px each = 65px
  const int barMax = 20;
  const int baseY  = 35;
  for (int ch = 1; ch <= 13; ch++) {
    int x = (ch - 1) * 5 + 3;
    if (chCount[ch] > 0) {
      int h = max(2, chCount[ch] * barMax / peak);
      u8g2.drawBox(x, baseY - h, 4, h);
      // count above bar
      char cnt[4];
      snprintf(cnt, sizeof(cnt), "%d", chCount[ch]);
      u8g2.drawStr(x, baseY - h - 1, cnt);
    }
  }

  // Axis labels: channels 1, 6, 11
  u8g2.drawStr(3,              40, "1");
  u8g2.drawStr(3 + 5 * 5,     40, "6");
  u8g2.drawStr(3 + 10 * 5,    40, "11");

  u8g2.sendBuffer();

  neo.setPixelColor(0, neo.Color(0, 0, 40));
  neo.show();

  Serial.println(">> Channel summary page");
}

// ── Display router (decides which page to show) ─────────────────────

void drawPage() {
  if (currentIdx < netCount)
    drawNetwork();
  else
    drawChannelSummary();
}

// ── Setup & Loop ────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("WiFi Scanner v2");
  Serial.println("Short press = next | Long press = re-scan");

  pinMode(BOOT_BTN, INPUT_PULLUP);

  neo.begin();
  neo.setBrightness(30);
  neo.clear();
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);

  doScan();
  drawPage();
}

void loop() {
  bool pressed = (digitalRead(BOOT_BTN) == LOW);
  unsigned long now = millis();

  // ── Fast-flash blue LED when viewing an open network ──
  if (currentIdx < netCount && netCount > 0 &&
      nets[currentIdx].auth == WIFI_AUTH_OPEN) {
    if (now - lastFlashMs >= FLASH_MS) {
      lastFlashMs = now;
      ledOn = !ledOn;
      neo.setPixelColor(0, ledOn ? neo.Color(0, 0, 80) : neo.Color(0, 0, 0));
      neo.show();
    }
  }

  // ── Button handling ──
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
    drawPage();
  }

  if (!pressed && btnWasDown) {
    if (!longHandled && (now - btnDownMs) >= DEBOUNCE) {
      int totalPages = netCount > 0 ? netCount + 1 : 1;  // +1 for channel summary
      currentIdx = (currentIdx + 1) % totalPages;
      drawPage();
    }
    btnWasDown = false;
  }

  delay(10);
}
