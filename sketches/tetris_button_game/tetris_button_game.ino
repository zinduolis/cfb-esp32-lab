#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 6
#define BOOT_BTN_PIN 9

#define SCREEN_W 72
#define SCREEN_H 40

#define CELL 4
#define BOARD_W 10
#define BOARD_H 10
#define BOARD_X 0
#define BOARD_Y 0

#define BUTTON_DEBOUNCE_MS 150
#define LONG_PRESS_MS 320
#define FALL_MS_START 650
#define FALL_MS_MIN 180

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);

enum GameState {
  STATE_START,
  STATE_PLAYING,
  STATE_GAME_OVER
};

struct Piece {
  int8_t type;
  int8_t rot;
  int8_t x;
  int8_t y;
};

// 7 tetrominoes, 4 rotations, 4 blocks, (x,y)
const int8_t SHAPES[7][4][4][2] = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  // O
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}
};

uint8_t board[BOARD_H][BOARD_W];
Piece cur;
GameState gameState = STATE_START;

bool buttonWasDown = false;
unsigned long pressStartMs = 0;
unsigned long lastPressEventMs = 0;
bool longHandled = false;
unsigned long lastFallMs = 0;
uint16_t score = 0;
uint16_t lines = 0;

bool validAt(int8_t type, int8_t rot, int8_t px, int8_t py) {
  for (int i = 0; i < 4; i++) {
    int x = px + SHAPES[type][rot][i][0];
    int y = py + SHAPES[type][rot][i][1];
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return false;
    if (board[y][x]) return false;
  }
  return true;
}

void drawCell(int x, int y, bool filled) {
  int px = BOARD_X + x * CELL;
  int py = BOARD_Y + y * CELL;
  if (filled) {
    u8g2.drawBox(px, py, CELL, CELL);
  } else {
    u8g2.drawFrame(px, py, CELL, CELL);
  }
}

void clearBoard() {
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = 0;
    }
  }
}

bool spawnPiece() {
  cur.type = random(7);
  cur.rot = random(4);
  cur.x = 3;
  cur.y = 0;
  return validAt(cur.type, cur.rot, cur.x, cur.y);
}

void startGame() {
  clearBoard();
  score = 0;
  lines = 0;
  lastFallMs = millis();
  if (!spawnPiece()) {
    gameState = STATE_GAME_OVER;
    return;
  }
  gameState = STATE_PLAYING;
  Serial.println("Tetris started.");
}

void lockPiece() {
  for (int i = 0; i < 4; i++) {
    int x = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int y = cur.y + SHAPES[cur.type][cur.rot][i][1];
    if (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H) {
      board[y][x] = 1;
    }
  }
}

void clearLines() {
  int cleared = 0;
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++) {
      if (!board[y][x]) {
        full = false;
        break;
      }
    }
    if (full) {
      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < BOARD_W; x++) {
          board[yy][x] = board[yy - 1][x];
        }
      }
      for (int x = 0; x < BOARD_W; x++) board[0][x] = 0;
      cleared++;
      y++;
    }
  }

  if (cleared > 0) {
    lines += cleared;
    score += (cleared * cleared) * 10;
    Serial.print("Lines cleared: ");
    Serial.print(cleared);
    Serial.print(" total lines=");
    Serial.print(lines);
    Serial.print(" score=");
    Serial.println(score);
  }
}

void gameOver() {
  gameState = STATE_GAME_OVER;
  Serial.print("Game over. Score=");
  Serial.print(score);
  Serial.print(" Lines=");
  Serial.println(lines);
}

void rotatePiece() {
  int8_t nextRot = (cur.rot + 1) % 4;
  if (validAt(cur.type, nextRot, cur.x, cur.y)) {
    cur.rot = nextRot;
  }
}

void hardDrop() {
  while (validAt(cur.type, cur.rot, cur.x, cur.y + 1)) {
    cur.y++;
  }
  lockPiece();
  clearLines();
  if (!spawnPiece()) gameOver();
}

void stepFall() {
  if (validAt(cur.type, cur.rot, cur.x, cur.y + 1)) {
    cur.y++;
  } else {
    lockPiece();
    clearLines();
    if (!spawnPiece()) {
      gameOver();
    }
  }
}

int centeredX(const char *txt) {
  return (SCREEN_W - u8g2.getStrWidth(txt)) / 2;
}

void drawStart() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("TETRIS"), 12, "TETRIS");
  u8g2.drawStr(centeredX("Press To Start"), 26, "Press To Start");
  u8g2.sendBuffer();
}

void drawGameOver() {
  char scoreTxt[16];
  snprintf(scoreTxt, sizeof(scoreTxt), "Score %u", score);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("Game Over"), 14, "Game Over");
  u8g2.drawStr(centeredX(scoreTxt), 24, scoreTxt);
  u8g2.drawStr(centeredX("Press"), 36, "Press");
  u8g2.sendBuffer();
}

void drawPlay() {
  u8g2.clearBuffer();

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) drawCell(x, y, true);
    }
  }

  for (int i = 0; i < 4; i++) {
    int x = cur.x + SHAPES[cur.type][cur.rot][i][0];
    int y = cur.y + SHAPES[cur.type][cur.rot][i][1];
    if (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H) {
      drawCell(x, y, true);
    }
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  char s1[12], s2[12];
  snprintf(s1, sizeof(s1), "S%u", score);
  snprintf(s2, sizeof(s2), "L%u", lines);
  u8g2.drawStr(42, 8, s1);
  u8g2.drawStr(42, 16, s2);
  u8g2.drawStr(42, 26, "tap rot");
  u8g2.drawStr(42, 34, "hold drp");

  u8g2.sendBuffer();
}

void handleButton() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !buttonWasDown) {
    buttonWasDown = true;
    pressStartMs = now;
    longHandled = false;
  }

  if (pressed && buttonWasDown && gameState == STATE_PLAYING && !longHandled &&
      (now - pressStartMs) >= LONG_PRESS_MS && (now - lastPressEventMs) > BUTTON_DEBOUNCE_MS) {
    longHandled = true;
    lastPressEventMs = now;
    hardDrop();
  }

  if (!pressed && buttonWasDown) {
    if (!longHandled && (now - lastPressEventMs) > BUTTON_DEBOUNCE_MS) {
      lastPressEventMs = now;
      if (gameState == STATE_START) {
        startGame();
      } else if (gameState == STATE_PLAYING) {
        rotatePiece();
      } else {
        gameState = STATE_START;
      }
    }
    buttonWasDown = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Tetris booting...");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(60);

  randomSeed(micros());
  drawStart();
}

void loop() {
  handleButton();

  if (gameState == STATE_START) {
    drawStart();
    delay(20);
    return;
  }

  if (gameState == STATE_GAME_OVER) {
    drawGameOver();
    delay(20);
    return;
  }

  unsigned long now = millis();
  uint16_t speedup = lines * 20;
  uint16_t fallMs = (FALL_MS_START > speedup) ? (FALL_MS_START - speedup) : FALL_MS_MIN;
  if (fallMs < FALL_MS_MIN) fallMs = FALL_MS_MIN;

  if (now - lastFallMs >= fallMs) {
    lastFallMs = now;
    stepFall();
  }

  if (gameState == STATE_PLAYING) {
    drawPlay();
  }
  delay(10);
}
