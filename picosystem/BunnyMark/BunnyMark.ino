// BunnyMark for the Pimoroni PicoSystem (RP2040, ST7789 240x240 on SPI), for the arduino-pico core.
//
// A port of Timofffee's Playdate bunnymark; the game itself is BunnyGame.h, the frame it draws
// into BunnyFrame.h. Here: a small driver for the display, the buttons and sending the frame.
//
// Buttons: A +1 bunny, B +100, UP +500, DOWN +1000 (each once, when let go), A + B back to 0,
// X the frame's depth, 16 -> 8 -> 1 bpp. See README.md.

#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include "BunnyGame.h"

// the PicoSystem's display and buttons
#define LCD_SPI spi0
#define LCD_SPI_HZ 62500000
#define LCD_SCK_PIN 6
#define LCD_MOSI_PIN 7
#define LCD_CS_PIN 5
#define LCD_DC_PIN 9
#define LCD_RESET_PIN 4
#define LCD_BACKLIGHT_PIN 12
#define BUTTON_UP_PIN 23
#define BUTTON_DOWN_PIN 20
#define BUTTON_LEFT_PIN 22
#define BUTTON_RIGHT_PIN 21
#define BUTTON_A_PIN 18
#define BUTTON_B_PIN 19
#define BUTTON_X_PIN 17
#define BUTTON_Y_PIN 16

#define SCREEN_W 240
#define SCREEN_H 240

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
    // The SPI clock is at most half the peripheral clock, which the core runs from the 48 MHz USB
    // PLL: that gave 24 MHz and 46 ms a frame (measured, 22 fps with no bunnies). Run from the
    // system clock (125 MHz, see the build's freq), the SPI reaches the ST7789's 62.5 MHz
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys),
                    clock_get_hz(clk_sys));
    spi_init(LCD_SPI, LCD_SPI_HZ);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

    // a reset pulse, then the ST7789 set up as the 32blit SDK does it for the PicoSystem
    gpio_put(LCD_RESET_PIN, 0);
    delay(10);
    gpio_put(LCD_RESET_PIN, 1);
    delay(10);
    lcdCommand(0x01);                                                  // software reset
    delay(150);
    static const uint8_t colmod[] = { 0x05 };                          // 16 bits a pixel
    static const uint8_t porch[] = { 0x0c, 0x0c, 0x00, 0x33, 0x33 };
    static const uint8_t gate[] = { 0x14 };
    static const uint8_t vcom[] = { 0x37 };
    static const uint8_t lcm[] = { 0x2c };
    static const uint8_t vdvVrhEnable[] = { 0x01 };
    static const uint8_t vrh[] = { 0x12 };
    static const uint8_t vdv[] = { 0x20 };
    static const uint8_t power[] = { 0xa4, 0xa1 };
    static const uint8_t gammaPos[] = { 0xD0, 0x08, 0x11, 0x08, 0x0c, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2d };
    static const uint8_t gammaNeg[] = { 0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0b, 0x16, 0x14, 0x2f, 0x31 };
    static const uint8_t frameRate[] = { 0x15 };                       // 50 Hz
    static const uint8_t madctl[] = { 0x00 };                          // no rotation, RGB
    lcdCommand(0x3A, colmod, 1);
    lcdCommand(0xB2, porch, 5);
    lcdCommand(0xB7, gate, 1);
    lcdCommand(0xBB, vcom, 1);
    lcdCommand(0xC0, lcm, 1);
    lcdCommand(0xC2, vdvVrhEnable, 1);
    lcdCommand(0xC3, vrh, 1);
    lcdCommand(0xC4, vdv, 1);
    lcdCommand(0xD0, power, 2);
    lcdCommand(0xE0, gammaPos, 14);
    lcdCommand(0xE1, gammaNeg, 14);
    lcdCommand(0xC6, frameRate, 1);
    lcdCommand(0x21);                                                  // inversion on, the panel needs it
    lcdCommand(0x11);                                                  // out of sleep
    lcdCommand(0x29);                                                  // display on
    delay(100);
    lcdCommand(0x36, madctl, 1);

    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
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
    if (!digitalRead(BUTTON_X_PIN)) buttons |= GAME_DEPTH;
    return buttons;
}

void setup()
{
    lcdInit();
    const uint8_t pins[] = { BUTTON_UP_PIN, BUTTON_DOWN_PIN, BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN,
                             BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_X_PIN, BUTTON_Y_PIN };
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
        Serial.printf("sys %lu peri %lu spi %lu | frames %lu game %lu us send %lu us depth %d bunnies %d\n",
                      (unsigned long)clock_get_hz(clk_sys), (unsigned long)clock_get_hz(clk_peri),
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
