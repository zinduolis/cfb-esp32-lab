#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>

#define BLUE_LED_PIN  8
#define NEOPIXEL_PIN  2
#define BUTTON_PIN    9
#define MAX_APS       3
#define TIMELINE_LEN  60
#define CAL_SECONDS   10
#define NUM_PAGES     3

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ── AP Tracker ──────────────────────────────────────────────────

struct APTracker {
  uint8_t  mac[6];
  char     ssid[17];
  int      channel;
  int      scanRssi;

  float    calSum, calSumSq;
  int      calCount;
  float    baseMean, baseStd;

  volatile float emaRssi, emaVar, emaDelta;
  volatile float perturbation;
  volatile unsigned long lastPktMs, pktCount;
  bool     initDone;
};

APTracker aps[MAX_APS];
int  numAPs   = 0;
int  activeCh = 0;
bool calibrating = true;

// ── State Machine ───────────────────────────────────────────────

enum State { ST_EMPTY, ST_STILL, ST_SHIFT, ST_WALK, ST_ACTIVE, ST_RUSH };
const char *stateLabel[] = { "EMPTY", "STILL", "SHIFT", "WALK", "ACTIVE", "RUSH" };

State curState = ST_EMPTY;
unsigned long stateHoldUntil = 0;
float composite = 0, smooth = 0;
int   domAP    = 0;

// ── Events & Stats ──────────────────────────────────────────────

bool present  = false;
int  totalEvt = 0;
int  peakPct  = 0;
const char *peakLbl = "EMPTY";
unsigned long bootMs = 0;

// ── Timeline ────────────────────────────────────────────────────

uint8_t tl[TIMELINE_LEN];
int  tlHead = 0, tlFilled = 0;
unsigned long lastTlMs = 0;

// ── UI & Timing ─────────────────────────────────────────────────

int  page = 0;
bool lastBtn = HIGH;
unsigned long lastBtnMs   = 0;
unsigned long lastDrawMs   = 0;
unsigned long lastSerMs    = 0;
unsigned long lastBlinkMs  = 0;
bool blueOn = false;

// ═════════════════════════════════════════════════════════════════
//  Sniffer callback (runs in WiFi task context — keep fast)
// ═════════════════════════════════════════════════════════════════

void snifferCB(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC) return;
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t *)buf;
  if (p->rx_ctrl.sig_len < 24) return;

  const uint8_t *hdr = p->payload;
  float rssi = (float)p->rx_ctrl.rssi;
  unsigned long now = millis();

  for (int a = 0; a < numAPs; a++) {
    bool hit = false;
    for (int i = 0; i < 3; i++)
      if (memcmp(hdr + 4 + i * 6, aps[a].mac, 6) == 0) { hit = true; break; }
    if (!hit) continue;

    if (calibrating) {
      aps[a].calSum   += rssi;
      aps[a].calSumSq += rssi * rssi;
      aps[a].calCount++;
    } else {
      if (!aps[a].initDone) {
        aps[a].emaRssi  = rssi;
        aps[a].emaVar   = 0;
        aps[a].emaDelta = 0;
        aps[a].initDone = true;
      } else {
        float d  = rssi - aps[a].emaRssi;
        float ad = d < 0 ? -d : d;
        aps[a].emaRssi  += 0.08f * d;
        aps[a].emaVar    = aps[a].emaVar * 0.92f + d * d * 0.08f;
        aps[a].emaDelta  = aps[a].emaDelta * 0.85f + ad * 0.15f;
      }
      aps[a].pktCount++;
    }
    aps[a].lastPktMs = now;
  }
}

// ═════════════════════════════════════════════════════════════════
//  Display helpers
// ═════════════════════════════════════════════════════════════════

void showMsg(const char *a, const char *b = nullptr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, a);
  if (b) u8g2.drawStr(0, 22, b);
  u8g2.sendBuffer();
}

void showProgress(const char *label, int pct) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, label);
  u8g2.drawFrame(0, 16, 72, 10);
  u8g2.drawBox(1, 17, 70 * pct / 100, 8);
  char b[8];
  snprintf(b, sizeof(b), "%d%%", pct);
  u8g2.drawStr(0, 38, b);
  u8g2.sendBuffer();
}

int centeredX(const char *s) {
  return (72 - u8g2.getStrWidth(s)) / 2;
}

// ═════════════════════════════════════════════════════════════════
//  Boot: scan & pick APs
// ═════════════════════════════════════════════════════════════════

void pickAPs() {
  int n = WiFi.scanNetworks();
  Serial.printf("Scan: %d networks\n", n);
  if (n <= 0) return;

  int idx[32];
  int cnt = n < 32 ? n : 32;
  for (int i = 0; i < cnt; i++) idx[i] = i;
  for (int i = 0; i < cnt - 1; i++)
    for (int j = i + 1; j < cnt; j++)
      if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) {
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
      }

  activeCh = WiFi.channel((uint8_t)idx[0]);

  for (int i = 0; i < cnt && numAPs < MAX_APS; i++) {
    int k = idx[i];
    if (WiFi.channel((uint8_t)k) != activeCh) continue;
    uint8_t *bssid = WiFi.BSSID((uint8_t)k);
    if (!bssid) continue;

    APTracker &ap = aps[numAPs];
    memset(&ap, 0, sizeof(APTracker));
    memcpy(ap.mac, bssid, 6);
    strncpy(ap.ssid, WiFi.SSID(k).c_str(), 16);
    ap.ssid[16] = '\0';
    ap.channel  = activeCh;
    ap.scanRssi = WiFi.RSSI(k);

    Serial.printf("AP%d: %-16s ch%d %ddBm %02X:%02X:%02X:%02X:%02X:%02X\n",
      numAPs, ap.ssid, ap.channel, ap.scanRssi,
      ap.mac[0], ap.mac[1], ap.mac[2], ap.mac[3], ap.mac[4], ap.mac[5]);
    numAPs++;
  }
}

// ═════════════════════════════════════════════════════════════════
//  Boot: finish calibration
// ═════════════════════════════════════════════════════════════════

void finishCal() {
  for (int a = 0; a < numAPs; a++) {
    APTracker &ap = aps[a];
    if (ap.calCount > 10) {
      ap.baseMean = ap.calSum / ap.calCount;
      float v = ap.calSumSq / ap.calCount - ap.baseMean * ap.baseMean;
      ap.baseStd  = v > 0 ? sqrtf(v) : 1.0f;
    } else {
      ap.baseMean = (float)ap.scanRssi;
      ap.baseStd  = 3.0f;
    }
    Serial.printf("AP%d cal: mean=%.1f std=%.2f (%d pkts)\n",
      a, ap.baseMean, ap.baseStd, ap.calCount);
  }
  calibrating = false;
}

// ═════════════════════════════════════════════════════════════════
//  Runtime: decay stale APs
// ═════════════════════════════════════════════════════════════════

void decayStale(unsigned long now) {
  for (int a = 0; a < numAPs; a++)
    if (aps[a].lastPktMs && (now - aps[a].lastPktMs) > 1200) {
      aps[a].emaVar   *= 0.92f;
      aps[a].emaDelta *= 0.92f;
    }
}

// ═════════════════════════════════════════════════════════════════
//  Runtime: sensor fusion
// ═════════════════════════════════════════════════════════════════

void fusion() {
  float wSum = 0, wTot = 0, maxP = 0;
  int maxA = 0;

  for (int a = 0; a < numAPs; a++) {
    APTracker &ap = aps[a];
    float shift = ap.emaRssi - ap.baseMean;
    if (shift < 0) shift = -shift;
    float zS = ap.baseStd > 0.01f ? shift / ap.baseStd : 0;

    float p = zS * 15.0f + sqrtf(ap.emaVar) * 10.0f + ap.emaDelta * 8.0f;
    if (p > 100) p = 100;
    ap.perturbation = p;

    float w = (float)(ap.scanRssi + 100);
    if (w < 1) w = 1;
    wSum += p * w;
    wTot += w;
    if (p > maxP) { maxP = p; maxA = a; }
  }

  composite = wTot > 0 ? wSum / wTot : 0;
  smooth    = smooth * 0.75f + composite * 0.25f;
  domAP     = maxA;
}

// ═════════════════════════════════════════════════════════════════
//  Runtime: state machine
// ═════════════════════════════════════════════════════════════════

State classify(float c) {
  if (c >= 75) return ST_RUSH;
  if (c >= 55) return ST_ACTIVE;
  if (c >= 35) return ST_WALK;
  if (c >= 18) return ST_SHIFT;
  if (c >= 8)  return ST_STILL;
  return ST_EMPTY;
}

void updateState(unsigned long now) {
  State cand = classify(smooth);

  if (now < stateHoldUntil && cand <= curState) return;

  if (cand < curState && classify(smooth + 5) >= curState) return;

  if (cand == curState) return;

  curState = cand;
  unsigned long holds[] = { 500, 600, 800, 1000, 1200, 1500 };
  stateHoldUntil = now + holds[curState];

  bool nowPresent = (curState != ST_EMPTY);
  if (nowPresent && !present) {
    present = true;
    totalEvt++;
    Serial.printf("{\"event\":\"enter\",\"t\":%lu}\n", now / 1000);
  } else if (!nowPresent && present) {
    present = false;
    Serial.printf("{\"event\":\"exit\",\"t\":%lu}\n", now / 1000);
  }

  int c = (int)(smooth + 0.5f);
  if (c > peakPct) {
    peakPct = c;
    peakLbl = stateLabel[curState];
  }
}

// ═════════════════════════════════════════════════════════════════
//  Runtime: timeline sampling
// ═════════════════════════════════════════════════════════════════

void sampleTimeline(unsigned long now) {
  if (now - lastTlMs >= 5000) {
    lastTlMs = now;
    tl[tlHead] = (uint8_t)(smooth + 0.5f);
    tlHead = (tlHead + 1) % TIMELINE_LEN;
    if (tlFilled < TIMELINE_LEN) tlFilled++;
  }
}

// ═════════════════════════════════════════════════════════════════
//  OLED Page 1: Live View
// ═════════════════════════════════════════════════════════════════

void pageLive() {
  u8g2.clearBuffer();

  const char *lbl = stateLabel[curState];
  u8g2.setFont(u8g2_font_8x13B_tr);
  u8g2.drawStr(centeredX(lbl), 12, lbl);

  u8g2.setFont(u8g2_font_5x7_tr);
  if (numAPs > 0 && curState != ST_EMPTY) {
    char z[20];
    snprintf(z, sizeof(z), "@ %s", aps[domAP].ssid);
    int len = strlen(z);
    while (u8g2.getStrWidth(z) > 72 && len > 4) z[--len] = '\0';
    u8g2.drawStr(0, 21, z);
  }

  int conf = (int)(smooth + 0.5f);
  int bw   = (int)(smooth * 52.0f / 100.0f);
  if (bw > 52) bw = 52;
  u8g2.drawFrame(0, 24, 54, 7);
  if (bw > 0) u8g2.drawBox(1, 25, bw, 5);
  char pct[6];
  snprintf(pct, sizeof(pct), "%d%%", conf);
  u8g2.drawStr(56, 30, pct);

  int n = tlFilled < 72 ? tlFilled : 72;
  for (int i = 0; i < n; i++) {
    int j = (tlHead - n + i + TIMELINE_LEN) % TIMELINE_LEN;
    int h = tl[j] * 6 / 100;
    if (h < 1 && tl[j] > 0) h = 1;
    if (h > 0) u8g2.drawVLine(i, 40 - h, h);
  }

  u8g2.sendBuffer();
}

// ═════════════════════════════════════════════════════════════════
//  OLED Page 2: Timeline
// ═════════════════════════════════════════════════════════════════

void pageTimeline() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "TIMELINE 5m");

  int gH = 22, gY = 10;
  int n = tlFilled < 72 ? tlFilled : 72;
  for (int i = 0; i < n; i++) {
    int j = (tlHead - n + i + TIMELINE_LEN) % TIMELINE_LEN;
    int h = tl[j] * gH / 100;
    if (h < 1 && tl[j] > 0) h = 1;
    if (h > 0) u8g2.drawBox(i, gY + gH - h, 1, h);
  }

  char e[16];
  snprintf(e, sizeof(e), "Events: %d", totalEvt);
  u8g2.drawStr(0, 39, e);
  u8g2.sendBuffer();
}

// ═════════════════════════════════════════════════════════════════
//  OLED Page 3: Stats
// ═════════════════════════════════════════════════════════════════

void pageStats() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "STATS");

  unsigned long s = (millis() - bootMs) / 1000;
  char b[24];
  snprintf(b, sizeof(b), "Up: %02d:%02d:%02d",
    (int)(s / 3600), (int)((s % 3600) / 60), (int)(s % 60));
  u8g2.drawStr(0, 17, b);

  snprintf(b, sizeof(b), "Events: %d", totalEvt);
  u8g2.drawStr(0, 27, b);

  snprintf(b, sizeof(b), "Peak:%s %d%%", peakLbl, peakPct);
  u8g2.drawStr(0, 37, b);

  u8g2.sendBuffer();
}

// ═════════════════════════════════════════════════════════════════
//  NeoPixel + Blue LED
// ═════════════════════════════════════════════════════════════════

void updateLEDs(unsigned long now) {
  int c = (int)(smooth + 0.5f);

  if (c < 5) {
    float ph = (sinf(now / 2000.0f * PI) + 1.0f) / 2.0f;
    neo.setBrightness((int)(8 + 12 * ph));
    neo.setPixelColor(0, neo.Color(0, 30, 10));
  } else {
    float spd = 500.0f + (100 - c) * 15.0f;
    float ph  = (sinf(now / spd * PI) + 1.0f) / 2.0f;
    int   br  = (int)(30 + ph * c * 2.25f);
    if (br > 255) br = 255;
    neo.setBrightness(br);
    neo.setPixelColor(0, neo.Color(map(c, 0, 100, 0, 255),
                                   map(c, 0, 100, 180, 0), 0));
  }
  neo.show();

  if (present) {
    int interval = map(c, 0, 100, 800, 50);
    if (now - lastBlinkMs >= (unsigned long)interval) {
      lastBlinkMs = now;
      blueOn = !blueOn;
      digitalWrite(BLUE_LED_PIN, blueOn ? LOW : HIGH);
    }
  } else {
    digitalWrite(BLUE_LED_PIN, HIGH);
  }
}

// ═════════════════════════════════════════════════════════════════
//  Button (BOOT / GPIO9)
// ═════════════════════════════════════════════════════════════════

void checkButton(unsigned long now) {
  bool b = digitalRead(BUTTON_PIN);
  if (b == LOW && lastBtn == HIGH && (now - lastBtnMs) > 200) {
    page = (page + 1) % NUM_PAGES;
    lastBtnMs = now;
  }
  lastBtn = b;
}

// ═════════════════════════════════════════════════════════════════
//  JSON serial output
// ═════════════════════════════════════════════════════════════════

void serialJSON(unsigned long now) {
  if (now - lastSerMs < 500) return;
  lastSerMs = now;

  int c = (int)(smooth + 0.5f);
  const char *z = numAPs > 0 ? aps[domAP].ssid : "?";

  Serial.printf("{\"t\":%lu,\"state\":\"%s\",\"conf\":%d,\"zone\":\"%s\",\"aps\":[",
    now / 1000, stateLabel[curState], c, z);

  for (int a = 0; a < numAPs; a++) {
    if (a) Serial.print(',');
    Serial.printf("{\"s\":\"%s\",\"p\":%.0f,\"r\":%.0f}",
      aps[a].ssid, aps[a].perturbation, aps[a].emaRssi);
  }
  Serial.println("]}");
}

// ═════════════════════════════════════════════════════════════════
//  Setup
// ═════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("wifi_presence starting...");

  bootMs = millis();

  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  neo.begin();
  neo.clear();
  neo.setBrightness(20);
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);

  showMsg("PRESENCE", "SCANNING...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  pickAPs();

  if (numAPs == 0) {
    showMsg("NO WIFI", "FOUND");
    Serial.println("No APs found — halting.");
    while (true) delay(1000);
  }

  char apMsg[20];
  snprintf(apMsg, sizeof(apMsg), "%d APs ch%d", numAPs, activeCh);
  showMsg("FOUND", apMsg);
  delay(800);

  Serial.printf("Calibrating ch%d for %ds — stay still\n", activeCh, CAL_SECONDS);

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(activeCh, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&snifferCB);
  esp_wifi_set_promiscuous(true);

  unsigned long calStart = millis();
  while (millis() - calStart < (unsigned long)(CAL_SECONDS * 1000)) {
    int pct = (int)((millis() - calStart) * 100 / (CAL_SECONDS * 1000));
    showProgress("CALIBRATING", pct);
    delay(200);
  }

  finishCal();

  showMsg("READY", "MONITORING");
  delay(600);

  memset(tl, 0, sizeof(tl));
  lastTlMs = millis();
}

// ═════════════════════════════════════════════════════════════════
//  Loop
// ═════════════════════════════════════════════════════════════════

void loop() {
  unsigned long now = millis();

  decayStale(now);
  fusion();
  updateState(now);
  sampleTimeline(now);
  checkButton(now);

  if (now - lastDrawMs >= 125) {
    lastDrawMs = now;
    switch (page) {
      case 0: pageLive();     break;
      case 1: pageTimeline(); break;
      case 2: pageStats();    break;
    }
  }

  updateLEDs(now);
  serialJSON(now);
}
