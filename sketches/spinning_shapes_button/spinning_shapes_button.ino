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

struct ShapeDef {
  const char *name;
  const Vec3 *verts;
  const Tri *tris;
  uint8_t vertexCount;
  uint8_t triCount;
  float scale;
};

const Vec3 CUBE_VERTS[] = {
  {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
  {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
};

const Tri CUBE_TRIS[] = {
  {0, 1, 2}, {0, 2, 3},
  {4, 6, 5}, {4, 7, 6},
  {0, 4, 5}, {0, 5, 1},
  {1, 5, 6}, {1, 6, 2},
  {2, 6, 7}, {2, 7, 3},
  {3, 7, 4}, {3, 4, 0}
};

const Vec3 TETRA_VERTS[] = {
  {1, 1, 1},
  {-1, -1, 1},
  {-1, 1, -1},
  {1, -1, -1}
};

const Tri TETRA_TRIS[] = {
  {0, 1, 2},
  {0, 3, 1},
  {0, 2, 3},
  {1, 3, 2}
};

const Vec3 OCTA_VERTS[] = {
  {1, 0, 0}, {-1, 0, 0},
  {0, 1, 0}, {0, -1, 0},
  {0, 0, 1}, {0, 0, -1}
};

const Tri OCTA_TRIS[] = {
  {0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},
  {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}
};

const Vec3 PYRAMID_VERTS[] = {
  {-1, -1, -0.8f}, {1, -1, -0.8f}, {1, 1, -0.8f}, {-1, 1, -0.8f},
  {0, 0, 1.6f}
};

const Tri PYRAMID_TRIS[] = {
  {0, 1, 2}, {0, 2, 3},
  {0, 4, 1}, {1, 4, 2}, {2, 4, 3}, {3, 4, 0}
};

const ShapeDef SHAPES[] = {
  {"CUBE", CUBE_VERTS, CUBE_TRIS, 8, 12, 24.0f},
  {"TETRA", TETRA_VERTS, TETRA_TRIS, 4, 4, 27.0f},
  {"OCTA", OCTA_VERTS, OCTA_TRIS, 6, 8, 26.0f},
  {"PYRAMID", PYRAMID_VERTS, PYRAMID_TRIS, 5, 6, 26.0f}
};

const uint8_t SHAPE_COUNT = sizeof(SHAPES) / sizeof(SHAPES[0]);

Vec3 rotated[8];
int16_t projX[8];
int16_t projY[8];
float viewZ[8];

FaceRender faces[12];
uint8_t faceCount = 0;

uint8_t currentShape = 0;
float angleX = 0.0f;
float angleY = 0.0f;
float angleZ = 0.0f;
unsigned long lastPressMs = 0;
bool buttonWasDown = false;

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

float edgeFunction(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  return (float)(x2 - x0) * (float)(y1 - y0) - (float)(y2 - y0) * (float)(x1 - x0);
}

void drawFilledTriangle(
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

      bool inside;
      if (area > 0) {
        inside = (w0 >= 0 && w1 >= 0 && w2 >= 0);
      } else {
        inside = (w0 <= 0 && w1 <= 0 && w2 <= 0);
      }

      if (inside) {
        if (shade == 3) {
          u8g2.drawPixel(x, y);
        } else if (shade == 2) {
          if (((x + y) & 1) == 0) {
            u8g2.drawPixel(x, y);
          }
        } else {
          if (((x + y) % 3) == 0) {
            u8g2.drawPixel(x, y);
          }
        }
      }
    }
  }
}

void sortFacesByDepth() {
  for (uint8_t i = 0; i < faceCount; i++) {
    for (uint8_t j = i + 1; j < faceCount; j++) {
      if (faces[i].depth > faces[j].depth) {
        FaceRender tmp = faces[i];
        faces[i] = faces[j];
        faces[j] = tmp;
      }
    }
  }
}

void buildFaces(const ShapeDef &shape) {
  faceCount = 0;
  const float cameraDistance = 2.05f;

  for (uint8_t i = 0; i < shape.vertexCount; i++) {
    rotatePoint(shape.verts[i], rotated[i], angleX, angleY, angleZ);
    float z = rotated[i].z + cameraDistance;
    if (z < 0.15f) z = 0.15f;

    float inv = shape.scale / z;
    projX[i] = (int16_t)(rotated[i].x * inv + (SCREEN_W / 2));
    projY[i] = (int16_t)(rotated[i].y * inv + (SCREEN_H / 2));
    viewZ[i] = z;
  }

  for (uint8_t i = 0; i < shape.triCount; i++) {
    Tri t = shape.tris[i];

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
    if (facing > 2.6f) {
      faces[faceCount].shade = 3;
    } else if (facing > 1.35f) {
      faces[faceCount].shade = 2;
    } else {
      faces[faceCount].shade = 1;
    }
    faceCount++;
  }
}

void drawShape() {
  const ShapeDef &shape = SHAPES[currentShape];

  buildFaces(shape);
  sortFacesByDepth();

  u8g2.clearBuffer();

  for (uint8_t i = 0; i < faceCount; i++) {
    drawFilledTriangle(
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

void nextShape() {
  currentShape = (currentShape + 1) % SHAPE_COUNT;
  Serial.print("Button press -> shape: ");
  Serial.println(SHAPES[currentShape].name);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Spinning Shapes starting...");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(60);

  Serial.print("Initial shape: ");
  Serial.println(SHAPES[currentShape].name);
}

void loop() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !buttonWasDown && (now - lastPressMs) > BUTTON_DEBOUNCE_MS) {
    lastPressMs = now;
    buttonWasDown = true;
    nextShape();
  }
  if (!pressed) {
    buttonWasDown = false;
  }

  angleX += 0.032f;
  angleY += 0.047f;
  angleZ += 0.022f;

  drawShape();
  delay(16);
}
