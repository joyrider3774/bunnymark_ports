// BunnyMark for the Pimoroni Explorer 2350 and Tufty 2350 (RP2350B, ST7789 320x240 on an 8 bit
// parallel bus), for the arduino-pico core. One sketch for both: the board picked when building
// says which (Explorer: its own board, ARDUINO_PIMORONI_EXPLORER; Tufty: Generic RP2350, RP2350B).
//
// A port of Timofffee's Playdate bunnymark; the game itself is BunnyGame.h, the frame it draws
// into BunnyFrame.h. Here: a driver for the display's parallel bus (a PIO program fed by DMA),
// the buttons and sending the frame.
//
// Buttons, Explorer: A +1 bunny, B +100, C +500, X +1000 (each once, when let go), A + B back to
// 0, Y the frame's depth, 16 -> 8 -> 1 bpp.
// Buttons, Tufty: A +1, B +100, UP +500, DOWN +1000, A + B back to 0, C the frame's depth.
// See README.md.

#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>
#include <math.h>
#include "BunnyGame.h"
#if !defined(ARDUINO_PIMORONI_EXPLORER)
#include "TuftyPower.h"
#endif

// the display, the same on both: data on GPIO 32-39, write strobe 30, read strobe 31, chip select
// 27, data/command 28, backlight 26, no reset pin. The Tufty's panel is mounted the other way round
#define LCD_PIO pio1
// The PIO clock, three steps a byte. Pimoroni's driver stops at 32 MHz (150 / 5 = 30: 15.4 ms a
// frame, measured); the ST7789 needs a write cycle of 66 ns and the strobe 15 ns low and high, which
// 37.5 MHz (150 / 4) keeps: 80 ns a byte, 27 ns phases
#if defined(ARDUINO_PIMORONI_EXPLORER)
#define LCD_PIO_MAX_HZ 37500000
#else
#define LCD_PIO_MAX_HZ 32000000       // the Tufty at Pimoroni's speed for now
#endif
#define LCD_D0_PIN 32
#define LCD_WR_PIN 30
#define LCD_RD_PIN 31
#define LCD_CS_PIN 27
#define LCD_DC_PIN 28
#define LCD_BACKLIGHT_PIN 26
// a PIO block reaches GPIO 0-31 or 16-47: the display's is moved to 16-47 for the data pins
#define LCD_PIO_GPIO_BASE 16

#if defined(ARDUINO_PIMORONI_EXPLORER)
#define LCD_MADCTL 0x60               // 320 wide, 240 high
#define BUTTON_A_PIN 16
#define BUTTON_B_PIN 15
#define BUTTON_C_PIN 14
#define BUTTON_X_PIN 17
#define BUTTON_Y_PIN 18
#define BUTTON_Z_PIN 19
#else
// Tufty 2350
#define LCD_MADCTL 0xA0               // 320 wide, 240 high, turned 180 degrees
// the backlight is PWM, at this brightness of 255 (gamma 2.8)
#define LCD_BACKLIGHT 200
#define BUTTON_A_PIN 7
#define BUTTON_B_PIN 9
#define BUTTON_C_PIN 10
#define BUTTON_UP_PIN 11
#define BUTTON_DOWN_PIN 6
#endif

#define SCREEN_W 320
#define SCREEN_H 240

static BunnyFrame frame;

// a message to a computer that has the USB serial port open
static void bunnyLog(const char *text)
{
    if (Serial)
        Serial.print(text);
}
static uint8_t lines[2][SCREEN_W * 2];

// ---------------------------------------------------------------------------- the parallel bus
//
// The PIO program, a byte per run, the write strobe as side-set:
//     out pins, 8   side 1    the byte on the data pins, strobe high
//     nop           side 0    strobe low
//     nop           side 1    strobe high: the display takes the byte
// It waits for the next byte at the first step, with the strobe high, so the strobe is high
// whenever chip select or data/command change (a program that waits with it low gives the display
// one more write of the last byte when chip select goes high). It shifts out the top 8 bits of a
// word: an 8 bit DMA write to the FIFO puts the byte in all four bytes of the word.

static const uint16_t parallelInstructions[] = { 0x7008, 0xA042, 0xB042 };
static const pio_program_t parallelProgram = { parallelInstructions, 3, -1 };
static uint lcdSm = 0, lcdDma = 0;

static void busSend(const uint8_t *data, uint32_t length)
{
    dma_channel_wait_for_finish_blocking(lcdDma);
    dma_channel_set_read_addr(lcdDma, data, false);
    dma_channel_set_transfer_count(lcdDma, dma_encode_transfer_count(length), true);
}

// returns once every byte sent is on the display, before data/command or chip select change: the
// FIFO empty, then the state machine stalled on it (the stall flag cleared first, so a stall from
// before the last byte does not count)
static void busDrain(void)
{
    dma_channel_wait_for_finish_blocking(lcdDma);
    const uint32_t stalled = 1u << (PIO_FDEBUG_TXSTALL_LSB + lcdSm);
    while (!pio_sm_is_tx_fifo_empty(LCD_PIO, lcdSm))
        tight_loop_contents();
    LCD_PIO->fdebug = stalled;
    while (!(LCD_PIO->fdebug & stalled))
        tight_loop_contents();
}

static void lcdCommand(uint8_t command, const uint8_t *args = nullptr, int count = 0)
{
    static uint8_t buffer[16];
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    buffer[0] = command;
    busSend(buffer, 1);
    busDrain();
    gpio_put(LCD_DC_PIN, 1);
    if (count)
    {
        memcpy(buffer, args, (size_t)count);
        busSend(buffer, (uint32_t)count);
        busDrain();
    }
    gpio_put(LCD_CS_PIN, 1);
}

static void busInit(void)
{
    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_put(LCD_CS_PIN, 1);
    gpio_init(LCD_DC_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);
    gpio_put(LCD_DC_PIN, 1);
    gpio_init(LCD_RD_PIN);                    // nothing is read from the display
    gpio_set_dir(LCD_RD_PIN, GPIO_OUT);
    gpio_put(LCD_RD_PIN, 1);

    // only possible while the block has no program loaded yet
    if (pio_set_gpio_base(LCD_PIO, LCD_PIO_GPIO_BASE) != PICO_OK)
        bunnyLog("the display's PIO block can not reach its pins\n");
    lcdSm = (uint)pio_claim_unused_sm(LCD_PIO, true);
    const uint offset = (uint)pio_add_program(LCD_PIO, &parallelProgram);
    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, offset, offset + 2);
    sm_config_set_sideset(&config, 1, false, false);
    sm_config_set_out_pins(&config, LCD_D0_PIN, 8);
    sm_config_set_sideset_pins(&config, LCD_WR_PIN);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&config, false, true, 8);     // most significant bit first, 8 at a time
    // a whole divider, a fraction would make the strobe uneven
    const uint32_t sysHz = clock_get_hz(clk_sys);
    sm_config_set_clkdiv_int_frac8(&config, (sysHz + LCD_PIO_MAX_HZ - 1) / LCD_PIO_MAX_HZ, 0);
    pio_sm_init(LCD_PIO, lcdSm, offset, &config);
    pio_gpio_init(LCD_PIO, LCD_WR_PIN);
    for (uint i = 0; i < 8; i++)
        pio_gpio_init(LCD_PIO, LCD_D0_PIN + i);
    pio_sm_set_consecutive_pindirs(LCD_PIO, lcdSm, LCD_WR_PIN, 1, true);
    pio_sm_set_consecutive_pindirs(LCD_PIO, lcdSm, LCD_D0_PIN, 8, true);
    pio_sm_set_enabled(LCD_PIO, lcdSm, true);

    lcdDma = (uint)dma_claim_unused_channel(true);
    dma_channel_config dmaConfig = dma_channel_get_default_config(lcdDma);
    channel_config_set_transfer_data_size(&dmaConfig, DMA_SIZE_8);
    channel_config_set_dreq(&dmaConfig, pio_get_dreq(LCD_PIO, lcdSm, true));
    dma_channel_configure(lcdDma, &dmaConfig, &LCD_PIO->txf[lcdSm], nullptr, 0, false);
}

// ---------------------------------------------------------------------------- the display

static void lcdClear(void);

static void lcdInit(void)
{
#if defined(ARDUINO_PIMORONI_EXPLORER)
    pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
#else
    // dark until the display shows something: PWM with a 16 bit period
    gpio_set_function(LCD_BACKLIGHT_PIN, GPIO_FUNC_PWM);
    const uint backlightSlice = pwm_gpio_to_slice_num(LCD_BACKLIGHT_PIN);
    pwm_set_wrap(backlightSlice, 65535);
    pwm_set_gpio_level(LCD_BACKLIGHT_PIN, 0);
    pwm_set_enabled(backlightSlice, true);
#endif
    busInit();
    // no reset pin: a software reset, then the ST7789 set up as Pimoroni's driver does it for
    // this panel (without its gamma tables)
    lcdCommand(0x01);
    delay(150);
    static const uint8_t te[] = { 0x00 };                 // tearing effect line on
    static const uint8_t colmod[] = { 0x05 };             // 16 bits a pixel
    static const uint8_t porch[] = { 0x0c, 0x0c, 0x00, 0x33, 0x33 };
    static const uint8_t lcm[] = { 0x2c };
    static const uint8_t vdvVrhEnable[] = { 0x01 };
    static const uint8_t vrh[] = { 0x12 };
    static const uint8_t vdv[] = { 0x20 };
    static const uint8_t power[] = { 0xa4, 0xa1 };
    static const uint8_t frameRate[] = { 0x0f };          // 60 Hz
    static const uint8_t ramctl[] = { 0x00, 0xc0 };       // pixels high byte first
    static const uint8_t gate[] = { 0x35 };
    static const uint8_t vcom[] = { 0x1f };
    static const uint8_t madctl[] = { LCD_MADCTL };
    lcdCommand(0x35, te, 1);
    lcdCommand(0x3A, colmod, 1);
    lcdCommand(0xB2, porch, 5);
    lcdCommand(0xC0, lcm, 1);
    lcdCommand(0xC2, vdvVrhEnable, 1);
    lcdCommand(0xC3, vrh, 1);
    lcdCommand(0xC4, vdv, 1);
    lcdCommand(0xD0, power, 2);
    lcdCommand(0xC6, frameRate, 1);
    lcdCommand(0xB0, ramctl, 2);
    lcdCommand(0xB7, gate, 1);
    lcdCommand(0xBB, vcom, 1);
    lcdCommand(0x36, madctl, 1);
    lcdCommand(0x21);                                     // inversion on, the panel needs it
    lcdCommand(0x11);                                     // out of sleep
    delay(120);
    lcdCommand(0x29);                                     // display on
#if defined(ARDUINO_PIMORONI_EXPLORER)
    digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
#else
    // black before the light comes on
    lcdClear();
    pwm_set_gpio_level(LCD_BACKLIGHT_PIN, (uint16_t)(pow(LCD_BACKLIGHT / 255.0f, 2.8f) * 65535.0f + 0.5f));
#endif
}

// the whole display black, a row of zeros sent again and again
static void lcdClear(void)
{
    static const uint8_t columns[] = { 0, 0, (SCREEN_W - 1) >> 8, (SCREEN_W - 1) & 0xFF };
    static const uint8_t rows[] = { 0, 0, (SCREEN_H - 1) >> 8, (SCREEN_H - 1) & 0xFF };
    lcdCommand(0x2A, columns, 4);
    lcdCommand(0x2B, rows, 4);
    static uint8_t ramwr = 0x2C;
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    busSend(&ramwr, 1);
    busDrain();
    gpio_put(LCD_DC_PIN, 1);
    memset(lines[0], 0, sizeof(lines[0]));
    for (int y = 0; y < SCREEN_H; y++)
        busSend(lines[0], sizeof(lines[0]));
    busDrain();
    gpio_put(LCD_CS_PIN, 1);
}

// The frame to the display: the whole screen as the window, then the pixels by DMA. A 16 bpp frame
// is already in the display's byte order and goes as one transfer; 8 and 1 bpp ones are turned into
// RGB565 a row at a time, into two row buffers in turn, so a row is converted while the one before
// it is still going out
static void present(void)
{
    static const uint8_t columns[] = { 0, 0, (SCREEN_W - 1) >> 8, (SCREEN_W - 1) & 0xFF };
    static const uint8_t rows[] = { 0, 0, (SCREEN_H - 1) >> 8, (SCREEN_H - 1) & 0xFF };
    lcdCommand(0x2A, columns, 4);
    lcdCommand(0x2B, rows, 4);
    static uint8_t ramwr = 0x2C;
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    busSend(&ramwr, 1);
    busDrain();
    gpio_put(LCD_DC_PIN, 1);
    if (frame.depth == 16)
    {
        busSend(frame.pixels, (uint32_t)SCREEN_W * SCREEN_H * 2);
    }
    else
    {
        for (int y = 0; y < SCREEN_H; y++)
        {
            uint8_t *line = lines[y & 1];
            // the buffer converted into now was handed to DMA two rows ago: busSend waited for
            // that one before starting the last
            bunnyFrameRowToDisplay(frame, y, line);
            busSend(line, sizeof(lines[0]));
        }
    }
    busDrain();
    gpio_put(LCD_CS_PIN, 1);
}

// ---------------------------------------------------------------------------- the buttons

static uint8_t readButtons(void)
{
    uint8_t buttons = 0;
    if (!digitalRead(BUTTON_A_PIN)) buttons |= GAME_ADD1;
    if (!digitalRead(BUTTON_B_PIN)) buttons |= GAME_ADD100;
#if defined(ARDUINO_PIMORONI_EXPLORER)
    if (!digitalRead(BUTTON_C_PIN)) buttons |= GAME_ADD500;
    if (!digitalRead(BUTTON_X_PIN)) buttons |= GAME_ADD1000;
    if (!digitalRead(BUTTON_Y_PIN)) buttons |= GAME_DEPTH;
#else
    if (!digitalRead(BUTTON_UP_PIN)) buttons |= GAME_ADD500;
    if (!digitalRead(BUTTON_DOWN_PIN)) buttons |= GAME_ADD1000;
    if (!digitalRead(BUTTON_C_PIN)) buttons |= GAME_DEPTH;
#endif
    return buttons;
}

void setup()
{
#if !defined(ARDUINO_PIMORONI_EXPLORER)
    // first: a long press of RESET goes to sleep before anything shows, and the display's power
    // comes on (TuftyPower.h)
    tuftyPowerStartup();
#endif
    lcdInit();
#if defined(ARDUINO_PIMORONI_EXPLORER)
    const uint8_t pins[] = { BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_C_PIN, BUTTON_X_PIN, BUTTON_Y_PIN, BUTTON_Z_PIN };
#else
    const uint8_t pins[] = { BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_C_PIN, BUTTON_UP_PIN, BUTTON_DOWN_PIN };
#endif
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
        Serial.printf("sys %lu | frames %lu game %lu us send %lu us depth %d bunnies %d\n",
                      (unsigned long)clock_get_hz(clk_sys), (unsigned long)testFrames,
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
