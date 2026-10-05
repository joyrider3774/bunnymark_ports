// BunnyMark for the TinyCircuits Thumby Color (RP2350A, GC9107 128x128 on SPI), for the
// arduino-pico core. The same sketch builds for the RP2350's Cortex-M33 cores and for its
// Hazard3 RISC-V cores (the board's CPU Architecture option).
//
// A port of Timofffee's Playdate bunnymark; the game itself is BunnyGame.h, the frame it draws
// into BunnyFrame.h. Here: a small driver for the display, the buttons and sending the frame.
// The pins and the display's setup are the ones of the *_embedded games' Thumby Color platform
// (CircuitPython's board files and TinyCircuits' own engine).
//
// Buttons: A +1 bunny, B +100, UP +500, DOWN +1000 (each once, when let go), A + B back to 0,
// MENU the frame's depth, 16 -> 8 -> 1 bpp. See README.md.

#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include "BunnyGame.h"

// the Thumby Color's display, buttons and speaker
#define LCD_SPI spi0
#define LCD_SPI_HZ 75000000
#define LCD_SCK_PIN 18
#define LCD_MOSI_PIN 19
#define LCD_CS_PIN 17
#define LCD_DC_PIN 16
#define LCD_RESET_PIN 4
#define LCD_BACKLIGHT_PIN 7
#define SPEAKER_ENABLE_PIN 20
#define BUTTON_LEFT_PIN 0
#define BUTTON_UP_PIN 1
#define BUTTON_RIGHT_PIN 2
#define BUTTON_DOWN_PIN 3
#define BUTTON_A_PIN 21
#define BUTTON_B_PIN 25
#define BUTTON_LB_PIN 6
#define BUTTON_RB_PIN 22
#define BUTTON_MENU_PIN 26

#define SCREEN_W 128
#define SCREEN_H 128

static BunnyFrame frame;
static uint8_t line[SCREEN_W * 2];

// ---------------------------------------------------------------------------- the display

static void lcdCommand(uint8_t command, const uint8_t *args = nullptr, int count = 0)
{
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    spi_write_blocking(LCD_SPI, &command, 1);
    gpio_put(LCD_DC_PIN, 1);
    if (count)
        spi_write_blocking(LCD_SPI, args, count);
    gpio_put(LCD_CS_PIN, 1);
}

static void lcdInit(void)
{
    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_put(LCD_CS_PIN, 1);
    gpio_init(LCD_DC_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);
    gpio_put(LCD_DC_PIN, 1);
    gpio_init(LCD_RESET_PIN);
    gpio_set_dir(LCD_RESET_PIN, GPIO_OUT);
    gpio_put(LCD_RESET_PIN, 1);
    // The SPI clock is at most half the peripheral clock. Run from the system clock (150 MHz, see
    // the build's freq) that is 75 MHz, within the GC9107's 80, as on the PicoSystem
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys),
                    clock_get_hz(clk_sys));
    spi_init(LCD_SPI, LCD_SPI_HZ);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

    // a reset pulse, then the GC9107 set up as TinyCircuits' engine does it
    gpio_put(LCD_RESET_PIN, 0);
    delay(10);
    gpio_put(LCD_RESET_PIN, 1);
    delay(120);
    static const uint8_t b0[] = { 0xC0 }, b1[] = { 0x80 }, b2[] = { 0x2F }, b3[] = { 0x03 };
    static const uint8_t b7[] = { 0x01 }, b6[] = { 0x19 };
    static const uint8_t ac[] = { 0xC8 };                              // RGB565 the usual way round
    static const uint8_t ab[] = { 0x0F };
    static const uint8_t colmod[] = { 0x05 };                          // 16 bits a pixel
    static const uint8_t b4[] = { 0x04 };
    static const uint8_t frameRate[] = { 0x07 };
    static const uint8_t b8[] = { 0x08 };
    static const uint8_t vreg[] = { 0x5A }, vgh[] = { 0x23 }, vgl[] = { 0x47 }, vghvglClock[] = { 0x99 };
    static const uint8_t c6[] = { 0x30 }, c7[] = { 0x1F };
    static const uint8_t gamma1[] = { 0x05, 0x1D, 0x51, 0x2F, 0x85, 0x2A, 0x11, 0x62, 0x00, 0x07, 0x07, 0x0F, 0x08, 0x1F };
    static const uint8_t gamma2[] = { 0x2E, 0x41, 0x62, 0x56, 0xA5, 0x3A, 0x3F, 0x60, 0x0F, 0x07, 0x0A, 0x18, 0x18, 0x1D };
    lcdCommand(0xFE);                                                  // inter register enable 1
    lcdCommand(0xEF);                                                  // inter register enable 2
    lcdCommand(0xB0, b0, 1);
    lcdCommand(0xB1, b1, 1);
    lcdCommand(0xB2, b2, 1);
    lcdCommand(0xB3, b3, 1);
    lcdCommand(0xB7, b7, 1);
    lcdCommand(0xB6, b6, 1);
    lcdCommand(0xAC, ac, 1);
    lcdCommand(0xAB, ab, 1);
    lcdCommand(0x3A, colmod, 1);
    lcdCommand(0xB4, b4, 1);
    lcdCommand(0xA8, frameRate, 1);
    lcdCommand(0xB8, b8, 1);
    lcdCommand(0xE7, vreg, 1);
    lcdCommand(0xE8, vgh, 1);
    lcdCommand(0xE9, vgl, 1);
    lcdCommand(0xEA, vghvglClock, 1);
    lcdCommand(0xC6, c6, 1);
    lcdCommand(0xC7, c7, 1);
    lcdCommand(0xF0, gamma1, 14);
    lcdCommand(0xF1, gamma2, 14);
    lcdCommand(0x11);                                                  // out of sleep
    delay(120);
    lcdCommand(0x29);                                                  // display on
    delay(10);

    // the backlight on PWM, at the *_embedded games' 200 of 255 with their gamma of 2.8
    gpio_set_function(LCD_BACKLIGHT_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(LCD_BACKLIGHT_PIN);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(LCD_BACKLIGHT_PIN, 129);
    pwm_set_enabled(slice, true);
}

// The frame to the display: the whole screen as the window, then the pixels. A 16 bpp frame is
// already in the display's byte order and goes as one transfer; 8 and 1 bpp ones are turned into
// RGB565 a row at a time
static void present(void)
{
    static const uint8_t columns[] = { 0, 0, (SCREEN_W - 1) >> 8, (SCREEN_W - 1) & 0xFF };
    static const uint8_t rows[] = { 0, 0, (SCREEN_H - 1) >> 8, (SCREEN_H - 1) & 0xFF };
    lcdCommand(0x2A, columns, 4);
    lcdCommand(0x2B, rows, 4);
    const uint8_t ramwr = 0x2C;
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    spi_write_blocking(LCD_SPI, &ramwr, 1);
    gpio_put(LCD_DC_PIN, 1);
    if (frame.depth == 16)
    {
        spi_write_blocking(LCD_SPI, frame.pixels, (size_t)SCREEN_W * SCREEN_H * 2);
    }
    else
    {
        for (int y = 0; y < SCREEN_H; y++)
        {
            bunnyFrameRowToDisplay(frame, y, line);
            spi_write_blocking(LCD_SPI, line, sizeof(line));
        }
    }
    gpio_put(LCD_CS_PIN, 1);
}

// ---------------------------------------------------------------------------- the buttons

static uint8_t readButtons(void)
{
    uint8_t buttons = 0;
    if (!digitalRead(BUTTON_A_PIN)) buttons |= GAME_ADD1;
    if (!digitalRead(BUTTON_B_PIN)) buttons |= GAME_ADD100;
    if (!digitalRead(BUTTON_UP_PIN)) buttons |= GAME_ADD500;
    if (!digitalRead(BUTTON_DOWN_PIN)) buttons |= GAME_ADD1000;
    if (!digitalRead(BUTTON_MENU_PIN)) buttons |= GAME_DEPTH;
    return buttons;
}

void setup()
{
    // the speaker stays switched off, BunnyMark makes no sound
    pinMode(SPEAKER_ENABLE_PIN, OUTPUT);
    digitalWrite(SPEAKER_ENABLE_PIN, LOW);
    lcdInit();
    const uint8_t pins[] = { BUTTON_UP_PIN, BUTTON_DOWN_PIN, BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN,
                             BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_LB_PIN, BUTTON_RB_PIN, BUTTON_MENU_PIN };
    for (uint8_t i = 0; i < sizeof(pins); i++)
        pinMode(pins[i], INPUT_PULLUP);

    bunnyFrameInit(frame, SCREEN_W, SCREEN_H);
    bunnyFrameSetDepth(frame, 16);
    bunnyGameSetup(frame, rp2040.hwrand32(), micros());
}

#if BUNNYMARK_SERIAL
// test builds only (-DBUNNYMARK_SERIAL=1): the clocks, and where a frame's time goes, once a second
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
#ifdef __riscv
        const char *cpu = "RISC-V";
#else
        const char *cpu = "ARM";
#endif
        Serial.printf("%s sys %lu peri %lu spi %lu | frames %lu game %lu us send %lu us depth %d bunnies %d\n",
                      cpu, (unsigned long)clock_get_hz(clk_sys), (unsigned long)clock_get_hz(clk_peri),
                      (unsigned long)spi_get_baudrate(LCD_SPI), (unsigned long)testFrames,
                      (unsigned long)(testGame / testFrames), (unsigned long)(testSend / testFrames),
                      frame.depth, bunnyCount);
        testGame = testSend = testFrames = 0;
        testStart = t2;
    }
#else
    bunnyGameFrame(frame, readButtons(), micros());
    present();
#endif
}
