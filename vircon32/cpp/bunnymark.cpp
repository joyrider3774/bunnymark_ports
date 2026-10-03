// BunnyMark for Vircon32 in C++, for the v32c++ transpiler (github.com/wedge1020/v32cxx), which
// turns it into Vircon32 C for the console's own compiler: the C version (../BunnyMark.c) as
// classes, so the three versions (C, Lua, C++) compare what each language costs on one console.
// A port of Timofffee's Playdate bunnymark, itself raylib's textures_bunnymark by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, X five hundred, Y a thousand, each once when the button is
// let go; A + B takes them all away again. Every bunny moves and bounces off the box each frame;
// the screen shows the frame rate, how much of a frame's CPU time the last frame took, the GPU
// budget it asked for, and the count. See ../README.md.
//
// Each bunny is a Bunny object with move() and draw(), all of them in one std::vector<Bunny>
// (v32c++'s built-in generic: a vector_Bunny class with its storage from malloc). Its room for
// every bunny is reserved at the start, so it never grows and copies. The texture is the C
// version's, assets/bunny.png: the 16x16 bunny at 0,0 and a 4x4 black block at 32,0 that is
// stretched into the box's lines.

#include "video.h"
#include "input.h"
#include "time.h"
#include "string.h"
#include "misc.h"
#include <vector>

#title "[C++] BunnyMark"
#version 1.0
#texture BUNNY_TEXTURE "assets/bunny.png"

#define MAX_BUNNIES 100000
#define CYCLES_PER_FRAME 250000
// the GPU draws at most 9 screens' worth of pixels a frame (640 x 360 x 9)
#define GPU_PIXELS_PER_FRAME 2073600

// the regions of the texture
#define REGION_BUNNY 0
#define REGION_BLACK 1

// the box the bunnies bounce in, as the Playdate's (5,40 390x195 on 400x240), on 640x360
#define BOX_X 5
#define BOX_Y 40
#define BOX_W 630
#define BOX_H 315
#define BUNNY_SIZE 16

// One bunny: where it is and where it is going. The constructor only fills in the fields, which
// is what a std::vector element may have (the vector copies elements in as plain memory)
class Bunny
{
public:
    float x;
    float y;
    float vx;
    float vy;

    Bunny(float startX, float startY, float speedX, float speedY)
    {
        x = startX;
        y = startY;
        vx = speedX;
        vy = speedY;
    }

    // a frame's move; it turns round where it reaches the box's edge
    void move()
    {
        x += vx;
        y += vy;
        if ((x > (BOX_X + BOX_W - BUNNY_SIZE)) || (x < BOX_X))
            vx = -vx;
        if ((y > (BOX_Y + BOX_H - BUNNY_SIZE)) || (y < BOX_Y))
            vy = -vy;
    }

    // drawn with the texture and the bunny's region already selected
    void draw()
    {
        draw_region_at((int)x, (int)y);
    }
};

std::vector<Bunny> bunnies;
bool pressed = false;
// A + B was held at some point since A and B were last both up: their releases then end the reset
// and add nothing
bool resetHeld = false;
int fps = 0;
int cpuPercent = 0;

void instantiateBunnies(int count)
{
    for (int i = 0; i < count; i++)
    {
        if (bunnies.size() < MAX_BUNNIES)
        {
            // the Playdate's speeds halved, as the bunny is
            Bunny b(320.0, 180.0, (float)(rand() % 500 - 250) / 100.0, (float)(rand() % 500 - 250) / 100.0);
            bunnies.push_back(b);
        }
    }
}

void updateBunnies()
{
    int count = bunnies.size();
    for (int i = 0; i < count; i++)
        bunnies[i].move();
}

// a line of the box: the 4x4 black block stretched
void drawBar(int x, int y, int w, int h)
{
    select_region(REGION_BLACK);
    set_drawing_scale((float)w / 4.0, (float)h / 4.0);
    draw_region_zoomed_at(x, y);
}

void drawRect(int x, int y, int w, int h)
{
    drawBar(x, y, w, 1);
    drawBar(x, y + h - 1, w, 1);
    drawBar(x, y, 1, h);
    drawBar(x + w - 1, y, 1, h);
}

void drawBunnies()
{
    clear_screen(color_white);

    int rX = 0;
    int rY = 0;
    if (pressed)
    {
        rX = rand() % 4 - 2;
        rY = rand() % 4 - 2;
    }
    drawRect(BOX_X + rX, BOX_Y + rY, BOX_W, BOX_H);

    // the BIOS font, drawn black
    int text[64];
    int number[16];
    strcpy(text, "FPS ");
    itoa(fps, number, 10);
    strcat(text, number);
    strcat(text, "  CPU ");
    itoa(cpuPercent, number, 10);
    strcat(text, number);
    strcat(text, "%  GPU ");
    // the clear and the bunnies; the box and the text are a few thousand pixels more
    itoa((640 * 360 + bunnies.size() * 16 * 16) / (GPU_PIXELS_PER_FRAME / 100), number, 10);
    strcat(text, number);
    strcat(text, "%");
    set_multiply_color(color_black);
    print_at(10 + rX, 10 + rY, text);
    strcpy(text, "bunnies: ");
    itoa(bunnies.size(), number, 10);
    strcat(text, number);
    print_at(400 + rX, 10 + rY, text);
    set_multiply_color(color_white);

    select_texture(BUNNY_TEXTURE);
    select_region(REGION_BUNNY);
    int count = bunnies.size();
    for (int i = 0; i < count; i++)
        bunnies[i].draw();
}

// A gamepad button reads above 0 while it is held (the frames it has been) and below 0 once it is
// up (-1 on the first frame after it is let go)
void checkButtons()
{
    int a = gamepad_button_a();
    int b = gamepad_button_b();
    int x = gamepad_button_x();
    int y = gamepad_button_y();

    if ((a > 0) && (b > 0))
    {
        bunnies.clear();
        resetHeld = true;
    }
    // a button adds its bunnies once, when it is let go
    if (!resetHeld)
    {
        if (a == -1)
            instantiateBunnies(1);
        if (b == -1)
            instantiateBunnies(100);
    }
    if ((a <= 0) && (b <= 0))
        resetHeld = false;
    if (x == -1)
        instantiateBunnies(500);
    if (y == -1)
        instantiateBunnies(1000);

    // the box shakes while a button is held, as on the Playdate
    pressed = (a > 0) || (b > 0) || (x > 0) || (y > 0);
}

int main()
{
    select_gamepad(0);
    srand(get_time());
    // room for every bunny up front: a vector that grows copies all its elements each time
    bunnies.reserve(MAX_BUNNIES);

    select_texture(BUNNY_TEXTURE);
    select_region(REGION_BUNNY);
    define_region_topleft(0, 0, 15, 15);
    select_region(REGION_BLACK);
    define_region_topleft(32, 0, 35, 3);

    int frames = 0;
    int secondStart = get_frame_counter();

    while (true)
    {
        // The cycle counter counts the cycles of the console's current frame and starts again at
        // every frame, so work that runs past one frame would read low again. The frame counter
        // says how many frames went by: whole frames plus the difference in cycles
        int startFrame = get_frame_counter();
        int startCycles = get_cycle_counter();

        checkButtons();
        updateBunnies();
        drawBunnies();

        // the cycles this frame's work took, as a share of one frame's (past 100%: more than a
        // frame, so the frame rate drops)
        int cycles = (get_frame_counter() - startFrame) * CYCLES_PER_FRAME + get_cycle_counter() - startCycles;
        cpuPercent = cycles * 100 / CYCLES_PER_FRAME;

        // frames drawn in the last 60 of the console's frames
        frames++;
        int now = get_frame_counter();
        if (now - secondStart >= 60)
        {
            fps = frames * 60 / (now - secondStart);
            frames = 0;
            secondStart = now;
        }

        end_frame();
    }
    return 0;
}
