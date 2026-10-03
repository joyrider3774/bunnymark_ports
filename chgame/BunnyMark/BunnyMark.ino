// BunnyMark for the CHGame: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, SELECT five hundred, each once when the
// button is let go; A + B takes them all away again. START switches the panel
// between 16, 12 and 18 bpp (see nextMode). Every
// bunny moves and bounces off the box each frame, the screen shows the frame rate
// and the count. Holding START for 3 s goes back to the SD game menu, as in the
// other CHGame games.
//
// What differs from the Playdate version, and why:
// - 128x128 instead of 400x240: the bunny is 16x16 (half size, tools/make_sprites.py)
//   and its speed is halved with it, so the motion looks the same.
// - 20 KB of RAM, of which CHGfx's 4 bpp frame buffer takes 8 KB: MAX_BUNNIES
//   instead of 50000. To fit as many as it can, a bunny is 3 bytes: its position in quarter
//   pixels (x and y 9 bits each) and its speed on each axis as a 3-bit index into SPEEDS, a
//   half to two and a half pixels a frame either way (integers, the CH32X035 has no FPU).
// - Drawing is CHGfx: clear, a span sprite per bunny, then the whole frame to the
//   panel (gfx_flush), as fast as it goes (the Playdate version runs unthrottled too).

#include <CHGame.h>
#include "bunny.h"

// what fits below the 2 KB stack, about 100 bytes to spare (built with USB serial left out)
#ifndef MAX_BUNNIES
#define MAX_BUNNIES 1800
#endif

enum : uint8_t { BM_BLACK, BM_WHITE };   // palette slots (CHGame.h has its own BLACK/WHITE names)
static const uint16_t PALETTE[2] = { 0x0000, 0xFFFF };

// the box the bunnies bounce in, below the status line (the Playdate's is 5,40 390x195)
#define BOX_X 2
#define BOX_Y 22
#define BOX_W 124
#define BOX_H 104
#define BUNNY_W 16
#define BUNNY_H 16
// in quarter pixels
#define MIN_X (BOX_X * 4)
#define MAX_X ((BOX_X + BOX_W - BUNNY_W) * 4)
#define MIN_Y (BOX_Y * 4)
#define MAX_Y ((BOX_Y + BOX_H - BUNNY_H) * 4)

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

// unsigned to text, without printf
static char *putNumber(char *p, uint32_t v) {
    char tmp[10];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static uint32_t fps = 0, fpsFrames = 0, fpsStart = 0;

// How the frame goes to the panel. The frame buffer is CHGfx's 4 bpp one whatever
// the mode, so drawing costs the same; what changes is the bytes per frame on the
// SPI wire: 12 bpp 24 KB (8.4 ms, 119 fps at most), 16 bpp 32 KB (11.1 ms, 90 fps),
// 18 bpp 48 KB (16.4 ms, 61 fps). Two colours look the same in all three.
static const uint8_t MODES[3] = { GFX_16BPP, GFX_12BPP, GFX_18BPP };
static const char *const MODE_NAMES[3] = { " @ 16 bpp", " @ 12 bpp", " @ 18 bpp" };
static uint8_t mode = 0;

// START: the next mode. The flush is blocking, so no frame is on its way to the
// panel while it changes; the frame rate is counted again from here
static void nextMode() {
    mode = (uint8_t)((mode + 1) % 3);
    gfx_setColorMode(MODES[mode]);
    fpsFrames = 0;
    fpsStart = millis();
}

static void drawFrame(bool shaking) {
    gfx_clear(BM_WHITE);
    for (uint16_t i = 0; i < bunnyCount; i++) {
        const Bunny &b = bunnies[i];
        gfx_sprite4(BUNNY, (b.xLow | ((b.bits & 1) << 8)) >> 2, (b.yLow | ((b.bits & 2) << 7)) >> 2);
    }

    // the box and the text shake while a button adds bunnies, as on the Playdate
    int rx = 0, ry = 0;
    if (shaking) {
        rx = (int)(rnd() % 3) - 1;
        ry = (int)(rnd() % 3) - 1;
    }
    gfx_rect(BOX_X + rx, BOX_Y + ry, BOX_W, BOX_H, BM_BLACK);
    // "FPS 51 @ 16 bpp" on the first line, the count on the second
    char text[24] = "FPS ";
    char *p = putNumber(text + 4, fps);
    for (const char *s = MODE_NAMES[mode]; *s; ) *p++ = *s++;
    *p = 0;
    gfx_text(3 + rx, 3 + ry, text, BM_BLACK);
    p = text;
    for (const char *s = "bunnies "; *s; ) *p++ = *s++;
    putNumber(p, bunnyCount);
    gfx_text(3 + rx, 12 + ry, text, BM_BLACK);

    gfx_flush();
}

void setup() {
    chgame.boot();
    // The library's own START exit counts 180 polls, 3 s at 60 fps; this runs
    // unthrottled at anything from 14 to 90 fps, so START is timed below instead
    chgame.startExits = false;
    gfx_begin(GFX_DIV2, MODES[mode]);
    gfx_setPalette(PALETTE, 2);
    rng = micros() | 1;
    fpsStart = millis();
}

// START held this long goes back to the SD game menu (a plain reset, which the
// menu bootloader answers with its menu)
#define EXIT_HOLD_MS 3000
static uint32_t startHeldSince = 0;     // when START went down, 0 while it is up

// A + B was held at some point since A and B were last both up: their releases
// then end the reset and add nothing
static bool resetHeld = false;

void loop() {
    chgame.pollButtons();

    if (chgame.pressed(START_BUTTON)) {
        if (!startHeldSince)
            startHeldSince = millis() | 1;
        else if (millis() - startHeldSince >= EXIT_HOLD_MS)
            chgame.exitToMenu();
    } else {
        startHeldSince = 0;
    }

    if (chgame.pressed(A_BUTTON | B_BUTTON)) {
        bunnyCount = 0;
        resetHeld = true;
    }
    // a button adds its bunnies once, when it is let go; one that was already down
    // at power-up (START still held from the exit, under a bootloader without the
    // menu, which starts this again) adds nothing
    static uint8_t heldAtBoot = chgame.buttons();
    uint8_t released = 0;
    for (uint8_t bit = 1; bit; bit <<= 1)
        if (chgame.justReleased(bit)) released |= bit;
    const uint8_t counts = released & ~heldAtBoot;
    heldAtBoot &= chgame.buttons();
    if (!resetHeld) {
        if (counts & A_BUTTON) addBunnies(1);
        if (counts & B_BUTTON) addBunnies(100);
    }
    if (counts & SELECT_BUTTON) addBunnies(500);
    if (counts & START_BUTTON) nextMode();
    if (!chgame.anyPressed(A_BUTTON | B_BUTTON))
        resetHeld = false;

    updateBunnies();
    // the box shakes while a button is held, as on the Playdate
    drawFrame(chgame.anyPressed(A_BUTTON | B_BUTTON | SELECT_BUTTON));

    fpsFrames++;
    const uint32_t now = millis();
    if (now - fpsStart >= 1000) {
        fps = fpsFrames * 1000 / (now - fpsStart);
        fpsFrames = 0;
        fpsStart = now;
    }
}
