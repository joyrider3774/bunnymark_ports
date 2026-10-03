// BunnyMark for Vircon32: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, X five hundred, Y a thousand, each once when the
// button is let go; A + B takes them all away again. Every bunny moves and bounces off the
// box each frame; the screen shows the frame rate, how much of a frame's CPU time the last
// frame took, and the count. See README.md.
//
// The Playdate's movement code, floats and all (Vircon32's floats are full speed hardware),
// on the console's 640x360 screen, with the Playdate's box margins; the bunnies bounce at its
// edges. The bunny is the image at half size, 16x16, the size every port uses so their frame
// rates can be compared, at half the speed; the GPU draws it as a texture region, one draw_region_at
// per bunny.
//
// Vircon32 runs at a fixed 60 frames a second and end_frame() waits for the next one, so a
// frame that needs more than one frame's CPU time (250,000 cycles at 15 MHz) takes two:
// the frame rate falls to 30, 20, ... The CPU line shows how much of the budget is used,
// past 100% once the work takes more than a frame.
//
// The dialect (Vircon32's C compiler): every type is one 32-bit word, arrays are declared
// as int[10] name, there is no ?: operator and no #if.

#include "video.h"
#include "input.h"
#include "time.h"
#include "string.h"
#include "misc.h"

#define MAX_BUNNIES 100000

#define CYCLES_PER_FRAME 250000

// the regions of texture 0 (assets/bunny.png)
#define REGION_BUNNY 0
#define REGION_BLACK 1

struct Vector2
{
    float x;
    float y;
};

struct Bunny
{
    Vector2 position;
    Vector2 speed;
};

Bunny[MAX_BUNNIES] bunnies;
int bunniesCount = 0;
bool pressed = false;

// the box the bunnies bounce in, as the Playdate's (5,40 390x195 on 400x240), on 640x360
#define BOX_X 5
#define BOX_Y 40
#define BOX_W 630
#define BOX_H 315
#define BUNNY_SIZE 16

void instantiateBunnies( int count )
{
    for( int i = 0; i < count; i++ )
    {
        if( bunniesCount < MAX_BUNNIES )
        {
            Bunny* b = &bunnies[ bunniesCount ];
            b->position.x = 320.0;
            b->position.y = 180.0;
            // the Playdate's speeds halved, as the bunny is
            b->speed.x = (float)( rand() % 500 - 250 ) / 100.0;
            b->speed.y = (float)( rand() % 500 - 250 ) / 100.0;
            bunniesCount++;
        }
    }
}

// A bunny turns round where it reaches the box's edge (the Playdate's own bounds were set for its
// 32x32 bunny and left a gap with the 16x16 one)
void updateBunnies()
{
    for( int i = 0; i < bunniesCount; i++ )
    {
        Bunny* b = &bunnies[ i ];
        b->position.x += b->speed.x;
        b->position.y += b->speed.y;

        if( ( b->position.x > ( BOX_X + BOX_W - BUNNY_SIZE ) ) || ( b->position.x < BOX_X ) )
            b->speed.x *= -1;
        if( ( b->position.y > ( BOX_Y + BOX_H - BUNNY_SIZE ) ) || ( b->position.y < BOX_Y ) )
            b->speed.y *= -1;
    }
}

// a line of the box: the 4x4 black block stretched
void drawBar( int x, int y, int w, int h )
{
    select_region( REGION_BLACK );
    set_drawing_scale( (float)w / 4.0, (float)h / 4.0 );
    draw_region_zoomed_at( x, y );
}

void drawRect( int x, int y, int w, int h )
{
    drawBar( x, y, w, 1 );
    drawBar( x, y + h - 1, w, 1 );
    drawBar( x, y, 1, h );
    drawBar( x + w - 1, y, 1, h );
}

int fps = 0;
int cpuPercent = 0;

// The GPU draws at most 9 screens' worth of pixels a frame (640 x 360 x 9); a draw past that
// is dropped, silently. A bunny costs its whole 16x16 region, transparent pixels included, so
// about 7000 of them fill the frame's budget long before the CPU is busy. The box and the text
// are drawn before the bunnies so they are never the ones dropped, and the GPU line shows the
// budget the frame asked for: past 100% some bunnies were not drawn
#define GPU_PIXELS_PER_FRAME 2073600

void drawBunnies()
{
    clear_screen( color_white );

    int rX = 0;
    int rY = 0;
    if( pressed )
    {
        rX = rand() % 4 - 2;
        rY = rand() % 4 - 2;
    }
    drawRect( BOX_X + rX, BOX_Y + rY, BOX_W, BOX_H );

    // the BIOS font, drawn black
    int[ 64 ] text;
    int[ 16 ] number;
    strcpy( text, "FPS " );
    itoa( fps, number, 10 );
    strcat( text, number );
    strcat( text, "  CPU " );
    itoa( cpuPercent, number, 10 );
    strcat( text, number );
    strcat( text, "%  GPU " );
    // the clear and the bunnies; the box and the text are a few thousand pixels more
    itoa( ( 640 * 360 + bunniesCount * 16 * 16 ) / ( GPU_PIXELS_PER_FRAME / 100 ), number, 10 );
    strcat( text, number );
    strcat( text, "%" );
    set_multiply_color( color_black );
    print_at( 10 + rX, 10 + rY, text );
    strcpy( text, "bunnies: " );
    itoa( bunniesCount, number, 10 );
    strcat( text, number );
    print_at( 400 + rX, 10 + rY, text );
    set_multiply_color( color_white );

    select_texture( 0 );
    select_region( REGION_BUNNY );
    for( int i = 0; i < bunniesCount; i++ )
        draw_region_at( (int)bunnies[ i ].position.x, (int)bunnies[ i ].position.y );
}

// A gamepad button reads above 0 while it is held (the frames it has been) and below 0 once
// it is up (-1 on the first frame after it is let go)
bool releasedNow( int button )
{
    return button == -1;
}

// A + B was held at some point since A and B were last both up: their releases then end the
// reset and add nothing
bool resetHeld = false;

void checkButtons()
{
    int a = gamepad_button_a();
    int b = gamepad_button_b();
    int x = gamepad_button_x();
    int y = gamepad_button_y();

    if( ( a > 0 ) && ( b > 0 ) )
    {
        bunniesCount = 0;
        resetHeld = true;
    }
    // a button adds its bunnies once, when it is let go
    if( !resetHeld )
    {
        if( releasedNow( a ) )
            instantiateBunnies( 1 );
        if( releasedNow( b ) )
            instantiateBunnies( 100 );
    }
    if( ( a <= 0 ) && ( b <= 0 ) )
        resetHeld = false;
    if( releasedNow( x ) )
        instantiateBunnies( 500 );
    if( releasedNow( y ) )
        instantiateBunnies( 1000 );

    // the box shakes while a button is held, as on the Playdate
    pressed = ( a > 0 ) || ( b > 0 ) || ( x > 0 ) || ( y > 0 );
}

void main()
{
    select_gamepad( 0 );
    srand( get_time() );

    select_texture( 0 );
    select_region( REGION_BUNNY );
    define_region_topleft( 0, 0, 15, 15 );
    select_region( REGION_BLACK );
    define_region_topleft( 32, 0, 35, 3 );

    int frames = 0;
    int secondStart = get_frame_counter();

    while( true )
    {
        // The cycle counter counts the cycles of the console's current frame and starts again
        // at every frame, so work that runs past one frame would read low again. The frame
        // counter says how many frames went by: whole frames plus the difference in cycles
        int startFrame = get_frame_counter();
        int startCycles = get_cycle_counter();

        checkButtons();
        updateBunnies();
        drawBunnies();

        // the cycles this frame's work took, as a share of one frame's (past 100%: more than a
        // frame, so the frame rate drops)
        int cycles = ( get_frame_counter() - startFrame ) * CYCLES_PER_FRAME + get_cycle_counter() - startCycles;
        cpuPercent = cycles * 100 / CYCLES_PER_FRAME;

        // frames drawn in the last 60 of the console's frames
        frames++;
        int now = get_frame_counter();
        if( now - secondStart >= 60 )
        {
            fps = frames * 60 / ( now - secondStart );
            frames = 0;
            secondStart = now;
        }

        end_frame();
    }
}
