# BunnyMark for the Pimoroni Explorer 2350 and Tufty 2350

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

One sketch for both (RP2350 at 150 MHz, ST7789 320x240 on an 8-bit parallel bus), built
with the arduino-pico core: `Explorer_BunnyMark.uf2` and `Tufty_BunnyMark.uf2`.
Explorer: hold BOOT while pressing RESET; Tufty: hold HOME while pressing RESET; then copy
the `.uf2` onto the drive that appears.

## Buttons

| | Explorer 2350 | Tufty 2350 |
|---|---|---|
| +1 bunny | A | A |
| +100 bunnies | B | B |
| +500 bunnies | C | UP |
| +1000 bunnies | X | DOWN |
| back to 0 bunnies | A + B | A + B |
| the frame's depth, 16 → 8 → 1 bpp | Y | C |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60 @ 16 bpp
bunnies 100
+--------------------------------+
|        bunnies bouncing        |
+--------------------------------+
```

The whole 320x240 display, 1:1, with the 16x16 bunny at half the Playdate's speed (the size every port uses so their frame rates can be compared). **FPS** is
counted over the last second, **@ 16 bpp** is the frame's depth, **bunnies** the count (at
most 10,000). The box and the text shake while a button that adds bunnies is held.

## What it measures

Each frame: every bunny moves (12.4 fixed point), the frame in RAM is cleared and every
bunny written into it, then the frame goes to the display over the parallel bus: a PIO
program puts each byte on the data pins and strobes it in, fed by DMA. The frame rate is
how many of those fit in a second.

The frame's depth is the one setting to switch, while it runs:

| Depth | Frame | The bunny | Sending |
|---|---|---|---|
| 16 bpp | 150 KB, RGB565 in the display's byte order | runs of 16-bit writes | the whole frame as one DMA transfer |
| 8 bpp | 75 KB, RGB332 | runs of `memset` | each row turned into RGB565, the next row converted while the last one goes out |
| 1 bpp | 9.4 KB | a mask and pattern per row, a few bytes | each row expanded, the same way |

## The display bus

The display's data pins are GPIO 32-39, which a PIO block only reaches once its GPIO base is
moved to 16. The PIO program is three steps a byte (`out pins, 8 side 1` / `nop side 0` /
`nop side 1`), so the write strobe is high whenever it waits: a program that waits with it
low gives the display one more write of the last byte when chip select goes high, and the
picture creeps a pixel a frame. The PIO runs at 150 / 4 = 37.5 MHz, 80 ns a byte with 27 ns
strobe phases: Pimoroni's own driver stops at 32 MHz (150 / 5 here), but the ST7789 only needs
a 66 ns write cycle with phases of 15 ns. The Tufty's panel is mounted the other way round
(MADCTL 0xA0 against the Explorer's 0x60), and it is powered by a switched rail: GPIO 41 has to
be set high first, or the display stays black.

On the Tufty, `TuftyPower.h` does the power management the *_embedded games' Tufty platform does
(Pimoroni's Badgeware firmware's): at every start the pins are set up (GPIO 41 among them) and
the RTC's timer interrupt is switched off, which would otherwise keep waking it. RESET held after
the reset sweeps the rear LEDs on and out; held until they are dark the Tufty goes to sleep (a
front button wakes it), with UP and DOWN held as well into shipping mode (only RESET or USB power
bring it back). A second tap of RESET within a second counts as a double tap.

## Tested on an Explorer and a Tufty

Measured with a test build that prints the time spent per frame over USB serial
(`-DBUNNYMARK_SERIAL=1`; the release build sends nothing), 16 bpp, no bunnies: the game part
1 ms, the send 12.3 ms (153,600 bytes at 12.5 MB/s), 76 fps, on the Tufty. With the PIO at
Pimoroni's 30 MHz the send was 15.4 ms and the frame rate 62, on both boards.

## Building

`tools/build_releases.py --only Explorer Tufty` in the folder above (arduino-pico: the
Pimoroni Explorer board, and Generic RP2350 with the RP2350B chip and 8 MB PSRAM on GPIO 8
for the Tufty, with the board options of the *_embedded games' release tool).

`BunnyGame.h` (the game), `BunnyFrame.h` (the frame), `bunny.h` and `font8x8_basic.h` are
copies of the ones in `common/` and `sdl/`, made by `tools/make_sprites.py`.
