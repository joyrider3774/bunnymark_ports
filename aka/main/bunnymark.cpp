// BunnyMark for the Gamebuino AKA: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, C five hundred, D a thousand, each once
// when the button is let go; A + B takes them all away again. Every bunny moves
// and bounces each frame; the frame rate and the count are shown on top.
// L1 and R1 step the panel's refresh rate down and up (see REFRESH_RATES).
// MENU + RUN held for half a second goes back to the launcher (Jicehel's
// convention); RUN alone switches the AKA off.
//
// The logic is the Playdate's, floats and all (the ESP32-S3 has an FPU), on a
// 320x240 screen instead of 400x240: the box is 80 pixels narrower, and the bunnies
// bounce at its edges. The bunny is the image at half size, 16x16, the size
// every port uses so their frame rates can be compared, and moves at half the speed;
// it is drawn as runs of black and white straight into the library's frame buffer
// (bunny.h, from tools/make_sprites.py). The 50000 bunnies the Playdate allows live in PSRAM.

#include "gb_core.h"
#include "gb_graphics.h"
#include "gb_ll_i2c.h"
#include "gb_ll_expander.h"
#include "bunny.h"

#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <stdlib.h>

#define MAX_BUNNIES 50000
#define SCREEN_W 320
#define SCREEN_H 240

gb_core core;
gb_graphics graphics;

typedef struct Vector2 { float x; float y; } Vector2;
typedef struct Bunny { Vector2 position; Vector2 speed; } Bunny;

static Bunny *bunnies = NULL;
static int bunniesCount = 0;
static bool pressed = false;

static const uint16_t WHITE = 0xFFFF, BLACK = 0x0000;

static int fps = 0, fpsFrames = 0;
static uint32_t fpsStart = 0;

// The panel's refresh rate, the one display setting the library lets a game change
// while it runs (the colour mode is fixed at RGB565 when it starts the panel). The
// library waits for the panel's tearing signal before each frame, so this is also
// the ceiling on the frame rate: once drawing takes longer than one refresh, a
// frame takes two. The library turns the rate into the panel's FRCTRL2 divider,
// which has coarse steps, so the panel's own rate can differ a little.
static const uint8_t REFRESH_RATES[] = { 40, 60, 80, 100 };
#define REFRESH_COUNT ((int)(sizeof(REFRESH_RATES) / sizeof(REFRESH_RATES[0])))
static int refresh = REFRESH_COUNT - 1;

static void setRefresh(int index)
{
    if (index < 0 || index >= REFRESH_COUNT)
        return;
    refresh = index;
    graphics.set_refresh_rate(REFRESH_RATES[refresh]);
    // the frame rate is counted again from here
    fpsFrames = 0;
    fpsStart = core.get_millis();
}

static void instantiateBunnies(int count)
{
    for (int i = 0; i < count; i++)
    {
        if (bunniesCount < MAX_BUNNIES)
        {
            bunnies[bunniesCount].position.x = 160.0f;
            bunnies[bunniesCount].position.y = 120.0f;
            // the Playdate's speeds halved, as the bunny is
            bunnies[bunniesCount].speed.x = (float)(rand() % 500 - 250) / 100.0f;
            bunnies[bunniesCount].speed.y = (float)(rand() % 500 - 250) / 100.0f;
            bunniesCount++;
        }
    }
}

// RUN is down, and whether it has been on its own all the while (no MENU)
static bool runDown = false, runAlone = false;

// The library's poll (core.pool) switches the AKA off as soon as it sees RUN, so
// MENU + RUN would never get here. The keys are read from the expander instead, the
// read the poll makes itself, and while RUN is down the poll is left out (the
// joystick is read on its own). RUN on its own still switches off when it comes
// up, as the library does; RUN that MENU joined does not.
static uint16_t readKeys(void)
{
    const uint16_t raw = gb_ll_expander_read() & EXPANDER_KEY;
    uint16_t keys;
    if (raw & GB_KEY_RUN)
    {
        core.joystick.update();
        keys = raw;
    }
    else
    {
        core.pool();
        keys = core.buttons.state();
    }
    if (keys & GB_KEY_RUN)
    {
        if (!runDown)
            runDown = runAlone = true;
        if (keys & GB_KEY_MENU)
            runAlone = false;
    }
    else if (runDown)
    {
        runDown = false;
        if (runAlone)
            gb_ll_expander_power_off();
    }
    return keys;
}

// Back to the launcher: it lives in the second app slot (ota_1, "loader"), so that
// slot becomes the one to boot and the AKA restarts into it
static void exitToLauncher(void)
{
    const esp_partition_t *loader = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                             ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (!loader)
        return;
    esp_ota_set_boot_partition(loader);
    esp_restart();
}

// MENU + RUN held this long calls exitToLauncher (Jicehel's timing, check_return_to_loader)
#define EXIT_HOLD_MS 500
static int64_t exitHeldSince = 0;     // when both went down, 0 while they are not

static uint16_t lastKeys = 0;
// A + B was held at some point since A and B were last both up: their releases
// then end the reset and add nothing
static bool resetHeld = false;

static void checkButtons(void)
{
    const uint16_t keys = readKeys();

    if ((keys & GB_KEY_MENU) && (keys & GB_KEY_RUN))
    {
        const int64_t now = esp_timer_get_time() / 1000;
        if (!exitHeldSince)
            exitHeldSince = now | 1;
        else if (now - exitHeldSince >= EXIT_HOLD_MS)
            exitToLauncher();
    }
    else
        exitHeldSince = 0;

    // a button adds its bunnies once, when it is let go
    const uint16_t released = lastKeys & ~keys;
    lastKeys = keys;
    if ((keys & GB_KEY_A) && (keys & GB_KEY_B))
    {
        bunniesCount = 0;
        resetHeld = true;
    }
    if (!resetHeld)
    {
        if (released & GB_KEY_A)
            instantiateBunnies(1);
        if (released & GB_KEY_B)
            instantiateBunnies(100);
    }
    if (!(keys & (GB_KEY_A | GB_KEY_B)))
        resetHeld = false;
    if (released & GB_KEY_C)
        instantiateBunnies(500);
    if (released & GB_KEY_D)
        instantiateBunnies(1000);
    if (released & GB_KEY_L1)
        setRefresh(refresh - 1);
    if (released & GB_KEY_R1)
        setRefresh(refresh + 1);
    // the box shakes while a button is held, as on the Playdate
    pressed = keys & (GB_KEY_A | GB_KEY_B | GB_KEY_C | GB_KEY_D);
}

// the box the bunnies bounce in, the Playdate's made 80 pixels narrower
#define BOX_X 5
#define BOX_Y 40
#define BOX_W (SCREEN_W - 10)
#define BOX_H 195

// A bunny turns round where it reaches the box's edge (the Playdate's own bounds were set for its
// 32x32 bunny and left a gap with the 16x16 one)
static void updateBunnies(void)
{
    for (int i = 0; i < bunniesCount; i++)
    {
        bunnies[i].position.x += bunnies[i].speed.x;
        bunnies[i].position.y += bunnies[i].speed.y;

        if ((bunnies[i].position.x > BOX_X + BOX_W - BUNNY_W) ||
            (bunnies[i].position.x < BOX_X)) bunnies[i].speed.x *= -1;
        if ((bunnies[i].position.y > BOX_Y + BOX_H - BUNNY_H) ||
            (bunnies[i].position.y < BOX_Y)) bunnies[i].speed.y *= -1;
    }
}

// the bunny with its top left at (x0, y0), clipped to the screen
static void drawBunny(int x0, int y0)
{
    for (int y = 0; y < BUNNY_H; y++)
    {
        const int sy = y0 + y;
        if (sy < 0 || sy >= SCREEN_H)
            continue;
        gb_pixel *row = &framebuffer[sy * SCREEN_W];
        for (int r = BUNNY_ROWS[y]; r < BUNNY_ROWS[y + 1]; r++)
        {
            int x = x0 + BUNNY_RUNS[r].x, end = x + BUNNY_RUNS[r].len;
            if (x < 0) x = 0;
            if (end > SCREEN_W) end = SCREEN_W;
            const gb_pixel c = BUNNY_RUNS[r].white ? WHITE : BLACK;
            for (; x < end; x++)
                row[x] = c;
        }
    }
}

static void drawBunnies(void)
{
    graphics.clear(WHITE);

    for (int i = 0; i < bunniesCount; i++)
        drawBunny((int)bunnies[i].position.x, (int)bunnies[i].position.y);

    int rX = 0, rY = 0;
    if (pressed)
    {
        rX = rand() % 4 - 2;
        rY = rand() % 4 - 2;
    }
    graphics.setColor(BLACK);
    graphics.drawRect(BOX_X + rX, BOX_Y + rY, BOX_W, BOX_H);
    graphics.move_cursor(10 + rX, 10 + rY);
    graphics.printf("FPS %d @ %d Hz", fps, REFRESH_RATES[refresh]);
    graphics.move_cursor(170 + rX, 10 + rY);
    graphics.printf("bunnies: %d", bunniesCount);
}

extern "C" void app_main(void)
{
    core.init();
    setRefresh(refresh);
    srand(esp_random());

    bunnies = (Bunny *)heap_caps_malloc(sizeof(Bunny) * MAX_BUNNIES, MALLOC_CAP_SPIRAM);
    if (!bunnies)
    {
        printf("bunnymark: no PSRAM for %d bunnies\n", MAX_BUNNIES);
        return;
    }
    fpsStart = core.get_millis();

    for (;;)
    {
        checkButtons();
        updateBunnies();
        drawBunnies();
        graphics.update();

        fpsFrames++;
        const uint32_t now = core.get_millis();
        if (now - fpsStart >= 1000)
        {
            fps = (int)(fpsFrames * 1000 / (now - fpsStart));
            fpsFrames = 0;
            fpsStart = now;
        }
    }
}
