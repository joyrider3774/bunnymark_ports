# BunnyMark for the Pimoroni PicoSystem

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

`PicoSystem_BunnyMark.uf2`: hold X while switching on and copy it onto the drive that
appears. Built with the arduino-pico core (RP2040 at 125 MHz, ST7789 240x240 on SPI).

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| UP | +500 bunnies |
| DOWN | +1000 bunnies |
| A + B | back to 0 bunnies |
| X | the frame's depth: 16 → 8 → 1 → 16 bpp |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60 @ 16 bpp
bunnies 100
+----------------------+
|   bunnies bouncing   |
+----------------------+
```

The whole 240x240 display, 1:1, with the 16x16 bunny at half the Playdate's speed (the size every port uses so their frame rates can be compared). **FPS** is
counted over the last second, **@ 16 bpp** is the frame's depth, **bunnies** the count (at
most 10,000). The box and the text shake while a button that adds bunnies is held.

## What it measures

Each frame: every bunny moves (12.4 fixed point, the RP2040 has no FPU), the frame in RAM
is cleared and every bunny written into it, then the frame goes to the display over SPI at
62.5 MHz (`spi_write_blocking`, its own small ST7789 driver). The frame rate is how many of
those fit in a second.

The frame's depth is the one setting to switch, while it runs:

| Depth | Frame | The bunny | Sending |
|---|---|---|---|
| 16 bpp | 113 KB, RGB565 in the display's byte order | runs of 16-bit writes | the whole frame in one transfer |
| 8 bpp | 56 KB, RGB332 | runs of `memset` | each row turned into RGB565 first |
| 1 bpp | 7 KB | a mask and pattern per row, a few bytes | each row expanded first |

A 16 bpp frame is 14.7 ms on the wire at 62.5 MHz; `spi_write_blocking` takes 17.6 ms
(measured), so with no bunnies the frame rate is 54. 8 and 1 bpp draw faster but add the
conversion to that.

The SPI clock is at most half the RP2040's peripheral clock, which arduino-pico runs from the
48 MHz USB PLL: the 62.5 MHz asked for became 24 MHz, 46 ms a frame and 22 fps with no
bunnies. The sketch runs the peripheral clock from the system clock instead, and the build
sets that to 125 MHz (`freq=125`) so the SPI gets exactly 62.5 MHz, the ST7789's rating.

## Tested on a device

The figures above are from a PicoSystem, with a test build that prints the clocks and the time
spent per frame over USB serial (`-DBUNNYMARK_SERIAL=1`; the release build sends nothing).

## Building

`tools/build_releases.py --only PicoSystem` in the folder above (arduino-pico, Generic
RP2040 with the board options of the *_embedded games' release tool). The display's pins
and setup are the PicoSystem's (the 32blit SDK's ST7789 setup).

`BunnyGame.h` (the game), `BunnyFrame.h` (the frame), `bunny.h` and `font8x8_basic.h` are
copies of the ones in `common/` and `sdl/`, made by `tools/make_sprites.py`.
