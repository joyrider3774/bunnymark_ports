# BunnyMark for the Arduboy

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

The release files are `Arduboy_BunnyMark.hex`, ready for [Ardens](https://github.com/tiberiusbrown/Ardens)
or for uploading, and `Arduboy_BunnyMark.bin`, the same program as a plain image from
address 0. The sketch is `BunnyMark/BunnyMark.ino`.

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| UP | +10 bunnies |
| B | +100 bunnies |
| A + B | back to 0 bunnies |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing. The Arduboy2 start-up options work as in any Arduboy game
(UP held at power-on for the flashlight, B for the system settings).

The build leaves out the USB code (`ARDUBOY_NO_USB`) to free its RAM for bunnies, so
the Arduboy does not show up as a USB device while BunnyMark runs: to upload something
else, hold DOWN while switching it on, which starts the bootloader.

## The screen

```
FPS 108  bunnies 100
+----------------------+
|   bunnies bouncing   |
+----------------------+
```

- **FPS**: frames per second, counted over the last second.
- **bunnies**: how many there are, at most 390.

The box and the text shake while A, B or UP is held, as on the Playdate. The
display is shown inverted (black on white, like the Playdate's), which the display
does by itself, so it costs nothing.

## What it measures

Each frame does three things, one after the other, and the FPS is how many of these
frames fit in a second:

1. **Move**: every bunny moves by its speed and turns around at the box's edges, in
   quarter pixels: a few 16-bit additions and compares per bunny on an 8-bit CPU.
2. **Draw**: the 1 KB screen buffer is cleared and every bunny is drawn with
   Arduboy2's `Sprites::drawPlusMask` (a 16x16 image with a mask), which has to shift
   the bunny's bits into place when it is not on a multiple of 8 rows. This is what
   grows with the bunny count.
3. **Send**: the buffer goes to the display over SPI (`arduboy.display()`), which
   costs the same every frame.

There is no frame rate cap (as on the Playdate), so with no bunnies the frame rate
shows the cost of clearing and sending alone.

In Ardens (v0.25.11, headless; see below):

| Bunnies | FPS |
|---|---|
| 0 | ~199 |
| 10 | ~160 |
| 100 | ~65 |
| 150 | ~50 |
| 390 | ~23 |

These are Ardens' figures, not measured on a device.

## Differences from the Playdate version

- The screen is 128x64 and one bit per pixel. The bunny is 16x16, the Playdate's image
  at half size, and moves at half its speed: the size every port uses so their frame
  rates can be compared. On this screen it is big, nearly a third of the box's height.
- Positions are integers in quarter pixels: the ATmega32U4 has no FPU, and floats on
  it are slow.
- At most 390 bunnies instead of 50,000: the Arduboy has 2.5 KB of RAM, and the
  screen buffer takes 1 KB. To fit that many, a bunny is 3 bytes: x (9 bits) and y
  (8 bits) in quarter pixels, and each axis' speed as a 3-bit index into a table of
  eight (-10 to +10 quarter pixels a frame; the table is symmetric, so bouncing is
  `7 - index`). A bunny that passes the box's edge is put back on it, as the 8 and 9
  bit positions would otherwise wrap. 390 bunnies and no USB code leave about 300
  bytes for the stack.
- Eight speeds per axis give only 64 different paths, so bunnies added together would
  lie exactly on top of each other: each new bunny starts a random 0-63 frames along
  its path instead.
- Only +1, +10 and +100: with room for 390 there is nothing for +500 or +1000 to do.

## Building

`tools/build_releases.py --only Arduboy` in the folder above builds it with arduino-cli,
the Arduboy2 library and the "Arduino Leonardo" board (the Arduboy is one), and makes the
`.bin` from the `.hex`. By hand:

```
arduino-cli compile --fqbn arduino:avr:leonardo BunnyMark
```

`bunny.h` comes from `tools/make_sprites.py` in the folder above.

## Testing in Ardens without a window

Ardens' headless mode runs a program for a number of simulated milliseconds and
prints what the program writes to the USB data register. It has no screenshots,
and in it every button reads as held. So there is a test build:

```
arduino-cli compile --fqbn arduino:avr:leonardo --build-path build \
    --build-property "compiler.cpp.extra_flags=-DBUNNYMARK_TEST=100 -DBUNNYMARK_DUMP_MS=6000" BunnyMark
Ardens --headless 8000 --file build/BunnyMark.ino.hex
```

`BUNNYMARK_TEST=N` starts with N bunnies, uses `boot()` instead of `begin()` (no
logo or start-up options, which wait for buttons), ignores the buttons, prints
`fps F bunnies N` every second and, at `BUNNYMARK_DUMP_MS`, the 1 KB screen buffer in
hex between `SCREEN` and `END`: 8 pages of 128 columns, bit 0 the top row of a page.
