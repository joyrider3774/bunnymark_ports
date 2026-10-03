# BunnyMark for the ESPboy

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

Two builds of one sketch (`BunnyMark/BunnyMark.ino`), one per display library:

| Release file | Library |
|---|---|
| `ESPboy_BunnyMark_LovyanGFX.bin` | [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (`LOVYANGFX 1`, the default) |
| `ESPboy_BunnyMark_TFT_eSPI.bin` | [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (`LOVYANGFX 0`) |

The bunnies are written straight into the frame's memory by the sketch itself, the
same code in both; the library clears the frame, draws the box and the text and sends
the frame to the display. So the two builds differ in what the libraries do every
frame, not in drawing the bunnies.
`BunnyMark/lib` is the ESPboy library the *_embedded games use (ESPboyInit, the
MCP23017 expander, the LED and LovyanGFX's ESPboy panel setup).

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| UP | +500 bunnies |
| DOWN | +1000 bunnies |
| A + B | back to 0 bunnies |
| right side button | frame buffer depth: 16 → 8 → 1 → 16 bpp |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

For testing from a PC there is a serial build (`BUNNYMARK_SERIAL`, see Building): it
prints a line a second on the serial port (115200 baud) and takes one-letter
commands, `a` +1, `b` +100, `u` +500, `d` +1000, `r` back to 0, `m` next depth:

```
fps 35 bunnies 1100 depth 16 lib LGFX heap 7920
```

The normal builds send and read nothing.

## The screen

```
FPS 47 @ 16 bpp LGFX
bunnies 100
+--------------------+
|  bunnies bouncing  |
+--------------------+
```

- **FPS**: frames per second, counted over the last second. It is counted again
  from zero after a depth change.
- **@ 16 bpp**: the frame buffer's depth. **LGFX / eSPI**: the library.
- **bunnies**: how many there are, at most 3000.

The box and the text shake while A, B, UP or DOWN is held, as on the Playdate.

## What it measures

Each frame does three things, one after the other, and the FPS is how many of these
frames fit in a second:

1. **Move**: every bunny moves by its speed and turns around at the box's edges, in
   quarter pixels (the ESP8266 has no FPU).
2. **Draw**: the frame, a 128x128 sprite in RAM, is filled white by the library and
   every bunny is written into its memory by the sketch, in a form made for the
   frame's depth (`bunny.h`): no library call and no colour conversion per bunny.
   This is what grows with the bunny count.
3. **Send**: the library sends the frame to the panel over SPI. The panel always
   takes 16-bit pixels, so an 8 or 1 bpp frame is turned into 16-bit pixels on the
   way out.

## The frame buffer depth

| Depth | Frame in RAM | The bunny | Drawing it | Sending |
|---|---|---|---|---|
| 16 bpp | 32 KB | its runs | a 16-bit write per pixel | as it is |
| 8 bpp | 16 KB | its runs | `memset` of RGB332 white or black | each pixel converted to RGB565 |
| 1 bpp | 2 KB | a 16-bit mask and white pattern per row | three bytes per row, masked | each bit expanded to RGB565 |

TFT_eSPI sends all three with `pushSprite`. With LovyanGFX the 8 and 1 bpp frames are
expanded row by row and sent as display-order RGB565: its `pushSprite` converts
them pixel by pixel, and its 1 bpp palette push leaves the ESPboy's panel empty
(both found in the *_embedded games, see their `PlatformESPboy.cpp`).

On a real ESPboy (ESP8266 at 160 MHz), frames per second:

| Bunnies | LovyanGFX 16 | 8 | 1 | TFT_eSPI 16 | 8 | 1 |
|---|---|---|---|---|---|---|
| 0 | 85 | | | 84 | | |
| 100 | 75 | 66 | 80 | 74 | 61 | 67 |
| 1100 | 35 | 24 | 60 | 34 | 23 | 53 |
| 1500 | 29 | | | 28 | | |

With no bunnies both are held by the SPI transfer (85 fps). With bunnies the two are
close, as the drawing is the same code: what is left is the libraries' clear and send.
1 bpp is the fastest to draw into (a few byte writes per bunny row); 8 bpp is slower
than 16 because every pixel is converted on the way to the display.

An earlier version drew every bunny run with the library's `drawFastHLine`. That
measured the libraries' drawing instead, at about a quarter of these rates
(LovyanGFX 8 fps and TFT_eSPI 14 at 1100 bunnies and 16 bpp): each call clips, and
converts its colour, and TFT_eSPI draws a 1 bpp line pixel by pixel.

## Differences from the Playdate version

- The screen is 128x128 instead of 400x240, so the bunny is 16x16 (half size) and
  moves at half the speed, as on the CHGame and every other port.
- Positions are integers in quarter pixels instead of floats.
- At most 3000 bunnies: the ESP8266 has 80 KB of RAM and the 16 bpp frame takes 32
  KB. A bunny is 3 bytes, as on the CHGame: x and y (9 bits each) in quarter pixels
  and each axis' speed as a 3-bit index into a symmetric table of eight; each new
  bunny starts a random 0-63 frames along its path so they don't stack. 3000 bunnies
  take the 9 KB that 1500 took at 6 bytes each, so about 8 KB of heap is still left.

## Building

`tools/build_releases.py` in the folder above builds both (`--only ESPboy`) with the
Arduino IDE 1.8's arduino-builder, or arduino-cli, and the board options the *_embedded
games' release tool uses (ESP8266 core 3.1.2, LOLIN(WEMOS) D1 mini, 160 MHz). It needs
LovyanGFX and TFT_eSPI installed. TFT_eSPI is set up for the ESPboy by the build's own
defines (`USER_SETUP_LOADED` and the ESPboy's ST7735: 128x128, BGR, DC on GPIO16, no CS
pin), so whatever its `User_Setup.h` holds does not matter.

For the serial test build, add `-DBUNNYMARK_SERIAL=1` to the build's defines.

To flash, with the esp8266 core's tools (the ESPboy shows up as a CH340 serial port):

```
python tools/upload.py --chip esp8266 --port COM3 --baud 921600 write_flash 0x0 ESPboy_BunnyMark_LovyanGFX.bin
```

`bunny.h` comes from `tools/make_sprites.py` in the folder above.
