// BunnyMark's frame for the devices that keep the whole screen in RAM and send it to their
// display every frame (PyBadge, PyGamer, PicoSystem, Explorer 2350, Tufty 2350).
//
// The frame's depth can change while it runs (bunnyFrameSetDepth):
//   16 bpp  RGB565, two bytes a pixel, high byte first: the display's own order, sent as it is
//   8 bpp   RGB332, a byte a pixel, turned into RGB565 on the way to the display
//   1 bpp   a bit a pixel, most significant bit first, set is white, expanded on the way out
// BunnyMark only draws black and white, which are 0x0000 / 0xFFFF, 0x00 / 0xFF and 0 / 1.
//
// The canonical copy is common/BunnyFrame.h; tools/make_sprites.py copies it into each sketch
// (an Arduino sketch only compiles what is in its own folder).

#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bunny.h"
#include "font8x8_basic.h"

struct BunnyFrame
{
    int width, height;
    uint8_t depth;
    int rowBytes;
    uint8_t *pixels;
};

// RGB332 as RGB565, high byte first, the way LovyanGFX converts it
static uint8_t bunnyPalette332[256 * 2];

static inline int bunnyRowBytes(int width, uint8_t bits)
{
    if (bits == 16) return width * 2;
    if (bits == 8) return width;
    return (width + 7) / 8;
}

static void bunnyFrameInit(BunnyFrame &f, int width, int height)
{
    f.width = width;
    f.height = height;
    f.depth = 0;
    f.rowBytes = 0;
    f.pixels = nullptr;
    for (int i = 0; i < 256; i++)
    {
        const uint8_t r3 = i >> 5, g3 = (i >> 2) & 7, b2 = i & 3;
        const uint16_t c = (uint16_t)((((r3 * 9) >> 1) << 11) | ((g3 * 9) << 5) | ((b2 * 0x55) >> 3));
        bunnyPalette332[i * 2] = (uint8_t)(c >> 8);
        bunnyPalette332[i * 2 + 1] = (uint8_t)c;
    }
}

// A frame of the given depth (16, 8 or 1) in place of the one there is. The old one is freed
// first, so a deeper frame has the room the shallower one had. False, and the frame as it was,
// when there is not enough memory for it
static bool bunnyFrameSetDepth(BunnyFrame &f, uint8_t bits)
{
    const uint8_t old = f.depth;
    free(f.pixels);
    f.pixels = (uint8_t *)malloc((size_t)bunnyRowBytes(f.width, bits) * f.height);
    if (f.pixels)
    {
        f.depth = bits;
    }
    else if (old)
    {
        f.pixels = (uint8_t *)malloc((size_t)bunnyRowBytes(f.width, old) * f.height);
        f.depth = f.pixels ? old : 0;
    }
    else
    {
        f.depth = 0;
    }
    f.rowBytes = f.depth ? bunnyRowBytes(f.width, f.depth) : 0;
    return f.depth == bits;
}

// the whole frame white or black: the same byte in every depth
static void bunnyFrameFill(BunnyFrame &f, bool white)
{
    memset(f.pixels, white ? 0xFF : 0x00, (size_t)f.rowBytes * f.height);
}

static inline void bunnyFramePixel(BunnyFrame &f, int x, int y, bool white)
{
    if (x < 0 || y < 0 || x >= f.width || y >= f.height)
        return;
    uint8_t *row = f.pixels + y * f.rowBytes;
    if (f.depth == 16)
    {
        row[x * 2] = row[x * 2 + 1] = white ? 0xFF : 0x00;
    }
    else if (f.depth == 8)
    {
        row[x] = white ? 0xFF : 0x00;
    }
    else
    {
        const uint8_t bit = (uint8_t)(0x80 >> (x & 7));
        if (white) row[x >> 3] |= bit;
        else row[x >> 3] &= (uint8_t)~bit;
    }
}

// the box's lines and the text are a few hundred pixels a frame: pixel by pixel is plenty
static void bunnyFrameRect(BunnyFrame &f, int x, int y, int w, int h, bool white)
{
    for (int i = 0; i < w; i++)
    {
        bunnyFramePixel(f, x + i, y, white);
        bunnyFramePixel(f, x + i, y + h - 1, white);
    }
    for (int i = 0; i < h; i++)
    {
        bunnyFramePixel(f, x, y + i, white);
        bunnyFramePixel(f, x + w - 1, y + i, white);
    }
}

// 8x8 characters, only the set pixels drawn
static void bunnyFrameText(BunnyFrame &f, int x, int y, const char *s, bool white)
{
    for (; *s; s++, x += 8)
    {
        const unsigned char *glyph = font8x8_basic[(unsigned char)*s & 127];
        for (int gy = 0; gy < 8; gy++)
            for (int gx = 0; gx < 8; gx++)
                if ((glyph[gy] >> gx) & 1)
                    bunnyFramePixel(f, x + gx, y + gy, white);
    }
}

// A bunny with its top left at (x0, y0), 16 or 32 pixels big, written into the frame in the
// form made for its depth: runs of white and black for 16 and 8 bpp, a row mask and pattern
// shifted into place for 1 bpp. A bunny can be a few pixels past the box when it turns, so it is
// kept on the screen here rather than clipped pixel by pixel
template <int SIZE>
static void bunnyFrameBunny(BunnyFrame &f, int x0, int y0, const uint16_t *rows, const BunnyRun *runs,
                            const void *masks, const void *whites)
{
    if (x0 < 0) x0 = 0;
    if (x0 > f.width - SIZE) x0 = f.width - SIZE;
    if (y0 < 0) y0 = 0;
    if (y0 > f.height - SIZE) y0 = f.height - SIZE;
    if (f.depth == 16)
    {
        uint16_t *row = (uint16_t *)(void *)(f.pixels + y0 * f.rowBytes) + x0;
        const int stride = f.rowBytes / 2;
        for (int y = 0; y < SIZE; y++, row += stride)
            for (int r = rows[y]; r < rows[y + 1]; r++)
            {
                uint16_t *p = row + runs[r].x;
                const uint16_t c = runs[r].white ? 0xFFFF : 0x0000;
                for (uint8_t n = runs[r].len; n; n--)
                    *p++ = c;
            }
    }
    else if (f.depth == 8)
    {
        uint8_t *row = f.pixels + y0 * f.rowBytes + x0;
        for (int y = 0; y < SIZE; y++, row += f.rowBytes)
            for (int r = rows[y]; r < rows[y + 1]; r++)
                memset(row + runs[r].x, runs[r].white ? 0xFF : 0x00, runs[r].len);
    }
    else
    {
        // a row of the bunny lands in SIZE / 8 + 1 bytes; a byte the shifted mask does not
        // reach is left alone, which keeps a bunny at the right edge inside its row
        const int shift = 8 - (x0 & 7), bytes = SIZE / 8 + 1;
        uint8_t *row = f.pixels + y0 * f.rowBytes + (x0 >> 3);
        for (int y = 0; y < SIZE; y++, row += f.rowBytes)
        {
            uint64_t m, w;
            if (SIZE == 16)
            {
                m = (uint64_t)((const uint16_t *)masks)[y] << shift;
                w = (uint64_t)((const uint16_t *)whites)[y] << shift;
            }
            else
            {
                m = (uint64_t)((const uint32_t *)masks)[y] << shift;
                w = (uint64_t)((const uint32_t *)whites)[y] << shift;
            }
            for (int i = 0; i < bytes; i++)
            {
                const int s = (bytes - 1 - i) * 8;
                const uint8_t mb = (uint8_t)(m >> s);
                if (mb)
                    row[i] = (uint8_t)((row[i] & ~mb) | (uint8_t)(w >> s));
            }
        }
    }
}

// Row y as the display takes it: RGB565, high byte first, width * 2 bytes into out. A 16 bpp
// frame already is that; the caller can send its rows straight from f.pixels instead
static void bunnyFrameRowToDisplay(const BunnyFrame &f, int y, uint8_t *out)
{
    const uint8_t *row = f.pixels + y * f.rowBytes;
    if (f.depth == 16)
    {
        memcpy(out, row, (size_t)f.width * 2);
    }
    else if (f.depth == 8)
    {
        for (int x = 0; x < f.width; x++)
        {
            const uint8_t *c = &bunnyPalette332[row[x] * 2];
            *out++ = c[0];
            *out++ = c[1];
        }
    }
    else
    {
        for (int x = 0; x < f.width; x += 8)
        {
            uint8_t bits = *row++;
            for (int b = 0; b < 8 && x + b < f.width; b++, bits <<= 1)
            {
                const uint8_t v = (bits & 0x80) ? 0xFF : 0x00;
                *out++ = v;
                *out++ = v;
            }
        }
    }
}
