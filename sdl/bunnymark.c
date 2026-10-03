// BunnyMark for SDL2 and SDL3: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// The Playdate's own setup: a 400x240 screen (the window scales it), its box and its movement
// code with floats; the bunnies bounce at the box's edges. The bunny is the image at half size, 16x16, the
// size every port uses so their frame rates can be compared, at half the speed. Bunnies are
// added once per press, when the button is let go; see README.md for the keys.
//
// Two display settings can be switched while it runs, like the panel mode on the
// CHGame and the refresh rate on the AKA:
// - the render mode: GPU draws every bunny as a copy of one texture through the SDL
//   renderer (what raylib's original does); CPU fills every bunny into a 400x240
//   frame buffer in memory, the way the AKA port does it, and uploads that once a frame
// - vsync on or off (off by default: the Playdate version runs unthrottled too)
//
// One source for both: BUNNYMARK_SDL3 picks SDL3, otherwise SDL2. The few calls that
// differ are wrapped at the top. Built with Emscripten it runs in a browser: the page calls
// each frame (emscripten_set_main_loop), and vsync picks how it does, on the display's
// refresh or as soon as it can.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BUNNYMARK_SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define SDL_NAME "SDL3"
typedef SDL_FRect BmRect;
typedef SDL_Gamepad BmPad;
typedef const bool *BmKeys;
#else
#define SDL_MAIN_HANDLED
#include <SDL.h>
#define SDL_NAME "SDL2"
typedef SDL_Rect BmRect;
typedef SDL_GameController BmPad;
typedef const Uint8 *BmKeys;
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "bunny.h"
#include "font8x8_basic.h"

#define SCREEN_W 400
#define SCREEN_H 240
#define WINDOW_SCALE 2
#define MAX_BUNNIES 1000000     // the Playdate's is 50000; a PC goes much further

// ---------------------------------------------------------------------------
// SDL2 / SDL3
// ---------------------------------------------------------------------------

static BmRect bm_rect(int x, int y, int w, int h)
{
    BmRect r;
    r.x = x; r.y = y; r.w = w; r.h = h;
    return r;
}

static void bm_copy(SDL_Renderer *r, SDL_Texture *t, const BmRect *src, const BmRect *dst)
{
#ifdef BUNNYMARK_SDL3
    SDL_RenderTexture(r, t, src, dst);
#else
    SDL_RenderCopy(r, t, src, dst);
#endif
}

static void bm_outline(SDL_Renderer *r, int x, int y, int w, int h)
{
    BmRect rect = bm_rect(x, y, w, h);
#ifdef BUNNYMARK_SDL3
    SDL_RenderRect(r, &rect);
#else
    SDL_RenderDrawRect(r, &rect);
#endif
}

#ifdef __EMSCRIPTEN__
// set once the browser drives the frames, see main
static bool mainLoopSet = false;
#endif

static void bm_set_vsync(SDL_Renderer *r, bool on)
{
#ifdef __EMSCRIPTEN__
    // In a browser the page decides when a frame runs: on the display's refresh
    // (requestAnimationFrame, which is vsync) or as soon as it can (setTimeout 0)
    (void)r;
    if (mainLoopSet)
        emscripten_set_main_loop_timing(on ? EM_TIMING_RAF : EM_TIMING_SETTIMEOUT, on ? 1 : 0);
#elif defined(BUNNYMARK_SDL3)
    SDL_SetRenderVSync(r, on ? 1 : 0);
#else
    SDL_RenderSetVSync(r, on ? 1 : 0);
#endif
}

static void bm_nearest(SDL_Texture *t)
{
#ifdef BUNNYMARK_SDL3
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
#else
    SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
#endif
}

static bool bm_create(SDL_Window **win, SDL_Renderer **ren)
{
#ifdef BUNNYMARK_SDL3
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return false;
    *win = SDL_CreateWindow("BunnyMark (" SDL_NAME ")", SCREEN_W * WINDOW_SCALE, SCREEN_H * WINDOW_SCALE,
                            SDL_WINDOW_RESIZABLE);
    if (!*win)
        return false;
    *ren = SDL_CreateRenderer(*win, NULL);
    if (!*ren)
        return false;
    SDL_SetRenderLogicalPresentation(*ren, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
#else
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        return false;
    *win = SDL_CreateWindow("BunnyMark (" SDL_NAME ")", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            SCREEN_W * WINDOW_SCALE, SCREEN_H * WINDOW_SCALE, SDL_WINDOW_RESIZABLE);
    if (!*win)
        return false;
    *ren = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED);
    if (!*ren)
        return false;
    SDL_RenderSetLogicalSize(*ren, SCREEN_W, SCREEN_H);
#endif
    return true;
}

static const char *bm_renderer_name(SDL_Renderer *r)
{
#ifdef BUNNYMARK_SDL3
    return SDL_GetRendererName(r);
#else
    SDL_RendererInfo info;
    return SDL_GetRendererInfo(r, &info) == 0 ? info.name : "?";
#endif
}

// the window's current contents as a .bmp
static void bm_screenshot(SDL_Renderer *r, const char *path)
{
#ifdef BUNNYMARK_SDL3
    SDL_Surface *s = SDL_RenderReadPixels(r, NULL);
    if (s)
    {
        SDL_SaveBMP(s, path);
        SDL_DestroySurface(s);
    }
#else
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s)
    {
        SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        SDL_SaveBMP(s, path);
        SDL_FreeSurface(s);
    }
#endif
}

static BmKeys bm_keys(void)
{
    return SDL_GetKeyboardState(NULL);
}

static BmPad *pad = NULL;

static bool bm_pad_button(int which)
{
    if (!pad)
        return false;
#ifdef BUNNYMARK_SDL3
    static const SDL_GamepadButton map[] = { SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START };
    return SDL_GetGamepadButton(pad, map[which]);
#else
    static const SDL_GameControllerButton map[] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START };
    return SDL_GameControllerGetButton(pad, map[which]) != 0;
#endif
}

// false when the window was closed or Escape pressed; opens and closes a gamepad
static bool bm_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
#ifdef BUNNYMARK_SDL3
        if (e.type == SDL_EVENT_QUIT)
            return false;
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
            return false;
        if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad)
            pad = SDL_OpenGamepad(e.gdevice.which);
        if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which)
        {
            SDL_CloseGamepad(pad);
            pad = NULL;
        }
#else
        if (e.type == SDL_QUIT)
            return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            return false;
        if (e.type == SDL_CONTROLLERDEVICEADDED && !pad)
            pad = SDL_GameControllerOpen(e.cdevice.which);
        if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad &&
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == e.cdevice.which)
        {
            SDL_GameControllerClose(pad);
            pad = NULL;
        }
#endif
    }
    return true;
}

// ---------------------------------------------------------------------------
// The bunnies: the Playdate's code
// ---------------------------------------------------------------------------

typedef struct Vector2 { float x; float y; } Vector2;
typedef struct Bunny { Vector2 position; Vector2 speed; } Bunny;

static Bunny *bunnies = NULL;
static int bunniesCount = 0;

static void instantiateBunnies(int count)
{
    for (int i = 0; i < count; i++)
    {
        if (bunniesCount < MAX_BUNNIES)
        {
            bunnies[bunniesCount].position.x = 200.0f;
            bunnies[bunniesCount].position.y = 120.0f;
            // the Playdate's speeds halved, as the bunny is
            bunnies[bunniesCount].speed.x = (float)(rand() % 500 - 250) / 100.0f;
            bunnies[bunniesCount].speed.y = (float)(rand() % 500 - 250) / 100.0f;
            bunniesCount++;
        }
    }
}

// the box the bunnies bounce in, the Playdate's
#define BOX_X 5
#define BOX_Y 40
#define BOX_W 390
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

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static SDL_Renderer *ren;
static SDL_Texture *bunnyTex;      // the bunny, for the GPU mode
static SDL_Texture *frameTex;      // the CPU mode's frame, uploaded every frame
static SDL_Texture *fontTex;       // 128 glyphs of 8x8 side by side
static uint32_t frame[SCREEN_W * SCREEN_H];

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u

// the bunny's pixels from its runs: black, white or transparent
static SDL_Texture *makeBunnyTexture(void)
{
    static uint32_t px[BUNNY_W * BUNNY_H];
    memset(px, 0, sizeof(px));
    for (int y = 0; y < BUNNY_H; y++)
        for (int r = BUNNY_ROWS[y]; r < BUNNY_ROWS[y + 1]; r++)
            for (int x = BUNNY_RUNS[r].x; x < BUNNY_RUNS[r].x + BUNNY_RUNS[r].len; x++)
                px[y * BUNNY_W + x] = BUNNY_RUNS[r].white ? WHITE : BLACK;
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, BUNNY_W, BUNNY_H);
    SDL_UpdateTexture(t, NULL, px, BUNNY_W * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    bm_nearest(t);
    return t;
}

static SDL_Texture *makeFontTexture(void)
{
    static uint32_t px[128 * 8 * 8];
    for (int c = 0; c < 128; c++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                px[y * 128 * 8 + c * 8 + x] = (font8x8_basic[c][y] >> x) & 1 ? WHITE : 0;
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 128 * 8, 8);
    SDL_UpdateTexture(t, NULL, px, 128 * 8 * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(t, 0, 0, 0);    // drawn black
    bm_nearest(t);
    return t;
}

static void drawText(int x, int y, const char *s)
{
    for (; *s; s++, x += 8)
    {
        const int c = (unsigned char)*s & 127;
        BmRect src = bm_rect(c * 8, 0, 8, 8), dst = bm_rect(x, y, 8, 8);
        bm_copy(ren, fontTex, &src, &dst);
    }
}

// the CPU mode: the bunny's runs filled into the frame, clipped to it
static void fillBunny(int x0, int y0)
{
    for (int y = 0; y < BUNNY_H; y++)
    {
        const int sy = y0 + y;
        if (sy < 0 || sy >= SCREEN_H)
            continue;
        uint32_t *row = &frame[sy * SCREEN_W];
        for (int r = BUNNY_ROWS[y]; r < BUNNY_ROWS[y + 1]; r++)
        {
            int x = x0 + BUNNY_RUNS[r].x, end = x + BUNNY_RUNS[r].len;
            if (x < 0) x = 0;
            if (end > SCREEN_W) end = SCREEN_W;
            const uint32_t c = BUNNY_RUNS[r].white ? WHITE : BLACK;
            for (; x < end; x++)
                row[x] = c;
        }
    }
}

enum { MODE_GPU, MODE_CPU };
static int renderMode = MODE_GPU;
static bool vsync = false;
static int fps = 0;

static void drawBunnies(bool pressed)
{
    if (renderMode == MODE_GPU)
    {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        for (int i = 0; i < bunniesCount; i++)
        {
            BmRect dst = bm_rect((int)bunnies[i].position.x, (int)bunnies[i].position.y, BUNNY_W, BUNNY_H);
            bm_copy(ren, bunnyTex, NULL, &dst);
        }
    }
    else
    {
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            frame[i] = WHITE;
        for (int i = 0; i < bunniesCount; i++)
            fillBunny((int)bunnies[i].position.x, (int)bunnies[i].position.y);
        SDL_UpdateTexture(frameTex, NULL, frame, SCREEN_W * 4);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        bm_copy(ren, frameTex, NULL, NULL);
    }

    // the box and the text shake while a button adds bunnies, as on the Playdate
    int rX = 0, rY = 0;
    if (pressed)
    {
        rX = rand() % 4 - 2;
        rY = rand() % 4 - 2;
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    bm_outline(ren, BOX_X + rX, BOX_Y + rY, BOX_W, BOX_H);
    char text[64];
    snprintf(text, sizeof text, "FPS %d @ %s, vsync %s", fps, renderMode == MODE_GPU ? "GPU" : "CPU",
             vsync ? "on" : "off");
    drawText(10 + rX, 10 + rY, text);
    snprintf(text, sizeof text, "bunnies: %d", bunniesCount);
    drawText(10 + rX, 22 + rY, text);
}

// ---------------------------------------------------------------------------
// Buttons: the keyboard and a gamepad, as one set like a handheld's
// ---------------------------------------------------------------------------

enum { BTN_A = 1, BTN_B = 2, BTN_C = 4, BTN_D = 8, BTN_MODE = 16, BTN_VSYNC = 32 };

static unsigned readButtons(void)
{
    BmKeys k = bm_keys();
    unsigned b = 0;
    if (k[SDL_SCANCODE_X] || bm_pad_button(0)) b |= BTN_A;
    if (k[SDL_SCANCODE_Z] || bm_pad_button(1)) b |= BTN_B;
    if (k[SDL_SCANCODE_C] || bm_pad_button(2)) b |= BTN_C;
    if (k[SDL_SCANCODE_V] || bm_pad_button(3)) b |= BTN_D;
    if (k[SDL_SCANCODE_M] || bm_pad_button(4)) b |= BTN_MODE;
    if (k[SDL_SCANCODE_S] || bm_pad_button(5)) b |= BTN_VSYNC;
    return b;
}

static unsigned lastButtons = 0;
// A + B was held at some point since A and B were last both up: their releases then
// end the reset and add nothing
static bool resetHeld = false;
static uint64_t fpsStart = 0;
static int fpsFrames = 0;

static void restartFps(void)
{
    fpsFrames = 0;
    fpsStart = SDL_GetPerformanceCounter();
}

// true while a button that adds bunnies is held (the shake)
static bool checkButtons(void)
{
    const unsigned buttons = readButtons();
    const unsigned released = lastButtons & ~buttons;
    lastButtons = buttons;

    if ((buttons & BTN_A) && (buttons & BTN_B))
    {
        bunniesCount = 0;
        resetHeld = true;
    }
    if (!resetHeld)
    {
        if (released & BTN_A) instantiateBunnies(1);
        if (released & BTN_B) instantiateBunnies(100);
    }
    if (!(buttons & (BTN_A | BTN_B)))
        resetHeld = false;
    if (released & BTN_C) instantiateBunnies(500);
    if (released & BTN_D) instantiateBunnies(1000);
    if (released & BTN_MODE)
    {
        renderMode = renderMode == MODE_GPU ? MODE_CPU : MODE_GPU;
        restartFps();
    }
    if (released & BTN_VSYNC)
    {
        vsync = !vsync;
        bm_set_vsync(ren, vsync);
        restartFps();
    }
    return (buttons & (BTN_A | BTN_B | BTN_C | BTN_D)) != 0;
}

static double runSeconds = 0;
static const char *runShot = NULL;
static uint64_t runStart = 0;

// one frame: input, move, draw, show, count; false once it is time to stop
static bool runFrame(void)
{
    if (!bm_events())
        return false;
    const bool pressed = checkButtons();
    updateBunnies();
    drawBunnies(pressed);

    const uint64_t freq = SDL_GetPerformanceFrequency(), now = SDL_GetPerformanceCounter();
    const bool last = runSeconds > 0 && (double)(now - runStart) / (double)freq >= runSeconds;
    if (last && runShot)
        bm_screenshot(ren, runShot);     // before presenting: the back buffer is this frame
    SDL_RenderPresent(ren);

    fpsFrames++;
    if (now - fpsStart >= freq)
    {
        fps = (int)((uint64_t)fpsFrames * freq / (now - fpsStart));
        if (runSeconds > 0)
        {
            printf("fps %d  bunnies %d  %s  vsync %s\n", fps, bunniesCount,
                   renderMode == MODE_GPU ? "GPU" : "CPU", vsync ? "on" : "off");
            fflush(stdout);
        }
        restartFps();
    }
    return !last;
}

#ifdef __EMSCRIPTEN__
static void webFrame(void)
{
    if (!runFrame())
        emscripten_cancel_main_loop();
}
#endif

// Options, for running it as a benchmark without touching it:
//   --bunnies N      start with N bunnies
//   --cpu            start in the CPU render mode
//   --vsync          start with vsync on
//   --seconds S      quit after S seconds, printing the frame rate every second
//   --shot out.bmp   save the screen when it quits
int main(int argc, char **argv)
{
    int startBunnies = 0;
    double seconds = 0;
    const char *shot = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--bunnies") && i + 1 < argc) startBunnies = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cpu")) renderMode = MODE_CPU;
        else if (!strcmp(argv[i], "--vsync")) vsync = true;
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else
        {
            fprintf(stderr, "usage: %s [--bunnies N] [--cpu] [--vsync] [--seconds S] [--shot out.bmp]\n", argv[0]);
            return 2;
        }
    }

    SDL_Window *win;
    if (!bm_create(&win, &ren))
    {
        fprintf(stderr, "bunnymark: %s\n", SDL_GetError());
        return 1;
    }
    bunnies = (Bunny *)malloc(sizeof(Bunny) * MAX_BUNNIES);
    if (!bunnies)
    {
        fprintf(stderr, "bunnymark: no memory for %d bunnies\n", MAX_BUNNIES);
        return 1;
    }
    srand((unsigned)SDL_GetPerformanceCounter());

    bunnyTex = makeBunnyTexture();
    fontTex = makeFontTexture();
    frameTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    bm_nearest(frameTex);
    bm_set_vsync(ren, vsync);

    char title[128];
    snprintf(title, sizeof title, "BunnyMark (" SDL_NAME ", %s)", bm_renderer_name(ren));
    SDL_SetWindowTitle(win, title);
    if (seconds > 0)
        printf("%s\n", title + strlen("BunnyMark "));

    instantiateBunnies(startBunnies);
    runSeconds = seconds;
    runShot = shot;
    runStart = SDL_GetPerformanceCounter();
    restartFps();
#ifdef __EMSCRIPTEN__
    // the browser calls the frame; it has no loop of its own to stay in
    emscripten_set_main_loop(webFrame, 0, 0);
    mainLoopSet = true;
    bm_set_vsync(ren, vsync);
    return 0;
#else
    while (runFrame())
        ;

    free(bunnies);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
#endif
}
