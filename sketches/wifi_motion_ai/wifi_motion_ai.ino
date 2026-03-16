#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define BLUE_LED_PIN 8
#define NEOPIXEL_PIN 2

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

uint8_t targetMac[6] = {0};
int targetChannel = 1;
bool hasTarget = false;

volatile float emaRssi = 0.0f;
volatile float emaVariance = 0.0f;
volatile float emaDelta = 0.0f;
volatile unsigned long lastPacketMs = 0;
volatile unsigned long matchedPackets = 0;

float displayIntensity = 0.0f;
unsigned long lastDrawMs = 0;
unsigned long lastBlinkMs = 0;
bool blinkOn = false;
int lastIntensity = 0;
int recentPeakIntensity = 0;
unsigned long recentPeakMs = 0;
unsigned long lastMotionMs = 0;
const char *heldMotionLabel = "STILL";
unsigned long holdMotionUntilMs = 0;

bool packetMatchesTarget(const uint8_t *macHeader) {
  for (int addr = 0; addr < 3; addr++) {
    if (memcmp(macHeader + 4 + (addr * 6), targetMac, 6) == 0) {
      return true;
    }
  }
  return false;
}

void snifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (!hasTarget || type == WIFI_PKT_MISC) {
    return;
  }

  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
  if (packet->rx_ctrl.sig_len < 24) {
    return;
  }

  const uint8_t *macHeader = packet->payload;
  if (!packetMatchesTarget(macHeader)) {
    return;
  }

  float rssi = (float)packet->rx_ctrl.rssi;
  if (matchedPackets == 0) {
    emaRssi = rssi;
    emaVariance = 0.0f;
    emaDelta = 0.0f;
  } else {
    float diff = rssi - emaRssi;
    float absDiff = diff < 0 ? -diff : diff;
    emaRssi += 0.08f * diff;
    emaVariance = (emaVariance * 0.92f) + ((diff * diff) * 0.08f);
    emaDelta = (emaDelta * 0.85f) + (absDiff * 0.15f);
  }

  matchedPackets++;
  lastPacketMs = millis();
}

void drawBootMessage(const char *line1, const char *line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 12, line1);
  u8g2.drawStr(0, 24, line2);
  u8g2.sendBuffer();
}

void chooseStrongestAccessPoint() {
  int networks = WiFi.scanNetworks();
  if (networks <= 0) {
    Serial.println("No Wi-Fi networks found.");
    return;
  }

  int bestIndex = 0;
  int bestRssi = WiFi.RSSI(0);
  for (int i = 1; i < networks; i++) {
    int rssi = WiFi.RSSI(i);
    if (rssi > bestRssi) {
      bestRssi = rssi;
      bestIndex = i;
    }
  }

  uint8_t *bssid = WiFi.BSSID((uint8_t)bestIndex);
  if (bssid == nullptr) {
    Serial.println("Could not read BSSID for strongest network.");
    return;
  }

  memcpy(targetMac, bssid, 6);
  targetChannel = WiFi.channel((uint8_t)bestIndex);
  hasTarget = true;

  Serial.printf(
    "Tracking AP RSSI %d dBm on channel %d (%02X:%02X:%02X:%02X:%02X:%02X)\n",
    bestRssi,
    targetChannel,
    targetMac[0], targetMac[1], targetMac[2],
    targetMac[3], targetMac[4], targetMac[5]
  );
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Wi-Fi Motion AI starting...");

  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);

  neo.begin();
  neo.clear();
  neo.setBrightness(40);
  neo.show();

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(50);

  drawBootMessage("MICRO AI", "SCANNING WIFI");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  chooseStrongestAccessPoint();

  if (!hasTarget) {
    drawBootMessage("NO WIFI FOUND", "TRY AGAIN");
    return;
  }

  drawBootMessage("TARGET LOCKED", "STARTING AI");

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_promiscuous(true);
}

const char *classifyMotion(int intensity, float variance, float delta, unsigned long now) {
  int rising = intensity - lastIntensity;

  if (intensity > recentPeakIntensity || (now - recentPeakMs) > 4000) {
    recentPeakIntensity = intensity;
    recentPeakMs = now;
  }

  if (intensity >= 20) {
    lastMotionMs = now;
  }

  if (now < holdMotionUntilMs && intensity > 10) {
    return heldMotionLabel;
  }

  const char *label = "STILL";
  unsigned long holdMs = 350;

  if (recentPeakIntensity >= 88 && intensity < 24 && (now - recentPeakMs) < 1400) {
    label = "FALLING";
    holdMs = 2200;
  } else if (intensity >= 80 || (variance > 4.8f && delta > 4.8f)) {
    label = "RUNNING";
    holdMs = 900;
  } else if (delta > 4.2f && variance < 2.4f && intensity >= 24 && intensity < 70) {
    label = "ARM WAVE";
    holdMs = 1200;
  } else if (intensity >= 52 || variance > 2.8f) {
    label = "WALKING";
    holdMs = 900;
  } else if (delta > 3.0f && intensity >= 22) {
    label = "REACHING";
    holdMs = 800;
  } else if (intensity < 16 && recentPeakIntensity >= 34 && (now - lastMotionMs) < 2200) {
    label = "SITTING";
    holdMs = 1600;
  } else if (intensity >= 10) {
    label = "SHIFTING";
    holdMs = 650;
  } else if (rising > 6) {
    label = "STANDING";
    holdMs = 700;
  }

  heldMotionLabel = label;
  holdMotionUntilMs = now + holdMs;
  return label;
}

const char *distanceLabelFor(int intensity, float deltaScore) {
  if (intensity >= 82 || deltaScore >= 6.0f) {
    return "CLOSE";
  }
  if (intensity >= 58 || deltaScore >= 4.0f) {
    return "NEAR";
  }
  if (intensity >= 26 || deltaScore >= 2.0f) {
    return "MID";
  }
  return "FAR";
}

int centeredX(const char *text) {
  int width = u8g2.getStrWidth(text);
  return (72 - width) / 2;
}

void updateLights(int intensity, unsigned long now) {
  int flashInterval = map(intensity, 0, 100, 950, 45);
  int pixelBrightness = map(intensity, 0, 100, 16, 255);
  int red = map(intensity, 0, 100, 0, 255);
  int green = map(intensity, 0, 100, 190, 0);
  int blue = map(intensity, 0, 100, 20, 0);

  if (now - lastBlinkMs < (unsigned long)flashInterval) {
    return;
  }

  lastBlinkMs = now;
  blinkOn = !blinkOn;

  if (blinkOn) {
    digitalWrite(BLUE_LED_PIN, LOW);
    neo.setBrightness(pixelBrightness);
    neo.setPixelColor(0, neo.Color(red, green, blue));
  } else {
    digitalWrite(BLUE_LED_PIN, HIGH);
    neo.clear();
  }
  neo.show();
}

void drawUi(const char *motion, const char *distance) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_8x13B_tr);
  u8g2.drawStr(centeredX(motion), 15, motion);

  u8g2.setFont(u8g2_font_6x10_tr);
  char distanceLine[20];
  snprintf(distanceLine, sizeof(distanceLine), "DIST %s", distance);
  u8g2.drawStr(centeredX(distanceLine), 33, distanceLine);

  u8g2.sendBuffer();
}

void loop() {
  if (!hasTarget) {
    digitalWrite(BLUE_LED_PIN, HIGH);
    neo.clear();
    neo.show();
    delay(200);
    return;
  }

  unsigned long now = millis();

  if (lastPacketMs != 0 && (now - lastPacketMs) > 1200) {
    emaVariance *= 0.92f;
    emaDelta *= 0.92f;
  }

  float varianceScore = emaVariance * 4.0f;
  if (varianceScore > 70.0f) {
    varianceScore = 70.0f;
  }

  float deltaScore = emaDelta * 8.0f;
  if (deltaScore > 30.0f) {
    deltaScore = 30.0f;
  }

  float rawIntensity = varianceScore + deltaScore;
  if (rawIntensity > 100.0f) {
    rawIntensity = 100.0f;
  }

  displayIntensity = (displayIntensity * 0.72f) + (rawIntensity * 0.28f);
  int intensity = (int)(displayIntensity + 0.5f);

  const char *motion = classifyMotion(intensity, emaVariance, emaDelta, now);
  const char *distance = distanceLabelFor(intensity, emaDelta);

  updateLights(intensity, now);

  if (now - lastDrawMs >= 120) {
    lastDrawMs = now;
    drawUi(motion, distance);

    Serial.printf(
      "motion=%s distance=%s intensity=%d variance=%.2f delta=%.2f packets=%lu\n",
      motion, distance, intensity, emaVariance, emaDelta, matchedPackets
    );
  }

  lastIntensity = intensity;
}
