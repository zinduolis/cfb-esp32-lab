#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define OLED_SDA  5
#define OLED_SCL  6
#define BTN_PIN   9  // BOOT button (only usable button; left button is RST)
#define GRN_LED   8  // Onboard green/blue LED, active LOW
#define NEO_PIN   2  // WS2812B NeoPixel (red)

#define SCR_W 72
#define SCR_H 40
#define CELL  4
#define BW    10
#define BH    10

#define DEBOUNCE_MS    100
#define LONG_PRESS_MS  350
#define FALL_START     600
#define FALL_MIN       150

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
Adafruit_NeoPixel neo(1, NEO_PIN, NEO_GRB + NEO_KHZ800);

enum State { S_TITLE, S_PLAY, S_OVER };
enum Act   { A_NONE, A_TAP, A_LONG };
enum Fx    { FX_NONE, FX_LOCK, FX_CLEAR, FX_OVER, FX_START };

struct Piece { int8_t type, rot, x, y; };

State state = S_TITLE;
Piece cur;
uint8_t board[BH][BW];
uint16_t score = 0, totalLines = 0;
unsigned long lastFall = 0;

bool btnDown = false;
unsigned long btnPressT = 0, btnLastEvt = 0;
bool btnLongDone = false;

Fx fx = FX_NONE;
unsigned long fxStart = 0;
uint8_t fxParam = 0;
unsigned long grnOffAt = 0;

// --- Button ---

Act pollBtn() {
  bool p = (digitalRead(BTN_PIN) == LOW);
  unsigned long now = millis();
  Act a = A_NONE;

  if (p && !btnDown) {
    btnDown = true;
    btnPressT = now;
    btnLongDone = false;
  }

  if (p && btnDown && !btnLongDone && (now - btnPressT) >= LONG_PRESS_MS) {
    btnLongDone = true;
    btnLastEvt = now;
    a = A_LONG;
  }

  if (!p && btnDown) {
    if (!btnLongDone && (now - btnLastEvt) > DEBOUNCE_MS) {
      btnLastEvt = now;
      a = A_TAP;
    }
    btnDown = false;
  }

  return a;
}

// --- LEDs ---

void flashGreen(unsigned long ms) {
  digitalWrite(GRN_LED, LOW);
  grnOffAt = millis() + ms;
}

void startFx(Fx f, uint8_t param = 0) {
  fx = f;
  fxStart = millis();
  fxParam = param;
}

void tickLeds() {
  if (grnOffAt && millis() >= grnOffAt) {
    digitalWrite(GRN_LED, HIGH);
    grnOffAt = 0;
  }

  unsigned long e = millis() - fxStart;
  switch (fx) {
    case FX_LOCK:
      if (e < 50)
        neo.setPixelColor(0, neo.Color(30, 0, 0));
      else {
        neo.setPixelColor(0, 0);
        fx = FX_NONE;
      }
      break;

    case FX_CLEAR: {
      uint16_t dur = 300 + fxParam * 120;
      if (e < dur) {
        uint8_t bright = 200 - (uint8_t)(e * 170 / dur);
        neo.setPixelColor(0, neo.Color(bright, 0, 0));
      } else {
        neo.setPixelColor(0, 0);
        fx = FX_NONE;
      }
      break;
    }

    case FX_OVER:
      if (e < 2000) {
        bool on = ((e / 200) % 2) == 0;
        neo.setPixelColor(0, on ? neo.Color(80, 0, 0) : 0);
        if (on && !grnOffAt) flashGreen(100);
      } else {
        neo.setPixelColor(0, 0);
        fx = FX_NONE;
      }
      break;

    case FX_START:
      if (e < 200)
        neo.setPixelColor(0, neo.Color(20, 0, 0));
      else {
        neo.setPixelColor(0, 0);
        fx = FX_NONE;
      }
      break;

    default:
      break;
  }
  neo.show();
}

// --- Tetromino data: 7 pieces x 4 rotations x 4 cells x (x,y) ---

const int8_t SH[7][4][4][2] = {
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
   {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}},
   {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
   {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
   {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
   {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}
};

// --- Game logic ---

bool fits(int8_t t, int8_t r, int8_t px, int8_t py) {
  for (int i = 0; i < 4; i++) {
    int x = px + SH[t][r][i][0];
    int y = py + SH[t][r][i][1];
    if (x < 0 || x >= BW || y < 0 || y >= BH) return false;
    if (board[y][x]) return false;
  }
  return true;
}

void lockPiece() {
  for (int i = 0; i < 4; i++) {
    int x = cur.x + SH[cur.type][cur.rot][i][0];
    int y = cur.y + SH[cur.type][cur.rot][i][1];
    if (x >= 0 && x < BW && y >= 0 && y < BH)
      board[y][x] = 1;
  }
}

bool spawn() {
  cur.type = random(7);
  cur.rot = 0;
  cur.x = 3;
  cur.y = 0;
  return fits(cur.type, cur.rot, cur.x, cur.y);
}

void doClear() {
  int n = 0;
  for (int y = BH - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BW; x++) {
      if (!board[y][x]) { full = false; break; }
    }
    if (full) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < BW; x++)
          board[yy][x] = board[yy - 1][x];
      for (int x = 0; x < BW; x++) board[0][x] = 0;
      n++;
      y++;
    }
  }
  if (n) {
    totalLines += n;
    score += n * n * 10;
    startFx(FX_CLEAR, n);
    flashGreen(300 + n * 100);
    Serial.printf("Cleared %d  total=%d  score=%d\n", n, totalLines, score);
  }
}

void doGameOver() {
  state = S_OVER;
  startFx(FX_OVER);
  Serial.printf("Game over  score=%d  lines=%d\n", score, totalLines);
}

void moveRight() {
  if (fits(cur.type, cur.rot, cur.x + 1, cur.y)) {
    cur.x++;
  } else {
    for (int8_t tx = 0; tx < cur.x; tx++) {
      if (fits(cur.type, cur.rot, tx, cur.y)) {
        cur.x = tx;
        return;
      }
    }
  }
}

void rotate() {
  int8_t nr = (cur.rot + 1) % 4;
  if      (fits(cur.type, nr, cur.x,     cur.y)) { cur.rot = nr; }
  else if (fits(cur.type, nr, cur.x - 1, cur.y)) { cur.x--; cur.rot = nr; }
  else if (fits(cur.type, nr, cur.x + 1, cur.y)) { cur.x++; cur.rot = nr; }
  else if (fits(cur.type, nr, cur.x - 2, cur.y)) { cur.x -= 2; cur.rot = nr; }
  else if (fits(cur.type, nr, cur.x + 2, cur.y)) { cur.x += 2; cur.rot = nr; }
}

void stepFall() {
  if (fits(cur.type, cur.rot, cur.x, cur.y + 1)) {
    cur.y++;
  } else {
    lockPiece();
    flashGreen(60);
    startFx(FX_LOCK);
    doClear();
    if (!spawn()) doGameOver();
  }
}

// --- Drawing ---

int cx(const char *t) { return (SCR_W - u8g2.getStrWidth(t)) / 2; }

void drawTitle() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(cx("TETRIS"), 10, "TETRIS");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx("tap: move >"), 20, "tap: move >");
  u8g2.drawStr(cx("hold: rotate"), 28, "hold: rotate");
  u8g2.drawStr(cx("press to play"), 38, "press to play");
  u8g2.sendBuffer();
}

void drawGame() {
  u8g2.clearBuffer();

  for (int y = 0; y < BH; y++)
    for (int x = 0; x < BW; x++)
      if (board[y][x])
        u8g2.drawBox(x * CELL, y * CELL, CELL, CELL);

  for (int i = 0; i < 4; i++) {
    int x = cur.x + SH[cur.type][cur.rot][i][0];
    int y = cur.y + SH[cur.type][cur.rot][i][1];
    if (x >= 0 && x < BW && y >= 0 && y < BH)
      u8g2.drawBox(x * CELL, y * CELL, CELL, CELL);
  }

  int8_t gy = cur.y;
  while (fits(cur.type, cur.rot, cur.x, gy + 1)) gy++;
  if (gy != cur.y) {
    for (int i = 0; i < 4; i++) {
      int x = cur.x + SH[cur.type][cur.rot][i][0];
      int y = gy   + SH[cur.type][cur.rot][i][1];
      if (x >= 0 && x < BW && y >= 0 && y < BH)
        u8g2.drawFrame(x * CELL, y * CELL, CELL, CELL);
    }
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  char buf[12];
  snprintf(buf, sizeof(buf), "S%u", score);
  u8g2.drawStr(42, 8, buf);
  snprintf(buf, sizeof(buf), "L%u", totalLines);
  u8g2.drawStr(42, 16, buf);

  u8g2.sendBuffer();
}

void drawOver() {
  char buf[16];
  snprintf(buf, sizeof(buf), "Score %u", score);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(cx("GAME OVER"), 14, "GAME OVER");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cx(buf), 26, buf);
  u8g2.drawStr(cx("press btn"), 38, "press btn");
  u8g2.sendBuffer();
}

// --- Core ---

void startGame() {
  memset(board, 0, sizeof(board));
  score = 0;
  totalLines = 0;
  lastFall = millis();
  startFx(FX_START);
  flashGreen(150);
  if (!spawn()) { doGameOver(); return; }
  state = S_PLAY;
  Serial.println("Game started");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Tetris v3 booting...");
  Serial.println("BOOT button (right): tap = move right (wraps), hold = rotate");

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(GRN_LED, OUTPUT);
  digitalWrite(GRN_LED, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.setContrast(60);

  neo.begin();
  neo.setBrightness(40);
  neo.setPixelColor(0, 0);
  neo.show();

  randomSeed(micros());
  drawTitle();
}

void loop() {
  Act a = pollBtn();

  switch (state) {
    case S_TITLE:
      if (a != A_NONE) startGame();
      drawTitle();
      break;

    case S_PLAY: {
      if (a == A_TAP)  moveRight();
      if (a == A_LONG) rotate();

      unsigned long now = millis();
      uint16_t spd = totalLines * 20;
      uint16_t fall = (FALL_START > spd) ? (FALL_START - spd) : FALL_MIN;
      if (fall < FALL_MIN) fall = FALL_MIN;
      if (now - lastFall >= fall) {
        lastFall = now;
        stepFall();
      }

      if (state == S_PLAY) drawGame();
      break;
    }

    case S_OVER:
      if (a == A_TAP) state = S_TITLE;
      drawOver();
      break;
  }

  tickLeds();
  delay(10);
}
