#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 6
#define BOOT_BTN_PIN 9
#define BLUE_LED_PIN 8

#define SCREEN_W 72
#define SCREEN_H 40

#define CELL_SIZE 4
#define GRID_W (SCREEN_W / CELL_SIZE)
#define GRID_H (SCREEN_H / CELL_SIZE)
#define MAX_SNAKE_LEN (GRID_W * GRID_H)

#define BUTTON_DEBOUNCE_MS 150
#define SNAKE_STEP_MS 170

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);

enum GameState {
  STATE_START,
  STATE_PLAYING,
  STATE_GAME_OVER
};

enum Direction {
  DIR_RIGHT,
  DIR_DOWN,
  DIR_LEFT,
  DIR_UP
};

struct Cell {
  int8_t x;
  int8_t y;
};

Cell snake[MAX_SNAKE_LEN];
uint16_t snakeLen = 0;
Direction dir = DIR_RIGHT;
Cell food = {0, 0};
GameState gameState = STATE_START;

bool buttonWasDown = false;
unsigned long lastPressMs = 0;
unsigned long lastStepMs = 0;
uint16_t score = 0;

bool isOnSnake(int8_t x, int8_t y) {
  for (uint16_t i = 0; i < snakeLen; i++) {
    if (snake[i].x == x && snake[i].y == y) {
      return true;
    }
  }
  return false;
}

void spawnFood() {
  if (snakeLen >= MAX_SNAKE_LEN) {
    return;
  }

  int16_t tries = 0;
  do {
    food.x = random(GRID_W);
    food.y = random(GRID_H);
    tries++;
  } while (isOnSnake(food.x, food.y) && tries < 2000);
}

void resetGame() {
  snakeLen = 3;
  snake[0] = {5, 5};
  snake[1] = {4, 5};
  snake[2] = {3, 5};
  dir = DIR_RIGHT;
  score = 0;
  lastStepMs = millis();
  spawnFood();
  gameState = STATE_PLAYING;
  Serial.println("Game started.");
}

void turnClockwise() {
  dir = (Direction)((dir + 1) % 4);
}

int centeredX(const char *text) {
  return (SCREEN_W - u8g2.getStrWidth(text)) / 2;
}

void drawStartScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("Press To Start"), 20, "Press To Start");
  u8g2.sendBuffer();
}

void drawGameOverScreen() {
  char scoreText[20];
  snprintf(scoreText, sizeof(scoreText), "Score: %u", score);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredX("Game Over"), 16, "Game Over");
  u8g2.drawStr(centeredX(scoreText), 28, scoreText);
  u8g2.sendBuffer();
}

void drawGame() {
  u8g2.clearBuffer();

  // food pixel (small bright target)
  int foodPx = food.x * CELL_SIZE + (CELL_SIZE / 2);
  int foodPy = food.y * CELL_SIZE + (CELL_SIZE / 2);
  u8g2.drawPixel(foodPx, foodPy);

  // snake body cells
  for (uint16_t i = 0; i < snakeLen; i++) {
    int px = snake[i].x * CELL_SIZE;
    int py = snake[i].y * CELL_SIZE;
    if (i == 0) {
      u8g2.drawBox(px, py, CELL_SIZE, CELL_SIZE);
    } else {
      u8g2.drawFrame(px, py, CELL_SIZE, CELL_SIZE);
    }
  }

  u8g2.sendBuffer();
}

void gameOver() {
  gameState = STATE_GAME_OVER;
  Serial.print("Game over. Score: ");
  Serial.println(score);
}

void showStartScreen() {
  gameState = STATE_START;
  Serial.println("Back to Press To Start screen.");
}

void updateBlueFlash(unsigned long now) {
  // On this board, blue LED is active-low (LOW = on).
  if (gameState != STATE_PLAYING || score == 0) {
    digitalWrite(BLUE_LED_PIN, HIGH);
    return;
  }

  uint16_t level = score;
  if (level > 20) level = 20;

  int intervalMs = 900 - (int)(35 * level);  // slower at first, faster as score rises
  if (intervalMs < 120) intervalMs = 120;

  int onMs = (intervalMs / 6) + (int)(level * 4);  // brighter feel via longer on-time
  if (onMs < 30) onMs = 30;
  if (onMs > intervalMs - 10) onMs = intervalMs - 10;

  bool ledOn = (now % (unsigned long)intervalMs) < (unsigned long)onMs;
  digitalWrite(BLUE_LED_PIN, ledOn ? LOW : HIGH);
}

void stepSnake() {
  Cell next = snake[0];
  if (dir == DIR_RIGHT) next.x++;
  if (dir == DIR_DOWN)  next.y++;
  if (dir == DIR_LEFT)  next.x--;
  if (dir == DIR_UP)    next.y--;

  // wall collision
  if (next.x < 0 || next.x >= GRID_W || next.y < 0 || next.y >= GRID_H) {
    gameOver();
    return;
  }

  bool grow = (next.x == food.x && next.y == food.y);
  uint16_t checkLen = grow ? snakeLen : (snakeLen - 1);

  // self collision
  for (uint16_t i = 0; i < checkLen; i++) {
    if (snake[i].x == next.x && snake[i].y == next.y) {
      gameOver();
      return;
    }
  }

  if (grow) {
    if (snakeLen < MAX_SNAKE_LEN) {
      for (int i = snakeLen; i > 0; i--) {
        snake[i] = snake[i - 1];
      }
      snake[0] = next;
      snakeLen++;
      score++;
      Serial.print("Food eaten. Score: ");
      Serial.println(score);
      spawnFood();
    }
  } else {
    for (int i = snakeLen - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    snake[0] = next;
  }
}

void handleButton() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !buttonWasDown && (now - lastPressMs) > BUTTON_DEBOUNCE_MS) {
    buttonWasDown = true;
    lastPressMs = now;

    if (gameState == STATE_START) {
      resetGame();
    } else if (gameState == STATE_PLAYING) {
      turnClockwise();
    } else if (gameState == STATE_GAME_OVER) {
      showStartScreen();
    }
  }

  if (!pressed) {
    buttonWasDown = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Snake game booting...");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(60);

  randomSeed(micros());
  drawStartScreen();
}

void loop() {
  unsigned long now = millis();
  handleButton();
  updateBlueFlash(now);

  if (gameState == STATE_START) {
    drawStartScreen();
    delay(20);
    return;
  }

  if (gameState == STATE_GAME_OVER) {
    drawGameOverScreen();
    delay(20);
    return;
  }

  if (now - lastStepMs >= SNAKE_STEP_MS) {
    lastStepMs = now;
    stepSnake();
  }

  if (gameState == STATE_PLAYING) {
    drawGame();
  }

  delay(10);
}
