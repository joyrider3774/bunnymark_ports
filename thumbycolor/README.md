# BunnyMark for the TinyCircuits Thumby Color

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

The Thumby Color is an RP2350A (520 KB of RAM, 16 MB of flash) with a 128x128 GC9107 display on
SPI. The RP2350 has two kinds of core on the one chip, two Cortex-M33 and two Hazard3 RISC-V, and
starts whichever the program is built for, so the same sketch is released twice:

| File | Built for |
|---|---|
| `ThumbyColor_BunnyMark_ARM.uf2` | the Cortex-M33 cores (UF2 family rp2350-arm-s) |
| `ThumbyColor_BunnyMark_RISCV.uf2` | the Hazard3 RISC-V cores (UF2 family rp2350-riscv) |

Both at 150 MHz, with the same compiler options (`-Os`), so the frame rates compare the two
cores and their compilers. Switch the Thumby Color off, hold DOWN while switching it on, and copy
the `.uf2` onto the drive that appears. That replaces the firmware it came with; TinyCircuits'
own firmware goes back on the same way.

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| UP | +500 bunnies |
| DOWN | +1000 bunnies |
| A + B | back to 0 bunnies |
| MENU | the frame's depth: 16 → 8 → 1 → 16 bpp |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60 @ 16 bpp
bunnies 100
+--------------+
|   bunnies    |
+--------------+
```

The whole 128x128 display, 1:1, with the 16x16 bunny at half the Playdate's speed (the size every
port uses so their frame rates can be compared). **FPS** is counted over the last second,
**@ 16 bpp** is the frame's depth, **bunnies** the count (at most 10,000). The box and the text
shake while a button that adds bunnies is held.

## What it measures

Each frame: every bunny moves (12.4 fixed point), the frame in RAM is cleared and every bunny
written into it, then the frame goes to the display over SPI at 75 MHz (`spi_write_blocking`, its
own small GC9107 driver). The frame rate is how many of those fit in a second.

The frame's depth is the one setting to switch, while it runs:

| Depth | Frame | The bunny | Sending |
|---|---|---|---|
| 16 bpp | 32 KB, RGB565 in the display's byte order | runs of 16-bit writes | the whole frame in one transfer |
| 8 bpp | 16 KB, RGB332 | runs of `memset` | each row turned into RGB565 first |
| 1 bpp | 2 KB | a mask and pattern per row, a few bytes | each row expanded first |

A 16 bpp frame is 3.5 ms on the wire at 75 MHz, so the screen is not what holds the frame rate
back for long: with a few thousand bunnies the cores' own work is most of a frame, and that is
where the ARM and RISC-V builds differ.

The SPI clock is at most half the peripheral clock; the sketch runs that from the system clock
(150 MHz), as on the PicoSystem, so the SPI gets 75 MHz, within the GC9107's 80.

## Not yet run on a device

Both builds are made and their UF2s checked (family and addresses); neither has been run on a
Thumby Color yet. A test build prints which core it runs on, the clocks and the time spent per
frame over USB serial (`-DBUNNYMARK_SERIAL=1`; the release builds send nothing).

## Building

`tools/build_releases.py --only ThumbyColor` in the folder above (arduino-pico: Generic RP2350
with the RP2350A chip and 16 MB of flash, the board the *_embedded games' Thumby Color builds
use, with CPU Architecture ARM for one and RISC-V for the other). The display's pins and setup
are those games' (CircuitPython's board files and TinyCircuits' engine).

`BunnyGame.h` (the game), `BunnyFrame.h` (the frame), `bunny.h` and `font8x8_basic.h` are
copies of the ones in `common/` and `sdl/`, made by `tools/make_sprites.py`.
