#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 6
#define BOOT_BTN_PIN 9

#define SCREEN_W 72
#define SCREEN_H 40

#define BUTTON_DEBOUNCE_MS 180

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Tri {
  uint8_t a;
  uint8_t b;
  uint8_t c;
};

struct FaceRender {
  int16_t x0, y0;
  int16_t x1, y1;
  int16_t x2, y2;
  float depth;
  uint8_t shade;
};

const Vec3 PYRAMID_VERTS[] = {
  {-1.1f, -1.1f, -0.75f},
  { 1.1f, -1.1f, -0.75f},
  { 1.1f,  1.1f, -0.75f},
  {-1.1f,  1.1f, -0.75f},
  { 0.0f,  0.0f,  1.75f}
};

const Tri PYRAMID_TRIS[] = {
  {0, 1, 2}, {0, 2, 3},
  {0, 4, 1}, {1, 4, 2}, {2, 4, 3}, {3, 4, 0}
};

const float SPEED_LEVELS[] = {0.35f, 0.7f, 1.1f, 1.55f, 2.1f};
const uint8_t SPEED_LEVEL_COUNT = sizeof(SPEED_LEVELS) / sizeof(SPEED_LEVELS[0]);

Vec3 rotated[5];
int16_t projX[5];
int16_t projY[5];
float viewZ[5];
FaceRender faces[6];
uint8_t faceCount = 0;

float angleX = 0.0f;
float angleY = 0.0f;
float angleZ = 0.0f;

uint8_t speedLevel = 0;
unsigned long lastPressMs = 0;
bool buttonWasDown = false;

float edgeFunction(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  return (float)(x2 - x0) * (float)(y1 - y0) - (float)(y2 - y0) * (float)(x1 - x0);
}

void rotatePoint(const Vec3 &in, Vec3 &out, float ax, float ay, float az) {
  float cx = cosf(ax), sx = sinf(ax);
  float cy = cosf(ay), sy = sinf(ay);
  float cz = cosf(az), sz = sinf(az);

  float x1 = in.x;
  float y1 = in.y * cx - in.z * sx;
  float z1 = in.y * sx + in.z * cx;

  float x2 = x1 * cy + z1 * sy;
  float y2 = y1;
  float z2 = -x1 * sy + z1 * cy;

  out.x = x2 * cz - y2 * sz;
  out.y = x2 * sz + y2 * cz;
  out.z = z2;
}

void drawShadedTriangle(
  int16_t x0, int16_t y0,
  int16_t x1, int16_t y1,
  int16_t x2, int16_t y2,
  uint8_t shade
) {
  int16_t minX = x0;
  int16_t maxX = x0;
  int16_t minY = y0;
  int16_t maxY = y0;

  if (x1 < minX) minX = x1;
  if (x2 < minX) minX = x2;
  if (x1 > maxX) maxX = x1;
  if (x2 > maxX) maxX = x2;
  if (y1 < minY) minY = y1;
  if (y2 < minY) minY = y2;
  if (y1 > maxY) maxY = y1;
  if (y2 > maxY) maxY = y2;

  if (maxX < 0 || maxY < 0 || minX >= SCREEN_W || minY >= SCREEN_H) {
    return;
  }

  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX >= SCREEN_W) maxX = SCREEN_W - 1;
  if (maxY >= SCREEN_H) maxY = SCREEN_H - 1;

  float area = edgeFunction(x0, y0, x1, y1, x2, y2);
  if (area == 0.0f) {
    return;
  }

  for (int16_t y = minY; y <= maxY; y++) {
    for (int16_t x = minX; x <= maxX; x++) {
      float w0 = edgeFunction(x1, y1, x2, y2, x, y);
      float w1 = edgeFunction(x2, y2, x0, y0, x, y);
      float w2 = edgeFunction(x0, y0, x1, y1, x, y);

      bool inside = (area > 0)
        ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
        : (w0 <= 0 && w1 <= 0 && w2 <= 0);

      if (!inside) {
        continue;
      }

      if (shade == 3) {
        u8g2.drawPixel(x, y);
      } else if (shade == 2) {
        if (((x + y) & 1) == 0) {
          u8g2.drawPixel(x, y);
        }
      } else {
        if (((x + 2 * y) % 3) == 0) {
          u8g2.drawPixel(x, y);
        }
      }
    }
  }
}

void sortFacesByDepth() {
  for (uint8_t i = 0; i < faceCount; i++) {
    for (uint8_t j = i + 1; j < faceCount; j++) {
      if (faces[i].depth > faces[j].depth) {
        FaceRender temp = faces[i];
        faces[i] = faces[j];
        faces[j] = temp;
      }
    }
  }
}

void buildFaces() {
  const float cameraDistance = 1.95f;
  const float projectionScale = 26.5f;
  faceCount = 0;

  for (uint8_t i = 0; i < 5; i++) {
    rotatePoint(PYRAMID_VERTS[i], rotated[i], angleX, angleY, angleZ);
    float z = rotated[i].z + cameraDistance;
    if (z < 0.15f) z = 0.15f;

    float inv = projectionScale / z;
    projX[i] = (int16_t)(rotated[i].x * inv + (SCREEN_W / 2));
    projY[i] = (int16_t)(rotated[i].y * inv + (SCREEN_H / 2));
    viewZ[i] = z;
  }

  for (uint8_t i = 0; i < 6; i++) {
    Tri t = PYRAMID_TRIS[i];

    Vec3 &v0 = rotated[t.a];
    Vec3 &v1 = rotated[t.b];
    Vec3 &v2 = rotated[t.c];

    Vec3 e1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 e2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

    Vec3 n = {
      e1.y * e2.z - e1.z * e2.y,
      e1.z * e2.x - e1.x * e2.z,
      e1.x * e2.y - e1.y * e2.x
    };

    if (n.z >= 0.0f) {
      continue;
    }

    faces[faceCount].x0 = projX[t.a];
    faces[faceCount].y0 = projY[t.a];
    faces[faceCount].x1 = projX[t.b];
    faces[faceCount].y1 = projY[t.b];
    faces[faceCount].x2 = projX[t.c];
    faces[faceCount].y2 = projY[t.c];
    faces[faceCount].depth = (viewZ[t.a] + viewZ[t.b] + viewZ[t.c]) / 3.0f;

    float facing = -n.z;
    if (facing > 3.0f) {
      faces[faceCount].shade = 3;
    } else if (facing > 1.5f) {
      faces[faceCount].shade = 2;
    } else {
      faces[faceCount].shade = 1;
    }

    faceCount++;
  }
}

void drawFrame() {
  buildFaces();
  sortFacesByDepth();

  u8g2.clearBuffer();

  for (uint8_t i = 0; i < faceCount; i++) {
    drawShadedTriangle(
      faces[i].x0, faces[i].y0,
      faces[i].x1, faces[i].y1,
      faces[i].x2, faces[i].y2,
      faces[i].shade
    );
  }

  u8g2.setDrawColor(0);
  for (uint8_t i = 0; i < faceCount; i++) {
    u8g2.drawLine(faces[i].x0, faces[i].y0, faces[i].x1, faces[i].y1);
    u8g2.drawLine(faces[i].x1, faces[i].y1, faces[i].x2, faces[i].y2);
    u8g2.drawLine(faces[i].x2, faces[i].y2, faces[i].x0, faces[i].y0);
  }
  u8g2.setDrawColor(1);

  u8g2.sendBuffer();
}

void advanceSpeedLevel() {
  speedLevel = (speedLevel + 1) % SPEED_LEVEL_COUNT;
  Serial.print("Speed level -> ");
  Serial.print(speedLevel + 1);
  Serial.print("/");
  Serial.print(SPEED_LEVEL_COUNT);
  Serial.print(" (mult=");
  Serial.print(SPEED_LEVELS[speedLevel], 2);
  Serial.println(")");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Pyramid spin speed demo starting...");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(60);

  Serial.print("Default speed multiplier: ");
  Serial.println(SPEED_LEVELS[speedLevel], 2);
}

void loop() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !buttonWasDown && (now - lastPressMs) > BUTTON_DEBOUNCE_MS) {
    lastPressMs = now;
    buttonWasDown = true;
    advanceSpeedLevel();
  }
  if (!pressed) {
    buttonWasDown = false;
  }

  float s = SPEED_LEVELS[speedLevel];
  angleX += 0.009f * s;
  angleY += 0.014f * s;
  angleZ += 0.006f * s;

  drawFrame();
  delay(16);
}
