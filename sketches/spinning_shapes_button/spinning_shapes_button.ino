#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 6
#define BOOT_BTN_PIN 9

#define SCREEN_W 72
#define SCREEN_H 40

#define BUTTON_DEBOUNCE_MS 180
#define MAX_VERTS 12
#define MAX_TRIS  20

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);

struct Vec3 { float x, y, z; };
struct Tri  { uint8_t a, b, c; };

struct FaceRender {
  int16_t x0, y0, x1, y1, x2, y2;
  float   depth;
  uint8_t shade;  // 1–5
};

struct ShapeDef {
  const char *name;
  const Vec3 *verts;
  const Tri  *tris;
  uint8_t vertexCount;
  uint8_t triCount;
  float   scale;
};

// --------------- geometry ---------------

const Vec3 CUBE_V[] = {
  {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
  {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
};
const Tri CUBE_T[] = {
  {0,1,2},{0,2,3}, {4,6,5},{4,7,6},
  {0,4,5},{0,5,1}, {1,5,6},{1,6,2},
  {2,6,7},{2,7,3}, {3,7,4},{3,4,0}
};

const Vec3 TETRA_V[] = {
  {1,1,1},{-1,-1,1},{-1,1,-1},{1,-1,-1}
};
const Tri TETRA_T[] = {
  {0,1,2},{0,3,1},{0,2,3},{1,3,2}
};

const Vec3 OCTA_V[] = {
  {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
};
const Tri OCTA_T[] = {
  {0,2,4},{2,1,4},{1,3,4},{3,0,4},
  {2,0,5},{1,2,5},{3,1,5},{0,3,5}
};

const Vec3 PYRA_V[] = {
  {-1,-1,-0.7f},{1,-1,-0.7f},{1,1,-0.7f},{-1,1,-0.7f},
  {0,0,1.5f}
};
const Tri PYRA_T[] = {
  {0,1,2},{0,2,3},
  {0,4,1},{1,4,2},{2,4,3},{3,4,0}
};

const Vec3 DIAM_V[] = {
  {-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1},
  {0,1.4f,0},{0,-1.4f,0}
};
const Tri DIAM_T[] = {
  {0,1,4},{1,2,4},{2,3,4},{3,0,4},
  {1,0,5},{2,1,5},{3,2,5},{0,3,5}
};

const ShapeDef SHAPES[] = {
  {"CUBE",    CUBE_V,  CUBE_T,  8,12, 24.0f},
  {"TETRA",   TETRA_V, TETRA_T, 4, 4, 27.0f},
  {"OCTA",    OCTA_V,  OCTA_T,  6, 8, 26.0f},
  {"PYRAMID", PYRA_V,  PYRA_T,  5, 6, 26.0f},
  {"DIAMOND", DIAM_V,  DIAM_T,  6, 8, 23.0f}
};
const uint8_t SHAPE_COUNT = sizeof(SHAPES) / sizeof(SHAPES[0]);

// --------------- work buffers ---------------

Vec3    rotated[MAX_VERTS];
int16_t projX[MAX_VERTS];
int16_t projY[MAX_VERTS];
float   viewZ[MAX_VERTS];

FaceRender faces[MAX_TRIS];
uint8_t faceCount = 0;

uint8_t  currentShape = 0;
float    angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
unsigned long lastPressMs = 0;
bool     buttonWasDown = false;

// Light direction (normalised): upper-right, towards camera
static const float LX =  0.41f;
static const float LY = -0.41f;
static const float LZ = -0.82f;

// Camera distance — shared; per-shape scale handles sizing
static const float CAM_DIST = 2.1f;

// --------------- math ---------------

void rotatePoint(const Vec3 &in, Vec3 &out, float ax, float ay, float az) {
  float cx = cosf(ax), sx = sinf(ax);
  float cy = cosf(ay), sy = sinf(ay);
  float cz = cosf(az), sz = sinf(az);
  float y1 = in.y * cx - in.z * sx;
  float z1 = in.y * sx + in.z * cx;
  float x2 = in.x * cy + z1 * sy;
  float z2 = -in.x * sy + z1 * cy;
  out.x = x2 * cz - y1 * sz;
  out.y = x2 * sz + y1 * cz;
  out.z = z2;
}

float fastInvSqrt(float x) {
  float xh = 0.5f * x;
  int32_t i;
  memcpy(&i, &x, 4);
  i = 0x5f3759df - (i >> 1);
  float y;
  memcpy(&y, &i, 4);
  y *= (1.5f - xh * y * y);
  return y;
}

// Integer edge function — much faster on ESP32-C3 (no FPU)
static inline int32_t edgeFn(int16_t x0, int16_t y0,
                              int16_t x1, int16_t y1,
                              int16_t px, int16_t py) {
  return (int32_t)(px - x0) * (int32_t)(y1 - y0)
       - (int32_t)(py - y0) * (int32_t)(x1 - x0);
}

// --------------- opaque dithered triangle ---------------
// Single-pass: writes EVERY pixel inside the triangle
// (set white if dithered-on, clear black if dithered-off).
// Correct painter's algorithm on a 1-bit display.

static const uint8_t BAYER4[4][4] = {
  { 0,  8,  2, 10},
  {12,  4, 14,  6},
  { 3, 11,  1,  9},
  {15,  7, 13,  5}
};

void drawOpaqueTriangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    uint8_t shade)
{
  // Bounding box
  int16_t minX = x0, maxX = x0, minY = y0, maxY = y0;
  if (x1 < minX) minX = x1;  if (x1 > maxX) maxX = x1;
  if (x2 < minX) minX = x2;  if (x2 > maxX) maxX = x2;
  if (y1 < minY) minY = y1;  if (y1 > maxY) maxY = y1;
  if (y2 < minY) minY = y2;  if (y2 > maxY) maxY = y2;

  if (maxX < 0 || maxY < 0 || minX >= SCREEN_W || minY >= SCREEN_H) return;
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX >= SCREEN_W) maxX = SCREEN_W - 1;
  if (maxY >= SCREEN_H) maxY = SCREEN_H - 1;

  int32_t area = edgeFn(x0, y0, x1, y1, x2, y2);
  if (area == 0) return;

  // Shade thresholds (0–16 range over 4×4 Bayer)
  static const uint8_t THR[] = {0, 2, 5, 9, 13, 16};
  uint8_t thr = THR[shade];

  // Direct buffer access — one pass, set or clear each pixel
  uint8_t *buf = u8g2.getBufferPtr();
  uint16_t rowBytes = (uint16_t)u8g2.getBufferTileWidth() * 8;

  for (int16_t y = minY; y <= maxY; y++) {
    uint16_t pageOff = ((uint16_t)(y >> 3)) * rowBytes;
    uint8_t  bitMask = 1 << (y & 7);

    for (int16_t x = minX; x <= maxX; x++) {
      int32_t w0 = edgeFn(x1, y1, x2, y2, x, y);
      int32_t w1 = edgeFn(x2, y2, x0, y0, x, y);
      int32_t w2 = edgeFn(x0, y0, x1, y1, x, y);

      bool inside = (area > 0)
        ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
        : (w0 <= 0 && w1 <= 0 && w2 <= 0);

      if (inside) {
        uint16_t idx = pageOff + x;
        if (thr >= 16 || BAYER4[y & 3][x & 3] < thr) {
          buf[idx] |= bitMask;
        } else {
          buf[idx] &= ~bitMask;
        }
      }
    }
  }
}

// --------------- face builder ---------------

void buildFaces(const ShapeDef &shape) {
  faceCount = 0;

  for (uint8_t i = 0; i < shape.vertexCount; i++) {
    rotatePoint(shape.verts[i], rotated[i], angleX, angleY, angleZ);
    float z = rotated[i].z + CAM_DIST;
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

    Vec3 e1 = {v1.x-v0.x, v1.y-v0.y, v1.z-v0.z};
    Vec3 e2 = {v2.x-v0.x, v2.y-v0.y, v2.z-v0.z};

    float nx = e1.y*e2.z - e1.z*e2.y;
    float ny = e1.z*e2.x - e1.x*e2.z;
    float nz = e1.x*e2.y - e1.y*e2.x;

    if (nz >= 0.0f) continue;  // back-face cull

    // Normalise
    float invLen = fastInvSqrt(nx*nx + ny*ny + nz*nz);
    nx *= invLen; ny *= invLen; nz *= invLen;

    // Lambertian + ambient
    float dot = nx*LX + ny*LY + nz*LZ;
    float bri = dot * 0.78f + 0.22f;
    if (bri < 0.0f) bri = 0.0f;
    if (bri > 1.0f) bri = 1.0f;

    uint8_t shade;
    if      (bri > 0.82f) shade = 5;
    else if (bri > 0.62f) shade = 4;
    else if (bri > 0.38f) shade = 3;
    else if (bri > 0.15f) shade = 2;
    else                  shade = 1;

    faces[faceCount].x0 = projX[t.a]; faces[faceCount].y0 = projY[t.a];
    faces[faceCount].x1 = projX[t.b]; faces[faceCount].y1 = projY[t.b];
    faces[faceCount].x2 = projX[t.c]; faces[faceCount].y2 = projY[t.c];
    faces[faceCount].depth = (viewZ[t.a]+viewZ[t.b]+viewZ[t.c]) / 3.0f;
    faces[faceCount].shade = shade;
    faceCount++;
  }
}

// --------------- depth sort (back-to-front) ---------------

void sortFaces() {
  for (uint8_t i = 1; i < faceCount; i++) {
    FaceRender key = faces[i];
    int8_t j = i - 1;
    while (j >= 0 && faces[j].depth < key.depth) {
      faces[j+1] = faces[j];
      j--;
    }
    faces[j+1] = key;
  }
}

// --------------- draw ---------------

void drawShape() {
  const ShapeDef &shape = SHAPES[currentShape];
  buildFaces(shape);
  sortFaces();

  u8g2.clearBuffer();

  // Opaque dithered fill — back to front
  for (uint8_t i = 0; i < faceCount; i++) {
    drawOpaqueTriangle(
      faces[i].x0, faces[i].y0,
      faces[i].x1, faces[i].y1,
      faces[i].x2, faces[i].y2,
      faces[i].shade
    );
  }

  // Black edge lines for clean face separation
  u8g2.setDrawColor(0);
  for (uint8_t i = 0; i < faceCount; i++) {
    u8g2.drawLine(faces[i].x0, faces[i].y0, faces[i].x1, faces[i].y1);
    u8g2.drawLine(faces[i].x1, faces[i].y1, faces[i].x2, faces[i].y2);
    u8g2.drawLine(faces[i].x2, faces[i].y2, faces[i].x0, faces[i].y0);
  }
  u8g2.setDrawColor(1);

  u8g2.sendBuffer();
}

// --------------- button ---------------

void nextShape() {
  currentShape = (currentShape + 1) % SHAPE_COUNT;
  Serial.print("Shape -> ");
  Serial.println(SHAPES[currentShape].name);
}

// --------------- setup & loop ---------------

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Spinning Shapes v3 starting...");

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
  if (!pressed) buttonWasDown = false;

  // Smooth constant rotation
  angleX += 0.032f;
  angleY += 0.047f;
  angleZ += 0.022f;

  drawShape();
  delay(16);
}
