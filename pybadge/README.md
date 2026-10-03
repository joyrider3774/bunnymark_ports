# BunnyMark for the Adafruit PyBadge and PyGamer

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

One sketch for both boards (SAMD51 at 120 MHz, ST7735 160x128): `PyBadge_BunnyMark.uf2`
and `PyGamer_BunnyMark.uf2`. Double press reset and copy the `.uf2` onto the drive that
appears.

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| UP (the PyGamer's joystick up) | +500 bunnies |
| DOWN (joystick down) | +1000 bunnies |
| A + B | back to 0 bunnies |
| SELECT | the frame's depth: 16 → 8 → 1 → 16 bpp |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60 @ 16 bpp
bunnies 100
+------------------+
| bunnies bouncing |
+------------------+
```

The whole 160x128 display, 1:1, with the 16x16 bunny at half the Playdate's speed (as on
the CHGame, the screen being about as small). **FPS** is counted over the last second,
**@ 16 bpp** is the frame's depth, **bunnies** the count (at most 10,000). The box and the
text shake while a button that adds bunnies is held.

## What it measures

Each frame: every bunny moves (12.4 fixed point; the SAMD51's FPU is single precision but
one code for all these devices is the point), the frame in RAM is cleared and every bunny
written into it, then the frame goes to the display over SPI with Adafruit_ST7735
(`writePixels`). The frame rate is how many of those fit in a second.

The frame's depth is the one setting to switch, while it runs:

| Depth | Frame | The bunny | Sending |
|---|---|---|---|
| 16 bpp | 40 KB, RGB565 in the display's byte order | runs of 16-bit writes | the whole frame in one transfer |
| 8 bpp | 20 KB, RGB332 | runs of `memset` | each row turned into RGB565 first |
| 1 bpp | 2.5 KB | a mask and pattern per row, a few bytes | each row expanded first |

The display always takes 16-bit pixels, so 8 and 1 bpp draw faster but pay for it when
sending.

A 160x128 frame is 40,960 bytes. The SPI clock is at most half its SERCOM's clock, 48 MHz
by default, so the library's 32 MHz became 24 MHz: 13.7 ms a frame and 61 fps with no
bunnies. The sketch moves SPI1 to the core's 100 MHz clock and runs the display at 50 MHz,
which takes 6.6 ms.

## Tested on a PyGamer

Measured with a test build that prints the time spent per frame over USB serial
(`-DBUNNYMARK_SERIAL=1`; the release build sends nothing), 16 bpp:

| Bunnies | FPS |
|---|---|
| 0 | 105 |
| 101 | ~90 |
| 201 | 77 |

The PyBadge is the same chip and display; not run on one.

## Building

`tools/build_releases.py --only PyBadge PyGamer` in the folder above: the Adafruit SAMD
core (1.7.16, with the arm-none-eabi-gcc 9-2019q4 it asks for), Adafruit GFX, ST7735 and
BusIO, and the UF2 made from the `.bin` (at 0x4000 for their bootloader). By hand: the
"Adafruit PyBadge M4 Express" or "PyGamer M4 Express" board.

`BunnyGame.h` (the game), `BunnyFrame.h` (the frame), `bunny.h` and `font8x8_basic.h` are
copies of the ones in `common/` and `sdl/`, made by `tools/make_sprites.py`.
