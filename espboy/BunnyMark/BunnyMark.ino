// BunnyMark for the ESPboy: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, UP five hundred, DOWN a thousand, each once
// when the button is let go; A + B takes them all away again. The right side button
// switches the frame buffer between 16, 8 and 1 bpp. Every bunny moves and bounces
// off the box each frame, the screen shows the frame rate and the count. See README.md.
//
// Two builds of one sketch: LOVYANGFX 1 uses LovyanGFX, 0 TFT_eSPI (set below or with
// -DLOVYANGFX=0). The bunnies are not drawn by either library: they are written
// straight into the frame sprite's memory, with a version of the bunny made for each
// depth (bunny.h), so nothing is converted and no library call is made per bunny.
// The library clears the frame, draws the box and the text, and sends the frame to the
// panel, which is where the two builds differ. lib/ is the ESPboy library
// (ESPboyInit, the MCP23017 expander, the LED, and the LovyanGFX panel setup);
// TFT_eSPI takes its ESPboy panel setup from its own User_Setup.h.
//
// What differs from the Playdate version, and why:
// - 128x128 instead of 400x240: the bunny is 16x16 (half size, tools/make_sprites.py)
//   and moves at half the speed, as on the CHGame.
// - The frame is a full-screen sprite (32 KB at 16 bpp) in the ESP8266's 80 KB of
//   RAM, which leaves room for MAX_BUNNIES. To fit as many as it can, a bunny is 3 bytes:
//   its position in quarter pixels (x and y 9 bits each) and its speed on each axis as a
//   3-bit index into SPEEDS, a half to two and a half pixels a frame either way (integers,
//   the ESP8266 has no FPU), as on the CHGame.
//
// BUNNYMARK_SERIAL 1 (only for testing, -DBUNNYMARK_SERIAL=1) makes the serial port
// (115200 baud) print a line a second with the frame rate and take one-letter
// commands, so it can be driven from a PC: a +1, b +100, u +500, d +1000, r back to 0,
// m next buffer depth. The normal builds send and read nothing.

#ifndef LOVYANGFX
#define LOVYANGFX 1
#endif
#ifndef BUNNYMARK_SERIAL
#define BUNNYMARK_SERIAL 0
#endif

#if LOVYANGFX
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "lib/LGFX_ESP8266_ESPboy.hpp"
typedef LGFX_Sprite FrameSprite;
#define LIBRARY_NAME "LGFX"
// the sprite's pixels; both libraries lay them out the same way: 16 bpp byte swapped
// RGB565, 8 bpp RGB332, 1 bpp packed most significant bit first, rows back to back
#define FRAME_PIXELS() ((uint8_t *)frame.getBuffer())
#else
#include <TFT_eSPI.h>
typedef TFT_eSprite FrameSprite;
#define LIBRARY_NAME "eSPI"
#define FRAME_PIXELS() ((uint8_t *)frame.getPointer())
#endif
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"
#include "lib/ESPboyMCP.cpp"
#include "lib/ESPboyLED.cpp"

#include "bunny.h"

#define SCREEN_W 128
#define SCREEN_H 128
// with the 16 bpp frame (32 KB) about 8 KB of heap is left
#ifndef MAX_BUNNIES
#define MAX_BUNNIES 3000
#endif

static const uint16_t WHITE = 0xFFFF, BLACK = 0x0000;

// the box the bunnies bounce in, below the two status lines (the Playdate's is 5,40 390x195)
#define BOX_X 2
#define BOX_Y 22
#define BOX_W 124
#define BOX_H 104
// in quarter pixels
#define MIN_X (BOX_X * 4)
#define MAX_X ((BOX_X + BOX_W - BUNNY_W) * 4)
#define MIN_Y (BOX_Y * 4)
#define MAX_Y ((BOX_Y + BOX_H - BUNNY_H) * 4)

static ESPboyInit myESPboy;
static FrameSprite frame(&myESPboy.tft);

// quarter pixels a frame, up to 2.5 px (half the Playdate's 5, as the bunny is half the
// size); SPEEDS[7 - i] is -SPEEDS[i], so turning round is 7 - i
static const int8_t SPEEDS[8] = { -10, -7, -4, -2, 2, 4, 7, 10 };

// 3 bytes: x's and y's low 8 bits, then their 9th bits (bits 0 and 1), the x speed's index
// (bits 2-4) and the y speed's (bits 5-7)
struct Bunny {
    uint8_t xLow;
    uint8_t yLow;
    uint8_t bits;
};

static Bunny bunnies[MAX_BUNNIES];
static uint16_t bunnyCount = 0;

static uint32_t rng = 1;
static inline uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

// One bunny unpacked, moved a frame and packed again. It turns round where it passes the box's
// edge and is put back on it: in 9 bits a bunny past the edge would wrap round
static inline void moveBunny(Bunny &b) {
    int x = b.xLow | ((b.bits & 1) << 8);
    int y = b.yLow | ((b.bits & 2) << 7);
    int vx = (b.bits >> 2) & 7, vy = (b.bits >> 5) & 7;
    x += SPEEDS[vx];
    y += SPEEDS[vy];
    if (x > MAX_X) { x = MAX_X; vx = 7 - vx; }
    else if (x < MIN_X) { x = MIN_X; vx = 7 - vx; }
    if (y > MAX_Y) { y = MAX_Y; vy = 7 - vy; }
    else if (y < MIN_Y) { y = MIN_Y; vy = 7 - vy; }
    b.xLow = (uint8_t)x;
    b.yLow = (uint8_t)y;
    b.bits = (uint8_t)((x >> 8) | ((y >> 8) << 1) | (vx << 2) | (vy << 5));
}

static void addBunnies(int n) {
    const int x = (BOX_X + (BOX_W - BUNNY_W) / 2) * 4;
    const int y = (BOX_Y + (BOX_H - BUNNY_H) / 2) * 4;
    while (n-- > 0 && bunnyCount < MAX_BUNNIES) {
        Bunny &b = bunnies[bunnyCount++];
        // a random speed on each axis, the Playdate's spread of them over the table
        const int vx = rnd() & 7, vy = rnd() & 7;
        b.xLow = (uint8_t)x;
        b.yLow = (uint8_t)y;
        b.bits = (uint8_t)((x >> 8) | ((y >> 8) << 1) | (vx << 2) | (vy << 5));
        // With 8 speeds an axis there are only 64 paths, and bunnies on the same one would lie
        // exactly on top of each other: each starts a random 0-63 frames along its own path
        for (int steps = rnd() & 63; steps; steps--)
            moveBunny(b);
    }
}

static void updateBunnies() {
    for (uint16_t i = 0; i < bunnyCount; i++)
        moveBunny(bunnies[i]);
}

// ---------------------------------------------------------------------------
// The frame buffer and its depth
// ---------------------------------------------------------------------------

// The frame sprite's colour depth. Drawing into a deeper sprite writes more bytes;
// sending it to the panel is always 16 bpp, so an 8 or 1 bpp frame has to be turned
// into 16 bit pixels on the way out
static const uint8_t DEPTHS[3] = { 16, 8, 1 };
static uint8_t depth = 0;
static bool frameOk = false;

#if LOVYANGFX
// LovyanGFX would send an 8 or 1 bpp sprite through its general pixel converter,
// pixel by pixel, and its 1 bpp palette push leaves the ESPboy's panel empty. So
// those two go out as rows of display-order RGB565 instead, which takes the raw byte
// path. The table
// is the library's own RGB332 to RGB565 conversion
static uint16_t rgb332[256];
#endif

static uint32_t fpsFrames = 0, fpsStart = 0;
static uint16_t fps = 0;

static void restartFps() {
    fpsFrames = 0;
    fpsStart = millis();
}

static void setDepth(uint8_t index) {
    depth = index;
    frame.deleteSprite();
    frame.setColorDepth(DEPTHS[depth]);
    frameOk = frame.createSprite(SCREEN_W, SCREEN_H) != nullptr;
#if BUNNYMARK_SERIAL
    if (!frameOk)
        Serial.printf("no memory for a %d bpp frame\n", DEPTHS[depth]);
#endif
    restartFps();
}

static void present() {
#if LOVYANGFX
    if (DEPTHS[depth] != 16) {
        const uint8_t *src = (const uint8_t *)frame.getBuffer();
        uint16_t line[SCREEN_W];
        myESPboy.tft.startWrite();
        myESPboy.tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
        for (int y = 0; y < SCREEN_H; y++) {
            if (DEPTHS[depth] == 1) {
                // most significant bit first; set is white, clear black (the same either byte order)
                for (int x = 0; x < SCREEN_W; x += 8) {
                    uint8_t bits = *src++;
                    for (uint8_t b = 0; b < 8; b++, bits <<= 1)
                        line[x + b] = (bits & 0x80) ? WHITE : BLACK;
                }
            } else {
                for (int x = 0; x < SCREEN_W; x++)
                    line[x] = rgb332[*src++];
            }
            myESPboy.tft.writePixels((const lgfx::swap565_t *)line, SCREEN_W);
        }
        myESPboy.tft.endWrite();
        return;
    }
#endif
    frame.pushSprite(0, 0);
}

// a bunny with its top left at (x0, y0), written into the frame sprite's memory in the
// form made for its depth. A bunny can be a pixel past the box when it turns, so it is
// kept on the screen here rather than clipped pixel by pixel
static void drawBunny(int x0, int y0) {
    if (x0 < 0) x0 = 0;
    if (x0 > SCREEN_W - BUNNY_W) x0 = SCREEN_W - BUNNY_W;
    if (y0 < 0) y0 = 0;
    if (y0 > SCREEN_H - BUNNY_H) y0 = SCREEN_H - BUNNY_H;
    uint8_t *pixels = FRAME_PIXELS();

    switch (DEPTHS[depth]) {
    case 16: {
        // white and black are the same in either byte order
        uint16_t *row = (uint16_t *)pixels + y0 * SCREEN_W + x0;
        for (int y = 0; y < BUNNY_H; y++, row += SCREEN_W)
            for (int r = BUNNY_ROWS[y]; r < BUNNY_ROWS[y + 1]; r++) {
                uint16_t *p = row + BUNNY_RUNS[r].x;
                const uint16_t c = BUNNY_RUNS[r].white ? WHITE : BLACK;
                for (uint8_t n = BUNNY_RUNS[r].len; n; n--)
                    *p++ = c;
            }
        break;
    }
    case 8: {
        // RGB332: white 0xFF, black 0x00
        uint8_t *row = pixels + y0 * SCREEN_W + x0;
        for (int y = 0; y < BUNNY_H; y++, row += SCREEN_W)
            for (int r = BUNNY_ROWS[y]; r < BUNNY_ROWS[y + 1]; r++)
                memset(row + BUNNY_RUNS[r].x, BUNNY_RUNS[r].white ? 0xFF : 0x00, BUNNY_RUNS[r].len);
        break;
    }
    default: {
        // 1 bpp: a row of the bunny lands in three bytes of the frame. Its 16-bit mask
        // and white pattern are shifted to where the bunny starts within the first
        // byte; the drawn bits are cleared and the white ones set (a set bit is white)
        const int stride = SCREEN_W / 8, shift = 8 - (x0 & 7);
        uint8_t *row = pixels + y0 * stride + (x0 >> 3);
        for (int y = 0; y < BUNNY_H; y++, row += stride) {
            const uint32_t m = (uint32_t)BUNNY_MASK1[y] << shift;
            const uint32_t w = (uint32_t)BUNNY_WHITE1[y] << shift;
            row[0] = (uint8_t)((row[0] & ~(m >> 16)) | (w >> 16));
            row[1] = (uint8_t)((row[1] & ~(m >> 8)) | (w >> 8));
            if (m & 0xFF)       // a bunny on a byte boundary covers two bytes only
                row[2] = (uint8_t)((row[2] & ~m) | w);
        }
        break;
    }
    }
}

static void drawFrame(bool shaking) {
    if (!frameOk)
        return;
    frame.fillSprite(WHITE);
    for (uint16_t i = 0; i < bunnyCount; i++)
        drawBunny((bunnies[i].xLow | ((bunnies[i].bits & 1) << 8)) >> 2, (bunnies[i].yLow | ((bunnies[i].bits & 2) << 7)) >> 2);

    // the box and the text shake while a button adds bunnies, as on the Playdate
    int rx = 0, ry = 0;
    if (shaking) {
        rx = (int)(rnd() % 3) - 1;
        ry = (int)(rnd() % 3) - 1;
    }
    frame.drawRect(BOX_X + rx, BOX_Y + ry, BOX_W, BOX_H, BLACK);
    frame.setTextColor(BLACK);
    frame.setCursor(3 + rx, 3 + ry);
    frame.printf("FPS %u @ %u bpp %s", fps, DEPTHS[depth], LIBRARY_NAME);
    frame.setCursor(3 + rx, 12 + ry);
    frame.printf("bunnies %u", bunnyCount);

    present();
}

// ---------------------------------------------------------------------------

void setup() {
    myESPboy.begin("BunnyMark " LIBRARY_NAME);    // also opens the serial port at 115200
#if LOVYANGFX
    for (int i = 0; i < 256; i++)
        rgb332[i] = (uint16_t)lgfx::color_convert<lgfx::swap565_t, lgfx::rgb332_t>(i);
#endif
    rng = ESP.random() | 1;
    setDepth(0);
}

static uint8_t lastKeys = 0;
// A + B was held at some point since A and B were last both up: their releases then
// end the reset and add nothing
static bool resetHeld = false;

void loop() {
    const uint8_t keys = myESPboy.getKeys();
    const uint8_t released = lastKeys & ~keys;
    lastKeys = keys;
    if ((keys & PAD_ACT) && (keys & PAD_ESC)) {
        bunnyCount = 0;
        resetHeld = true;
    }
    // a button adds its bunnies once, when it is let go
    if (!resetHeld) {
        if (released & PAD_ACT) addBunnies(1);
        if (released & PAD_ESC) addBunnies(100);
    }
    if (!(keys & (PAD_ACT | PAD_ESC)))
        resetHeld = false;
    if (released & PAD_UP) addBunnies(500);
    if (released & PAD_DOWN) addBunnies(1000);
    if (released & PAD_RGT) setDepth((depth + 1) % 3);

#if BUNNYMARK_SERIAL
    // the same from the serial port
    while (Serial.available()) {
        switch (Serial.read()) {
            case 'a': addBunnies(1); break;
            case 'b': addBunnies(100); break;
            case 'u': addBunnies(500); break;
            case 'd': addBunnies(1000); break;
            case 'r': bunnyCount = 0; break;
            case 'm': setDepth((depth + 1) % 3); break;
        }
    }
#endif

    updateBunnies();
    drawFrame(keys & (PAD_ACT | PAD_ESC | PAD_UP | PAD_DOWN));

    fpsFrames++;
    const uint32_t now = millis();
    if (now - fpsStart >= 1000) {
        fps = (uint16_t)(fpsFrames * 1000 / (now - fpsStart));
#if BUNNYMARK_SERIAL
        Serial.printf("fps %u bunnies %u depth %u lib %s heap %u\n", fps, bunnyCount, DEPTHS[depth],
                      LIBRARY_NAME, ESP.getFreeHeap());
#endif
        restartFps();
    }
}
