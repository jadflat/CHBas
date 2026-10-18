// The PICO-8 calls BAS makes, drawn straight into CHGfx's framebuffer.
//
// PICO-8's screen is this one: 128x128, 16 colours, 4 bits a pixel. The
// cart's sheet is stored packed the way gfx_fb is (the left pixel in the low
// nibble). Like PICO-8:
// - everything is offset by camera();
// - every colour goes through the draw palette pal(); the screen palette
//   pal(c, p, 1) recolours the whole picture as it goes to the panel;
// - spr() and sspr() leave out the colours palt() makes transparent (0 by
//   default), tested before pal();
// - a call without a colour (PEN) uses the last colour given;
// - coordinates are whole pixels (the caller floors: toi()).
#pragma once
#include <stdint.h>

namespace p8 {

const int PEN = -1;                             // "no colour argument": the current one

void camera(int x = 0, int y = 0);
// pal(): the draw palette, transparency and screen palette back to PICO-8's.
void pal();
void pal(uint8_t c0, uint8_t c1);               // draw c0 as c1
// pal(tbl, 0): tbl[0] is colour 1 ... tbl[14] colour 15, tbl[15] colour 0
// (the cart's 1-based tables; PICO-8 takes colour 16 as 0).
void palTable(const uint8_t tbl[16]);
void palScreen(uint8_t c, uint8_t p);           // pal(c, p, 1): p 0-15 or 128-143
uint8_t screenPal(uint8_t c);                   // peek(0x5f10 + c)
void palt();                                    // only colour 0 transparent
void palt(uint8_t c, bool transparent);
void color(int c);

void cls(uint8_t c = 0);
void spr(int n, int x, int y, int w = 1, int h = 1, bool flipX = false, bool flipY = false);
void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool flipX = false, bool flipY = false);
void rectfill(int x0, int y0, int x1, int y1, int c = PEN);     // inclusive corners, either order
void rect(int x0, int y0, int x1, int y1, int c = PEN);
void circfill(int x, int y, int r, int c = PEN);
void line(int x0, int y0, int x1, int y1, int c = PEN);
void pset(int x, int y, int c = PEN);
// PICO-8's text: lower case as the 3x5 capitals; the cart's glyphs as
// GL_LEFT ... GL_X bytes, 8 pixels wide.
void print(const char *s, int x, int y, int c = PEN);
void printNum(int v, int x, int y, int c = PEN);
// print("\^i...") : inverted, PICO-8's default border on: a box in the
// colour with the letters cut out of it.
void printInverted(const char *s, int x, int y, int c = PEN);

enum : char { GL_DOWN = (char)0x83, GL_HEART = (char)0x87, GL_LEFT = (char)0x8b, GL_O = (char)0x8e,
              GL_RIGHT = (char)0x91, GL_UP = (char)0x94, GL_X = (char)0x97 };

// The screen palette as RGB565 for the panel (Bas.ino hands it to CHGfx when it changes).
bool screenPalChanged();
void screenPalRGB(uint16_t out[16]);

}  // namespace p8
