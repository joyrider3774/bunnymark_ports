// BunnyMark for the Arduboy: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, UP ten, B a hundred, each once when the button is let
// go; A + B takes them all away again. Every bunny moves and bounces off the box
// each frame, the screen shows the frame rate and the count. See README.md.
//
// What differs from the Playdate version, and why:
// - 128x64, one bit per pixel: the bunny is 16x16, the size every port uses so their frame
//   rates can be compared (tools/make_sprites.py), and moves at half the Playdate's speed,
//   as it is half the size.
// - The display is inverted in the panel (arduboy.invert), so the frame is drawn as
//   usual and shows black on white like the Playdate's, at no cost.
// - 2.5 KB of RAM, 1 KB of it the screen buffer: MAX_BUNNIES instead of 50000. To fit as
//   many as it can, a bunny is 3 bytes: its position in quarter pixels (x 9 bits, y 8 bits)
//   and its speed on each axis as a 3-bit index into SPEEDS, a half to two and a half
//   pixels a frame either way (integers, the ATmega32U4 has no FPU). And the USB code is left out
//   (ARDUBOY_NO_USB), which frees its RAM: to upload, hold DOWN while switching on.
// - Drawing is Arduboy2's: clear, Sprites::drawPlusMask per bunny, then the buffer to
//   the display, as fast as it goes (no frame rate cap, as on the Playdate).
//
// BUNNYMARK_TEST=N builds a test version for Ardens' headless mode, which has no
// buttons and no screenshots: it starts with N bunnies, prints the frame rate every
// second and, at BUNNYMARK_DUMP_MS, the screen buffer in hex. The text goes straight
// into the USB data register (UEDATX), which is what Ardens captures as serial output
// (as its own tests do); Arduino's Serial only sends once a host has opened the port.

#include <Arduboy2.h>
#include "bunny.h"

Arduboy2 arduboy;

#ifdef BUNNYMARK_TEST
struct TestOut : Print {
    size_t write(uint8_t c) override { UEDATX = c; return 1; }
};
static TestOut testOut;
#endif

// what fits with about 300 bytes left for the stack
#ifndef MAX_BUNNIES
#define MAX_BUNNIES 390
#endif

// the box the bunnies bounce in, below the status line (the Playdate's is 5,40 390x195), in
// quarter pixels
#define BOX_X 0
#define BOX_Y 9
#define BOX_W 128
#define BOX_H 55
#define BUNNY_SIZE 16
#define MIN_X ((BOX_X + 1) * 4)
#define MAX_X ((BOX_X + BOX_W - 1 - BUNNY_SIZE) * 4)
#define MIN_Y ((BOX_Y + 1) * 4)
#define MAX_Y ((BOX_Y + BOX_H - 1 - BUNNY_SIZE) * 4)

// quarter pixels a frame, up to 2.5 px as on the CHGame; SPEEDS[7 - i] is -SPEEDS[i], so
// turning round is 7 - i
static const int8_t SPEEDS[8] = { -10, -7, -4, -2, 2, 4, 7, 10 };

// 3 bytes: x's low 8 bits, y, then x's 9th bit (bit 0), the x speed's index (bits 1-3) and
// the y speed's (bits 4-6)
struct Bunny {
    uint8_t xLow;
    uint8_t y;
    uint8_t bits;
};

static Bunny bunnies[MAX_BUNNIES];
static uint16_t bunnyCount = 0;

static uint16_t rng = 1;
static inline uint16_t rnd() {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

// One bunny unpacked, moved a frame and packed again. It turns round where it passes the box's
// edge and is put back on it: in 9 and 8 bits a bunny past the edge would wrap round
static inline void moveBunny(Bunny &b) {
    int16_t x = b.xLow | ((int16_t)(b.bits & 1) << 8);
    int16_t y = b.y;
    uint8_t vx = (b.bits >> 1) & 7, vy = (b.bits >> 4) & 7;
    x += SPEEDS[vx];
    y += SPEEDS[vy];
    if (x > MAX_X) { x = MAX_X; vx = 7 - vx; }
    else if (x < MIN_X) { x = MIN_X; vx = 7 - vx; }
    if (y > MAX_Y) { y = MAX_Y; vy = 7 - vy; }
    else if (y < MIN_Y) { y = MIN_Y; vy = 7 - vy; }
    b.xLow = (uint8_t)x;
    b.y = (uint8_t)y;
    b.bits = (uint8_t)((x >> 8) | (vx << 1) | (vy << 4));
}

static void addBunnies(uint16_t n) {
    const uint16_t x = (BOX_X + (BOX_W - BUNNY_SIZE) / 2) * 4;
    const uint8_t y = (BOX_Y + (BOX_H - BUNNY_SIZE) / 2) * 4;
    while (n-- > 0 && bunnyCount < MAX_BUNNIES) {
        Bunny &b = bunnies[bunnyCount++];
        b.xLow = (uint8_t)x;
        b.y = y;
        // a random speed on each axis, the Playdate's spread of them over the table
        const uint8_t vx = rnd() & 7, vy = rnd() & 7;
        b.bits = (uint8_t)((x >> 8) | (vx << 1) | (vy << 4));
        // With 8 speeds an axis there are only 64 paths, and bunnies on the same one would lie
        // exactly on top of each other: each starts a random 0-63 frames along its own path
        for (uint8_t steps = rnd() & 63; steps; steps--)
            moveBunny(b);
    }
}

// Every bunny moved a frame (the same as moveBunny, written out here: a call per bunny costs
// the 8-bit CPU more than the move itself)
static void updateBunnies() {
    for (uint16_t i = 0; i < bunnyCount; i++) {
        Bunny &b = bunnies[i];
        int16_t x = b.xLow | ((int16_t)(b.bits & 1) << 8);
        int16_t y = b.y;
        uint8_t vx = (b.bits >> 1) & 7, vy = (b.bits >> 4) & 7;
        x += SPEEDS[vx];
        y += SPEEDS[vy];
        if (x > MAX_X) { x = MAX_X; vx = 7 - vx; }
        else if (x < MIN_X) { x = MIN_X; vx = 7 - vx; }
        if (y > MAX_Y) { y = MAX_Y; vy = 7 - vy; }
        else if (y < MIN_Y) { y = MIN_Y; vy = 7 - vy; }
        b.xLow = (uint8_t)x;
        b.y = (uint8_t)y;
        b.bits = (uint8_t)((x >> 8) | (vx << 1) | (vy << 4));
    }
}

static uint16_t fps = 0, fpsFrames = 0;
static uint32_t fpsStart = 0;

static void drawFrame(bool shaking) {
    arduboy.clear();
    for (uint16_t i = 0; i < bunnyCount; i++) {
        const Bunny &b = bunnies[i];
        Sprites::drawPlusMask((b.xLow | ((int16_t)(b.bits & 1) << 8)) >> 2, b.y >> 2, BUNNY, 0);
    }

    // the box and the text shake while a button adds bunnies, as on the Playdate
    int8_t rx = 0, ry = 0;
    if (shaking) {
        rx = (int8_t)(rnd() % 3) - 1;
        ry = (int8_t)(rnd() % 3) - 1;
    }
    // drawn in "white", which the inverted display shows black
    arduboy.drawRect(BOX_X + rx, BOX_Y + ry, BOX_W, BOX_H, WHITE);
    arduboy.setCursor(1 + rx, 0 + ry);
    arduboy.print(F("FPS "));
    arduboy.print(fps);
    arduboy.setCursor(56 + rx, 0 + ry);
    arduboy.print(F("bunnies "));
    arduboy.print(bunnyCount);

    arduboy.display();
}

#ifndef BUNNYMARK_TEST
// No USB: its RAM goes to bunnies. Holding DOWN while switching on goes to the bootloader for
// an upload. (Not in the test build: Ardens' headless run reads every button as held, DOWN
// included, and the test's output goes out through the USB data register.) It defines main(),
// so it stays below the types: the Arduino build puts its function prototypes above the first
// function in the sketch
ARDUBOY_NO_USB
#endif

void setup() {
#ifdef BUNNYMARK_TEST
    // the hardware only: begin()'s logo and start-up button checks (flashlight,
    // system buttons) wait on buttons that a headless run never lets go of
    arduboy.boot();
#else
    arduboy.begin();
#endif
    arduboy.invert(true);
    rng = (uint16_t)arduboy.generateRandomSeed() | 1;
#ifdef BUNNYMARK_TEST
    addBunnies(BUNNYMARK_TEST);
#endif
    fpsStart = millis();
}

// A + B was held at some point since A and B were last both up: their releases
// then end the reset and add nothing
static bool resetHeld = false;

void loop() {
#ifndef BUNNYMARK_TEST
    // (left out of the test build: a headless run reads every button as held, and
    // A + B would reset the count every frame)
    arduboy.pollButtons();
    if (arduboy.pressed(A_BUTTON | B_BUTTON)) {
        bunnyCount = 0;
        resetHeld = true;
    }
    // a button adds its bunnies once, when it is let go
    if (!resetHeld) {
        if (arduboy.justReleased(A_BUTTON)) addBunnies(1);
        if (arduboy.justReleased(B_BUTTON)) addBunnies(100);
    }
    if (arduboy.justReleased(UP_BUTTON)) addBunnies(10);
    if (arduboy.notPressed(A_BUTTON | B_BUTTON))
        resetHeld = false;
    const bool shaking = arduboy.anyPressed(A_BUTTON | B_BUTTON | UP_BUTTON);
#else
    const bool shaking = false;
#endif

    updateBunnies();
    drawFrame(shaking);

    fpsFrames++;
    const uint32_t now = millis();
    if (now - fpsStart >= 1000) {
        fps = (uint16_t)((uint32_t)fpsFrames * 1000 / (now - fpsStart));
        fpsFrames = 0;
        fpsStart = now;
#ifdef BUNNYMARK_TEST
        testOut.print(F("fps "));
        testOut.print(fps);
        testOut.print(F(" bunnies "));
        testOut.println(bunnyCount);
#endif
    }

#ifdef BUNNYMARK_TEST
    // the screen buffer once, in hex: 8 pages of 128 columns, bit 0 the top row
    static bool dumped = false;
    if (!dumped && now >= BUNNYMARK_DUMP_MS) {
        dumped = true;
        testOut.println(F("SCREEN"));
        const uint8_t *buf = arduboy.getBuffer();
        for (uint16_t i = 0; i < 1024; i++) {
            if (buf[i] < 16) testOut.print('0');
            testOut.print(buf[i], HEX);
            if ((i & 63) == 63) testOut.println();
        }
        testOut.println(F("END"));
    }
#endif
}
