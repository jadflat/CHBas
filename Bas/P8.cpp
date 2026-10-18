// The PICO-8 calls on gfx_fb (P8.h).
#include <CHGame.h>
#include "P8.h"
#include "src/assets/Cart.h"

namespace p8 {

static int camX, camY;
static uint8_t pen = 6;                         // PICO-8 starts with colour 6
static uint8_t drawPal[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static bool palPlain = true;                    // drawPal is the identity
static uint16_t trans = 1;                      // palt: bit c = colour c transparent
static uint8_t scrPal[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
// Two pixels at a time (a sheet byte): the draw palette, and 0xF over each
// transparent pixel. Made again when pal()/palt() change.
static uint8_t palByte[256], keepByte[256];
static bool palDirty = true, keepDirty = true;

static void tables() {
    if (palDirty) {
        for (int b = 0; b < 256; b++) palByte[b] = (uint8_t)(drawPal[b & 15] | (drawPal[b >> 4] << 4));
        palDirty = false;
    }
    if (keepDirty) {
        for (int b = 0; b < 256; b++)
            keepByte[b] = (uint8_t)((((trans >> (b & 15)) & 1) ? 0x0F : 0) | (((trans >> (b >> 4)) & 1) ? 0xF0 : 0));
        keepDirty = false;
    }
}
static bool scrChanged;

void camera(int x, int y) { camX = x; camY = y; }

static void checkPlain() {
    palDirty = true;
    palPlain = true;
    for (uint8_t i = 0; i < 16; i++)
        if (drawPal[i] != i) palPlain = false;
}

void pal() {
    for (uint8_t i = 0; i < 16; i++) {
        drawPal[i] = i;
        if (scrPal[i] != i) { scrPal[i] = i; scrChanged = true; }
    }
    palPlain = true;
    palDirty = true;
    if (trans != 1) { trans = 1; keepDirty = true; }
}

void pal(uint8_t c0, uint8_t c1) {
    drawPal[c0 & 15] = c1 & 15;
    checkPlain();
}

void palTable(const uint8_t tbl[16]) {
    for (uint8_t i = 0; i < 16; i++) drawPal[(i + 1) & 15] = tbl[i] & 15;
    checkPlain();
}

void palScreen(uint8_t c, uint8_t p) {
    c &= 15;
    if (scrPal[c] != p) { scrPal[c] = p; scrChanged = true; }
}

uint8_t screenPal(uint8_t c) { return scrPal[c & 15]; }

bool screenPalChanged() {
    bool r = scrChanged;
    scrChanged = false;
    return r;
}

void screenPalRGB(uint16_t out[16]) {
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t p = scrPal[i];
        out[i] = cart::PALETTE[p >= 128 ? 16 + (p & 15) : (p & 15)];
    }
}

void palt() {
    if (trans != 1) { trans = 1; keepDirty = true; }
}
void palt(uint8_t c, bool t) {
    uint16_t was = trans;
    if (t) trans |= (uint16_t)(1u << (c & 15));
    else trans &= (uint16_t)~(1u << (c & 15));
    if (trans != was) keepDirty = true;
}

void color(int c) { pen = (uint8_t)(c & 15); }

// A colour argument, or the pen; through the draw palette.
static inline uint8_t ink(int c) {
    if (c >= 0) pen = (uint8_t)(c & 15);
    return drawPal[pen];
}

static inline void plot(int x, int y, uint8_t v) {
    uint8_t *p = gfx_fb + y * GFX_FB_STRIDE + (x >> 1);
    *p = (x & 1) ? (uint8_t)((*p & 0x0F) | (v << 4)) : (uint8_t)((*p & 0xF0) | v);
}

void cls(uint8_t c) {
    c &= 15;
    memset(gfx_fb, c | (c << 4), GFX_FB_BYTES);
}

// ---- sprites ----------------------------------------------------------------
// The sheet's region (sx,sy,sw,sh) at (dx,dy), stretched to dw x dh as PICO-8
// does (source pixel = floor(i * sw / dw)); palt()'s colours left out.
RAMFUNC(p8blit) static void blit(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool fx, bool fy) {
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    dx -= camX;
    dy -= camY;
    if (dx >= 128 || dy >= 128 || dx + dw <= 0 || dy + dh <= 0) return;
    int c0 = dx < 0 ? -dx : 0, c1 = dx + dw > 128 ? 128 - dx : dw;
    int r0 = dy < 0 ? -dy : 0, r1 = dy + dh > 128 ? 128 - dy : dh;
    uint16_t tm = trans;
    for (int r = r0; r < r1; r++) {
        int ry = sw == dw && sh == dh ? r : (int)(((uint32_t)r * sh) / dh);
        int y = sy + (fy ? sh - 1 - ry : ry);
        if ((unsigned)y > 127) continue;
        const uint8_t *src = cart::SHEET + y * 64;
        uint8_t *d = gfx_fb + (dy + r) * GFX_FB_STRIDE;
        // floor(i * sw / dw), stepped exactly: a quotient and its remainder
        int rx = (int)((uint32_t)c0 * sw / dw), rem = (int)((uint32_t)c0 * sw % dw);
        for (int i = c0; i < c1; i++) {
            if (i > c0) {
                rem += sw;
                while (rem >= dw) { rem -= dw; rx++; }
            }
            int x = sx + (fx ? sw - 1 - rx : rx);
            if ((unsigned)x > 127) continue;
            uint8_t b = src[x >> 1];
            uint8_t c = (x & 1) ? (b >> 4) : (b & 15);
            if ((tm >> c) & 1) continue;
            int xx = dx + i;
            uint8_t *p = d + (xx >> 1);
            uint8_t v = drawPal[c];
            *p = (xx & 1) ? (uint8_t)((*p & 0x0F) | (v << 4)) : (uint8_t)((*p & 0xF0) | v);
        }
    }
}

// An 8x8 sprite wholly on screen across (rows r0..r1-1 of it on screen), a
// row (4 sheet bytes) at a time: mirrored if asked, through the draw
// palette, and merged with what is there under its transparent pixels.
RAMFUNC(p8tile) static void tile8(int n, int x, int y, int r0, int r1, bool fx, bool fy) {
    tables();
    const uint8_t *src = cart::SHEET + (n >> 4) * 8 * 64 + (n & 15) * 4;
    uint8_t *d = gfx_fb + (y + r0) * GFX_FB_STRIDE + (x >> 1);
    bool solid = !(cart::COLOURS[n] & trans);
    for (int r = r0; r < r1; r++, d += GFX_FB_STRIDE) {
        const uint8_t *s = src + (fy ? 7 - r : r) * 64;
        uint8_t b[4];
        if (fx) {
            for (int k = 0; k < 4; k++) { uint8_t v = s[3 - k]; b[k] = (uint8_t)((v >> 4) | (v << 4)); }
        } else {
            b[0] = s[0]; b[1] = s[1]; b[2] = s[2]; b[3] = s[3];
        }
        uint32_t keep = 0;
        if (!solid) {
            keep = keepByte[b[0]] | (keepByte[b[1]] << 8) | (keepByte[b[2]] << 16) | ((uint32_t)keepByte[b[3]] << 24);
            if (keep == 0xFFFFFFFFu) continue;
        }
        uint32_t w = palPlain ? (b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24))
                              : (palByte[b[0]] | (palByte[b[1]] << 8) | (palByte[b[2]] << 16) | ((uint32_t)palByte[b[3]] << 24));
        if (!(x & 1)) {
            if (!keep) {
                d[0] = (uint8_t)w; d[1] = (uint8_t)(w >> 8); d[2] = (uint8_t)(w >> 16); d[3] = (uint8_t)(w >> 24);
            } else {
                for (int k = 0; k < 4; k++) {
                    uint8_t mb = (uint8_t)(keep >> (8 * k));
                    d[k] = (uint8_t)((d[k] & mb) | ((uint8_t)(w >> (8 * k)) & ~mb));
                }
            }
        } else {
            // One nibble on: the first byte keeps its low half, a fifth its high half.
            uint32_t lo = (w << 4), loKeep = (keep << 4) | 0xF;
            uint8_t hi = (uint8_t)(w >> 28), hiKeep = (uint8_t)((keep >> 28) | 0xF0);
            for (int k = 0; k < 4; k++) {
                uint8_t mb = (uint8_t)(loKeep >> (8 * k));
                d[k] = (uint8_t)((d[k] & mb) | ((uint8_t)(lo >> (8 * k)) & ~mb));
            }
            d[4] = (uint8_t)((d[4] & hiKeep) | (hi & ~hiKeep));
        }
    }
}

// spr(n,x,y,w,h,fx,fy): a block of w x h sprites, each drawn by the fast
// path when it can be (PICO-8 mirrors the whole block).
void spr(int n, int x, int y, int w, int h, bool flipX, bool flipY) {
    if (n < 0 || n > 255 || w <= 0 || h <= 0) return;
    int sx = (n & 15) * 8, sy = (n >> 4) * 8;
    if (sx + w * 8 > 128 || sy + h * 8 > 128) {         // past the sheet's edge: as PICO-8, by the pixel
        blit(sx, sy, w * 8, h * 8, x, y, w * 8, h * 8, flipX, flipY);
        return;
    }
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int t = n + j * 16 + i;
            int tx = x + (flipX ? w - 1 - i : i) * 8 - camX;
            int ty = y + (flipY ? h - 1 - j : j) * 8 - camY;
            if (tx <= -8 || tx >= 128 || ty <= -8 || ty >= 128) continue;
            if (tx >= 0 && tx <= 120)
                tile8(t, tx, ty, ty < 0 ? -ty : 0, ty > 120 ? 128 - ty : 8, flipX, flipY);
            else
                blit((t & 15) * 8, (t >> 4) * 8, 8, 8, tx + camX, ty + camY, 8, 8, flipX, flipY);
        }
    }
}

void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool flipX, bool flipY) {
    blit(sx, sy, sw, sh, dx, dy, dw, dh, flipX, flipY);
}

// ---- shapes -----------------------------------------------------------------
RAMFUNC(p8rectfill) void rectfill(int x0, int y0, int x1, int y1, int c) {
    uint8_t v = ink(c);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    x0 -= camX; x1 -= camX; y0 -= camY; y1 -= camY;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 127) x1 = 127;
    if (y1 > 127) y1 = 127;
    if (x0 > x1 || y0 > y1) return;
    uint8_t both = v | (v << 4);
    for (int y = y0; y <= y1; y++) {
        uint8_t *d = gfx_fb + y * GFX_FB_STRIDE;
        int x = x0;
        if (x & 1) { d[x >> 1] = (d[x >> 1] & 0x0F) | (v << 4); x++; }
        int end = x1 + 1;                 // exclusive
        if (end & 1) { d[end >> 1] = (d[end >> 1] & 0xF0) | v; end--; }
        if (end > x) memset(d + (x >> 1), both, (end - x) >> 1);
    }
}

void rect(int x0, int y0, int x1, int y1, int c) {
    ink(c);
    rectfill(x0, y0, x1, y0);
    rectfill(x0, y1, x1, y1);
    rectfill(x0, y0, x0, y1);
    rectfill(x1, y0, x1, y1);
}

void pset(int x, int y, int c) {
    uint8_t v = ink(c);
    x -= camX;
    y -= camY;
    if ((unsigned)x > 127 || (unsigned)y > 127) return;
    plot(x, y, v);
}

void circfill(int x, int y, int r, int c) {
    uint8_t v = ink(c);
    if (r < 0) return;
    if (r == 0) { pset(x, y); return; }
    gfx_fillCircle(x - camX, y - camY, r, v);
}

void line(int x0, int y0, int x1, int y1, int c) {
    uint8_t v = ink(c);
    gfx_line(x0 - camX, y0 - camY, x1 - camX, y1 - camY, v);
}

// ---- text -------------------------------------------------------------------
// The cart's glyphs, 7x5, a byte a row (bit 6 = the left pixel): PICO-8's
// buttons, each a rounded key with its symbol cut out. PICO-8 advances 8
// pixels for them, 4 for the rest.
static const uint8_t GLYPHS[7][6] = {
    { (uint8_t)GL_LEFT,  0x3E, 0x67, 0x47, 0x67, 0x3E },
    { (uint8_t)GL_RIGHT, 0x3E, 0x73, 0x71, 0x73, 0x3E },
    { (uint8_t)GL_UP,    0x3E, 0x77, 0x63, 0x63, 0x3E },
    { (uint8_t)GL_DOWN,  0x3E, 0x63, 0x63, 0x77, 0x3E },
    { (uint8_t)GL_O,     0x3E, 0x63, 0x6B, 0x63, 0x3E },
    { (uint8_t)GL_X,     0x3E, 0x6B, 0x77, 0x6B, 0x3E },
    { (uint8_t)GL_HEART, 0x36, 0x7F, 0x7F, 0x3E, 0x08 },
};

static const uint8_t *glyph(char ch) {
    for (uint8_t k = 0; k < 7; k++)
        if ((char)GLYPHS[k][0] == ch) return GLYPHS[k] + 1;
    return nullptr;
}

void print(const char *s, int x, int y, int c) {
    uint8_t v = ink(c);
    x -= camX;
    y -= camY;
    char one[2] = { 0, 0 };
    for (; *s; s++) {
        char ch = *s;
        if (const uint8_t *g = glyph(ch)) {
            for (int r = 0; r < 5; r++)
                for (int i = 0; i < 7; i++)
                    if ((g[r] >> (6 - i)) & 1) {
                        int xx = x + i, yy = y + r;
                        if ((unsigned)xx < 128 && (unsigned)yy < 128) plot(xx, yy, v);
                    }
            x += 8;
            continue;
        }
        one[0] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
        if (ch != ' ' && x > -4 && x < 128 && y > -6 && y < 128) text35(x, y, one, v);
        x += 4;
    }
}

void printNum(int v, int x, int y, int c) {
    char buf[12], tmp[12];
    int n = 0, k = 0;
    bool neg = v < 0;
    unsigned u = neg ? (unsigned)-v : (unsigned)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) buf[k++] = '-';
    while (n) buf[k++] = tmp[--n];
    buf[k] = 0;
    print(buf, x, y, c);
}

// Each letter's cell (1 pixel of border left and above, the line's
// spacing below) filled, the letter's pixels left as they were.
void printInverted(const char *s, int x, int y, int c) {
    uint8_t v = ink(c);
    x -= camX;
    y -= camY;
    for (; *s; s++, x += 4) {
        char ch = (*s >= 'a' && *s <= 'z') ? (char)(*s - 32) : *s;
        const uint8_t *g = ::glyph35(ch);     // CHGame's 3x5 font: 3 columns, bit 0 the top row
        for (int yy = -1; yy <= 5; yy++)
            for (int xx = -1; xx <= 3; xx++) {
                bool on = g && xx >= 0 && xx < 3 && yy >= 0 && yy < 5 && ((g[xx] >> yy) & 1);
                int px = x + xx, py = y + yy;
                if (!on && (unsigned)px < 128 && (unsigned)py < 128) plot(px, py, v);
            }
    }
}

}  // namespace p8
