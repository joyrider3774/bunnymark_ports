// BunnyMark itself for the devices that keep their whole screen in RAM (PyBadge, PyGamer,
// PicoSystem, Explorer 2350, Tufty 2350): the bunnies, the buttons' rules and the status line.
// A port of Timofffee's Playdate bunnymark (github.com/Timofffee/bunnymark-playdate), itself
// raylib's textures_bunnymark by Ramon Santamaria.
//
// The sketch starts the device, reads its buttons into the GAME_ bits below, calls
// bunnyGameFrame every frame and sends the frame to its display. Positions and speeds are
// 12.4 fixed point, as on the CHGame and the ESPboy (the RP2040 and the SAMD51 have no FPU;
// the RP2350's would do, but one code for all is the point). The bunny is 16x16 at half the
// Playdate's speed on every screen, the size every port uses so their frame rates can be
// compared.
//
// The canonical copy is common/BunnyGame.h; tools/make_sprites.py copies it into each sketch.

#pragma once
#include "BunnyFrame.h"

// what the device's buttons are turned into
#define GAME_ADD1    0x01
#define GAME_ADD100  0x02
#define GAME_ADD500  0x04
#define GAME_ADD1000 0x08
#define GAME_DEPTH   0x10     // next frame depth: 16, 8, 1

#ifndef BUNNY_MAX
#define BUNNY_MAX 10000
#endif

struct Bunny
{
    int16_t x, y;     // 12.4
    int8_t vx, vy;    // 12.4 per frame
};

static Bunny bunnyList[BUNNY_MAX];
static int bunnyCount = 0;
static uint32_t bunnyRng = 1;

// the box the bunnies bounce in, below the two status lines (the Playdate's is 5,40 390x195)
static int boxX, boxY, boxW, boxH;
static const int bunnySize = BUNNY16_W;

static inline uint32_t bunnyRandom(void)
{
    bunnyRng = bunnyRng * 1664525u + 1013904223u;
    return bunnyRng >> 8;
}

static void bunnyAdd(int n)
{
    while (n-- > 0 && bunnyCount < BUNNY_MAX)
    {
        Bunny &b = bunnyList[bunnyCount++];
        b.x = (int16_t)((boxX + (boxW - bunnySize) / 2) << 4);
        b.y = (int16_t)((boxY + (boxH - bunnySize) / 2) << 4);
        // the Playdate's (rand() % 500 - 250) / 50.0 pixels, halved as the bunny is, in 12.4
        b.vx = (int8_t)(((int)(bunnyRandom() % 500) - 250) * 16 / 100);
        b.vy = (int8_t)(((int)(bunnyRandom() % 500) - 250) * 16 / 100);
    }
}

static void bunnyMove(void)
{
    const int minX = boxX << 4, maxX = (boxX + boxW - bunnySize) << 4;
    const int minY = boxY << 4, maxY = (boxY + boxH - bunnySize) << 4;
    for (int i = 0; i < bunnyCount; i++)
    {
        Bunny &b = bunnyList[i];
        b.x += b.vx;
        b.y += b.vy;
        if (b.x > maxX || b.x < minX) b.vx = -b.vx;
        if (b.y > maxY || b.y < minY) b.vy = -b.vy;
    }
}

static char *bunnyPutNumber(char *p, uint32_t v)
{
    char tmp[10];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static char *bunnyPutText(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}

static uint32_t bunnyFps = 0, bunnyFpsFrames = 0, bunnyFpsStart = 0;
static uint8_t bunnyLastButtons = 0;
static bool bunnyResetHeld = false;

static void bunnyRestartFps(uint32_t nowMicros)
{
    bunnyFpsFrames = 0;
    bunnyFpsStart = nowMicros;
}

// the frame's size and depth set, the layout worked out from them
static void bunnyGameSetup(BunnyFrame &f, uint32_t seed, uint32_t nowMicros)
{
    bunnyRng = seed | 1;
    boxX = 2;
    boxY = 22;
    boxW = f.width - 4;
    boxH = f.height - boxY - 2;
    bunnyRestartFps(nowMicros);
}

// One frame: the buttons, the bunnies moved, everything drawn into f. The caller sends f to
// the display afterwards
static void bunnyGameFrame(BunnyFrame &f, uint8_t buttons, uint32_t nowMicros)
{
    const uint8_t released = bunnyLastButtons & (uint8_t)~buttons;
    bunnyLastButtons = buttons;

    // A + B (the +1 and +100 buttons) takes the bunnies away; letting go of them afterwards
    // adds nothing
    if ((buttons & GAME_ADD1) && (buttons & GAME_ADD100))
    {
        bunnyCount = 0;
        bunnyResetHeld = true;
    }
    // a button adds its bunnies once, when it is let go
    if (!bunnyResetHeld)
    {
        if (released & GAME_ADD1) bunnyAdd(1);
        if (released & GAME_ADD100) bunnyAdd(100);
    }
    if (!(buttons & (GAME_ADD1 | GAME_ADD100)))
        bunnyResetHeld = false;
    if (released & GAME_ADD500) bunnyAdd(500);
    if (released & GAME_ADD1000) bunnyAdd(1000);
    if (released & GAME_DEPTH)
    {
        // 16 -> 8 -> 1 -> 16; a depth there is no memory for is passed over
        const uint8_t next[3][2] = { { 16, 8 }, { 8, 1 }, { 1, 16 } };
        uint8_t want = f.depth;
        for (int tries = 0; tries < 2; tries++)
        {
            for (int i = 0; i < 3; i++)
                if (next[i][0] == want) { want = next[i][1]; break; }
            if (bunnyFrameSetDepth(f, want))
                break;
        }
        bunnyRestartFps(nowMicros);
    }

    bunnyMove();

    bunnyFrameFill(f, true);
    for (int i = 0; i < bunnyCount; i++)
        bunnyFrameBunny<16>(f, bunnyList[i].x >> 4, bunnyList[i].y >> 4, BUNNY16_ROWS, BUNNY16_RUNS,
                            BUNNY16_MASK1, BUNNY16_WHITE1);

    // the box and the text shake while a button adds bunnies, as on the Playdate
    int rx = 0, ry = 0;
    if (buttons & (GAME_ADD1 | GAME_ADD100 | GAME_ADD500 | GAME_ADD1000))
    {
        rx = (int)(bunnyRandom() % 3) - 1;
        ry = (int)(bunnyRandom() % 3) - 1;
    }
    bunnyFrameRect(f, boxX + rx, boxY + ry, boxW, boxH, false);
    char text[40];
    char *p = bunnyPutText(text, "FPS ");
    p = bunnyPutNumber(p, bunnyFps);
    p = bunnyPutText(p, " @ ");
    p = bunnyPutNumber(p, f.depth);
    bunnyPutText(p, " bpp");
    bunnyFrameText(f, 3 + rx, 3 + ry, text, false);
    p = bunnyPutText(text, "bunnies ");
    bunnyPutNumber(p, (uint32_t)bunnyCount);
    bunnyFrameText(f, 3 + rx, 12 + ry, text, false);

    bunnyFpsFrames++;
    if (nowMicros - bunnyFpsStart >= 1000000u)
    {
        bunnyFps = (uint32_t)((uint64_t)bunnyFpsFrames * 1000000u / (nowMicros - bunnyFpsStart));
        bunnyRestartFps(nowMicros);
    }
}
