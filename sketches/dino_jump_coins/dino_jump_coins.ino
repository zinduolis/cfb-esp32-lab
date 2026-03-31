#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 6
#define BOOT_BTN_PIN 9
#define BLUE_LED_PIN 8
#define NEOPIXEL_PIN 2

#define SCREEN_W 72
#define SCREEN_H 40
#define GROUND_Y 35

#define DINO_X 4
#define DINO_W 20
#define DINO_H 18
#define COIN_W 7
#define COIN_H 7

#define DINO_HIT_LEFT (DINO_X + 7)
#define DINO_HIT_RIGHT (DINO_X + 8)

#define BUTTON_DEBOUNCE_MS 80
#define TICK_MS 30
#define WALK_FRAME_MS 130
#define COLLECT_FLASH_MS 280
#define GRAVITY_Q8 20
#define JUMP_VEL_Q8 450

#define MAX_COINS 3
#define BASE_COIN_SPEED 110
#define SPAWN_MIN_MS 1500
#define SPAWN_EXTRA_MS 1200

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
Adafruit_NeoPixel neo(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ---- T-Rex bitmaps (20×18, XBM LSB-first, 3 bytes/row) ----
//
// Facing right, 20 px wide × 18 px tall.
//   Head top-right with eye hole at col 16.
//   Visible arms at rows 8-9 (cols 13-14) extending forward.
//   Tail sweeps left. Two-frame walk + jump pose.
//
//   Col: 0123456789ABCDEFGHIJ   (G=16 H=17 I=18 J=19)
//    0:  ............._####__   head top
//    1:  ............####_##_   head (eye at col 16)
//    2:  ............#######_   head
//    3:  ............#####___   jaw
//    4:  ..........#####_____   neck
//    5:  ........_#####______   upper body
//    6:  ........######______   body
//    7:  .......#######______   body (arm root)
//    8:  .....######__##_____   body + arm
//    9:  ...#######____#_____   body+tail + arm tip
//   10:  .########___________   tail
//   11:  ######______________   tail tip
//   12:  ....####____________   hips
//   13:  .....###____________   thighs
//   W1 legs / W2 legs / Jump legs below

static const uint8_t dino_walk1[] PROGMEM = {
  0x00, 0xE0, 0x03,  // row 0
  0x00, 0xF0, 0x06,  // row 1:  eye hole at col 16
  0x00, 0xF0, 0x07,  // row 2
  0x00, 0xF0, 0x01,  // row 3:  jaw
  0x00, 0x7C, 0x00,  // row 4:  neck
  0x00, 0x3E, 0x00,  // row 5
  0x00, 0x3F, 0x00,  // row 6
  0x80, 0x3F, 0x00,  // row 7:  arm root
  0xE0, 0x67, 0x00,  // row 8:  body + arm (cols 13-14)
  0xF8, 0x43, 0x00,  // row 9:  body + arm tip (col 14)
  0xFE, 0x01, 0x00,  // row 10: tail
  0x3F, 0x00, 0x00,  // row 11: tail tip
  0xF0, 0x00, 0x00,  // row 12: hips
  0xE0, 0x00, 0x00,  // row 13: thighs
  0x20, 0x02, 0x00,  // row 14: legs apart
  0x10, 0x04, 0x00,  // row 15
  0x10, 0x04, 0x00,  // row 16
  0x10, 0x04, 0x00,  // row 17: feet
};

static const uint8_t dino_walk2[] PROGMEM = {
  0x00, 0xE0, 0x03,
  0x00, 0xF0, 0x06,
  0x00, 0xF0, 0x07,
  0x00, 0xF0, 0x01,
  0x00, 0x7C, 0x00,
  0x00, 0x3E, 0x00,
  0x00, 0x3F, 0x00,
  0x80, 0x3F, 0x00,
  0xE0, 0x67, 0x00,
  0xF8, 0x43, 0x00,
  0xFE, 0x01, 0x00,
  0x3F, 0x00, 0x00,
  0xF0, 0x00, 0x00,
  0xE0, 0x00, 0x00,
  0xC0, 0x00, 0x00,  // row 14: legs close
  0x20, 0x01, 0x00,  // row 15
  0x10, 0x02, 0x00,  // row 16
  0x10, 0x02, 0x00,  // row 17: feet
};

static const uint8_t dino_jump_bmp[] PROGMEM = {
  0x00, 0xE0, 0x03,
  0x00, 0xF0, 0x06,
  0x00, 0xF0, 0x07,
  0x00, 0xF0, 0x01,
  0x00, 0x7C, 0x00,
  0x00, 0x3E, 0x00,
  0x00, 0x3F, 0x00,
  0x80, 0x3F, 0x00,
  0xE0, 0x67, 0x00,
  0xF8, 0x43, 0x00,
  0xFE, 0x01, 0x00,
  0x3F, 0x00, 0x00,
  0xF0, 0x00, 0x00,
  0xE0, 0x00, 0x00,
  0x20, 0x01, 0x00,  // row 14: tucked
  0xC0, 0x00, 0x00,  // row 15
  0x00, 0x00, 0x00,  // row 16
  0x00, 0x00, 0x00,  // row 17
};

// ---- Coin bitmap (7×7, circle + centre dot + shadow) ----

static const uint8_t coin_bmp[] PROGMEM = {
  0x1C,   //   ..###..
  0x22,   //   .#...#.
  0x41,   //   #.....#
  0x49,   //   #..#..#   centre dot
  0x61,   //   #....##   shadow
  0x32,   //   .#..##.   shadow
  0x1C,   //   ..###..
};

// ---- State ----

enum GameState { STATE_START, STATE_PLAYING, STATE_GAME_OVER };

struct Coin {
  int16_t xQ8;
  bool active;
  bool scored;
};

GameState gameState = STATE_START;
uint16_t score = 0;
uint16_t highScore = 0;

bool buttonWasDown = false;
unsigned long lastPressMs = 0;
unsigned long lastTickMs = 0;
unsigned long lastWalkMs = 0;
unsigned long nextSpawnMs = 0;
unsigned long collectFlashUntil = 0;

int walkFrame = 0;
int16_t jumpQ8 = 0;
int16_t velQ8 = 0;
uint16_t groundScroll = 0;

Coin coins[MAX_COINS];

// ---- Helpers ----

static int16_t feetY() {
  return GROUND_Y - (int16_t)(jumpQ8 >> 8);
}

static int coinSpeed() {
  int s = BASE_COIN_SPEED + (int)score * 3;
  return s > 280 ? 280 : s;
}

static int centeredX(const char *s) {
  return (SCREEN_W - u8g2.getStrWidth(s)) / 2;
}

// ---- Drawing ----

static void drawGround() {
  u8g2.drawHLine(0, GROUND_Y, SCREEN_W);
  int off1 = groundScroll % 18;
  for (int x = -off1; x < SCREEN_W; x += 18)
    u8g2.drawHLine(x, GROUND_Y + 1, 3);
  int off2 = (groundScroll + 8) % 13;
  for (int x = -off2; x < SCREEN_W; x += 13)
    u8g2.drawPixel(x, GROUND_Y + 2);
}

static void drawDino() {
  int fy = feetY();
  int by = fy - DINO_H + 1;
  bool air = jumpQ8 > 64;

  const uint8_t *frame;
  if (air)
    frame = dino_jump_bmp;
  else if (walkFrame == 0)
    frame = dino_walk1;
  else
    frame = dino_walk2;

  u8g2.drawXBMP(DINO_X, by, DINO_W, DINO_H, frame);
}

static void drawCoinAt(int x) {
  u8g2.drawXBMP(x, GROUND_Y - COIN_H, COIN_W, COIN_H, coin_bmp);
}

// ---- LEDs ----

static void flashLeds() {
  collectFlashUntil = millis() + COLLECT_FLASH_MS;
}

static void updateLeds(unsigned long now) {
  if (now < collectFlashUntil) {
    bool on = ((now - (collectFlashUntil - COLLECT_FLASH_MS)) / 50) & 1;
    digitalWrite(BLUE_LED_PIN, on ? LOW : HIGH);
    neo.setPixelColor(0, on ? neo.Color(255, 230, 60)
                            : neo.Color(120, 70, 0));
    neo.show();
  } else {
    digitalWrite(BLUE_LED_PIN, HIGH);
    neo.setPixelColor(0, 0);
    neo.show();
  }
}

// ---- Screens ----

static void drawStart() {
  u8g2.clearBuffer();
  drawGround();
  drawDino();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("Dino Run"), 8, "Dino Run");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(centeredX("BOOT = jump"), GROUND_Y + 4, "BOOT = jump");
  u8g2.sendBuffer();
}

static void drawGameOver() {
  char sb[20], hb[20];
  snprintf(sb, sizeof(sb), "Score: %u", score);
  snprintf(hb, sizeof(hb), "Best: %u", highScore);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("Game Over"), 14, "Game Over");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(centeredX(sb), 24, sb);
  u8g2.drawStr(centeredX(hb), 32, hb);
  u8g2.sendBuffer();
}

// ---- Game logic ----

static void resetPlay() {
  score = 0;
  jumpQ8 = 0;
  velQ8 = 0;
  walkFrame = 0;
  groundScroll = 0;
  for (int i = 0; i < MAX_COINS; i++) {
    coins[i].active = false;
    coins[i].scored = false;
  }
  nextSpawnMs = millis() + 1200;
  Serial.println("Game started! Jump over coins to collect.");
}

static void doGameOver() {
  gameState = STATE_GAME_OVER;
  if (score > highScore) highScore = score;
  Serial.print("Game over! Score: ");
  Serial.print(score);
  Serial.print("  Best: ");
  Serial.println(highScore);
}

static void trySpawnCoin(unsigned long now) {
  if (now < nextSpawnMs) return;
  int slot = -1;
  for (int i = 0; i < MAX_COINS; i++) {
    if (!coins[i].active) { slot = i; break; }
  }
  if (slot < 0) { nextSpawnMs = now + 300; return; }

  coins[slot].active = true;
  coins[slot].scored = false;
  coins[slot].xQ8 = (int16_t)((SCREEN_W + 4) << 8);

  int gap = SPAWN_MIN_MS + (int)random(SPAWN_EXTRA_MS)
            - (int)score * 8;
  if (gap < 800) gap = 800;
  nextSpawnMs = now + (unsigned long)gap;
}

static void updatePhysics() {
  if (jumpQ8 == 0 && velQ8 <= 0) { velQ8 = 0; return; }
  velQ8 -= GRAVITY_Q8;
  jumpQ8 += velQ8;
  if (jumpQ8 < 0) { jumpQ8 = 0; velQ8 = 0; }
}

static void stepGame(unsigned long now) {
  updatePhysics();
  trySpawnCoin(now);
  groundScroll++;

  int fy = feetY();
  int speed = coinSpeed();

  for (int i = 0; i < MAX_COINS; i++) {
    if (!coins[i].active) continue;
    coins[i].xQ8 -= (int16_t)speed;
    int cx = coins[i].xQ8 >> 8;
    int cr = cx + COIN_W - 1;

    if (cr < -4) { coins[i].active = false; continue; }

    if (cr >= DINO_HIT_LEFT && cx <= DINO_HIT_RIGHT) {
      if (fy >= GROUND_Y) {
        doGameOver();
        return;
      }
    }

    if (cr < DINO_HIT_LEFT && !coins[i].scored) {
      coins[i].scored = true;
      score++;
      flashLeds();
      Serial.print("Coin! Score: ");
      Serial.println(score);
    }
  }

  if (now - lastWalkMs >= WALK_FRAME_MS) {
    lastWalkMs = now;
    if (jumpQ8 < 64) walkFrame ^= 1;
  }
}

static void drawPlaying() {
  u8g2.clearBuffer();
  drawGround();
  drawDino();
  for (int i = 0; i < MAX_COINS; i++)
    if (coins[i].active) drawCoinAt(coins[i].xQ8 >> 8);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", score);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(SCREEN_W - u8g2.getStrWidth(buf) - 1, 6, buf);
  u8g2.sendBuffer();
}

// ---- Button ----

static void handleButton() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !buttonWasDown
      && (now - lastPressMs) > BUTTON_DEBOUNCE_MS) {
    buttonWasDown = true;
    lastPressMs = now;

    switch (gameState) {
      case STATE_START:
        resetPlay();
        gameState = STATE_PLAYING;
        lastTickMs = now;
        lastWalkMs = now;
        break;
      case STATE_PLAYING:
        if (jumpQ8 == 0 && velQ8 <= 0) {
          velQ8 = JUMP_VEL_Q8;
          Serial.println("Jump!");
        }
        break;
      case STATE_GAME_OVER:
        gameState = STATE_START;
        jumpQ8 = 0;
        velQ8 = 0;
        break;
    }
  }
  if (!pressed) buttonWasDown = false;
}

// ---- Main ----

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("dino_jump_coins boot");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);

  pinMode(NEOPIXEL_PIN, OUTPUT);
  delay(10);
  neo.begin();
  neo.setBrightness(110);
  neo.clear();
  neo.show();

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(60);

  randomSeed(esp_random());
  drawStart();
}

void loop() {
  unsigned long now = millis();
  handleButton();
  updateLeds(now);

  switch (gameState) {
    case STATE_START:
      drawStart();
      delay(25);
      break;
    case STATE_PLAYING:
      if (now - lastTickMs >= TICK_MS) {
        lastTickMs = now;
        stepGame(now);
      }
      if (gameState == STATE_PLAYING) drawPlaying();
      delay(4);
      break;
    case STATE_GAME_OVER:
      drawGameOver();
      delay(30);
      break;
  }
}
