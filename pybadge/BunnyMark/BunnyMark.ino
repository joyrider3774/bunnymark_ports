// BunnyMark for the Adafruit PyBadge and PyGamer (SAMD51, ST7735 160x128). One sketch for both:
// the board picked when building says which (the PyGamer's d-pad is an analog joystick).
//
// A port of Timofffee's Playdate bunnymark; the game itself is BunnyGame.h, the frame it draws
// into BunnyFrame.h. Here: the display (Adafruit_ST7735), the buttons and sending the frame.
//
// Buttons: A +1 bunny, B +100, UP +500, DOWN +1000 (each once, when let go), A + B back to 0,
// SELECT the frame's depth, 16 -> 8 -> 1 bpp. See README.md.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "BunnyGame.h"

// the display and the buttons, on the pins Adafruit's Arcada library uses for both boards
#define TFT_CS_PIN 44
#define TFT_DC_PIN 45
#define TFT_RST_PIN 46
#define TFT_BACKLIGHT_PIN 47
#define BUTTON_CLOCK_PIN 48
#define BUTTON_DATA_PIN 49
#define BUTTON_LATCH_PIN 50

// the buttons' bits in the shift register, 1 while held; only the PyBadge has its d-pad there
#define SHIFT_B      0x80
#define SHIFT_A      0x40
#define SHIFT_START  0x20
#define SHIFT_SELECT 0x10
#define SHIFT_RIGHT  0x08
#define SHIFT_DOWN   0x04
#define SHIFT_UP     0x02
#define SHIFT_LEFT   0x01

#if defined(ADAFRUIT_PYGAMER_M4_EXPRESS)
#define JOYSTICK_Y_PIN A10
// 0..1023 with the middle at 512; further than this from the middle is a direction (as in Arcada)
#define JOYSTICK_THRESHOLD 350
#endif

#define SCREEN_W 160
#define SCREEN_H 128
#define TFT_SPI_HZ 50000000         // the 100 MHz SERCOM clock halved

static Adafruit_ST7735 tft(&SPI1, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
static BunnyFrame frame;
static uint8_t line[SCREEN_W * 2];

// the latch loads the buttons into the shift register, then they are clocked out most
// significant bit first
static uint8_t readShiftRegister(void)
{
    digitalWrite(BUTTON_LATCH_PIN, LOW);
    delayMicroseconds(1);
    digitalWrite(BUTTON_LATCH_PIN, HIGH);
    delayMicroseconds(1);
    uint8_t held = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        held = (uint8_t)((held << 1) | (digitalRead(BUTTON_DATA_PIN) ? 1 : 0));
        digitalWrite(BUTTON_CLOCK_PIN, HIGH);
        delayMicroseconds(1);
        digitalWrite(BUTTON_CLOCK_PIN, LOW);
        delayMicroseconds(1);
    }
    return held;
}

static uint8_t readButtons(void)
{
    const uint8_t held = readShiftRegister();
    uint8_t buttons = 0;
    if (held & SHIFT_A) buttons |= GAME_ADD1;
    if (held & SHIFT_B) buttons |= GAME_ADD100;
    if (held & SHIFT_SELECT) buttons |= GAME_DEPTH;
#if defined(ADAFRUIT_PYGAMER_M4_EXPRESS)
    const int y = analogRead(JOYSTICK_Y_PIN) - 512;
    if (y < -JOYSTICK_THRESHOLD) buttons |= GAME_ADD500;
    if (y > JOYSTICK_THRESHOLD) buttons |= GAME_ADD1000;
#else
    if (held & SHIFT_UP) buttons |= GAME_ADD500;
    if (held & SHIFT_DOWN) buttons |= GAME_ADD1000;
#endif
    return buttons;
}

// The frame to the display: a 16 bpp frame is already in the display's byte order and goes as
// one transfer; 8 and 1 bpp ones are turned into RGB565 a row at a time
static void present(void)
{
    tft.startWrite();
    tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
    if (frame.depth == 16)
    {
        tft.writePixels((uint16_t *)(void *)frame.pixels, (uint32_t)SCREEN_W * SCREEN_H, true, true);
    }
    else
    {
        for (int y = 0; y < SCREEN_H; y++)
        {
            bunnyFrameRowToDisplay(frame, y, line);
            tft.writePixels((uint16_t *)(void *)line, SCREEN_W, true, true);
        }
    }
    tft.endWrite();
}

void setup()
{
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(TFT_BACKLIGHT_PIN, LOW);
    tft.initR(INITR_BLACKTAB);
    // The SPI clock is at most half its SERCOM's clock, 48 MHz by default: the library's 32 MHz
    // became 24, 13.7 ms a frame (measured). From the 100 MHz clock it can run at 50
    SPI1.setClockSource(SERCOM_CLOCK_SOURCE_100M);
    tft.setSPISpeed(TFT_SPI_HZ);
    tft.setRotation(1);           // 160 wide, 128 high
    tft.fillScreen(0x0000);
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

    pinMode(BUTTON_LATCH_PIN, OUTPUT);
    pinMode(BUTTON_CLOCK_PIN, OUTPUT);
    pinMode(BUTTON_DATA_PIN, INPUT);
    digitalWrite(BUTTON_LATCH_PIN, HIGH);
    digitalWrite(BUTTON_CLOCK_PIN, LOW);

    bunnyFrameInit(frame, SCREEN_W, SCREEN_H);
    bunnyFrameSetDepth(frame, 16);
    bunnyGameSetup(frame, micros() ^ (uint32_t)analogRead(A1) << 16, micros());
}

#if BUNNYMARK_SERIAL
// test builds only (-DBUNNYMARK_SERIAL=1): where a frame's time goes, once a second
static uint32_t testGame = 0, testSend = 0, testFrames = 0, testStart = 0;
#endif

void loop()
{
#if BUNNYMARK_SERIAL
    const uint32_t t0 = micros();
    bunnyGameFrame(frame, readButtons(), t0);
    const uint32_t t1 = micros();
    present();
    const uint32_t t2 = micros();
    testGame += t1 - t0;
    testSend += t2 - t1;
    testFrames++;
    if (t2 - testStart >= 1000000)
    {
        Serial.print("cpu ");
        Serial.print(F_CPU);
        Serial.print(" | frames ");
        Serial.print(testFrames);
        Serial.print(" game ");
        Serial.print(testGame / testFrames);
        Serial.print(" us send ");
        Serial.print(testSend / testFrames);
        Serial.print(" us depth ");
        Serial.print(frame.depth);
        Serial.print(" bunnies ");
        Serial.println(bunnyCount);
        testGame = testSend = testFrames = 0;
        testStart = t2;
    }
#else
    bunnyGameFrame(frame, readButtons(), micros());
    present();
#endif
}
