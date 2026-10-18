// The port's side of the golden traces (tools/ref/compare.py): Game.cpp on
// the PC with nothing drawn, fed a file of buttons, printing its state after
// every frame in the format compare.py's Lua prints the cart's.
//
//   trace <inputs>
//
// <inputs>: one byte a frame (the cart's btn() bits), from boot.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Game.h"
#include "P8.h"
#include "Sounds.h"
#include "CartData.h"

// Nothing is drawn; the screen palette is kept (the splash peeks it).
namespace p8 {
static uint8_t scr[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
void camera(int, int) {}
void pal() { for (int i = 0; i < 16; i++) scr[i] = (uint8_t)i; }
void pal(uint8_t, uint8_t) {}
void palTable(const uint8_t *) {}
void palScreen(uint8_t c, uint8_t p) { scr[c & 15] = p; }
uint8_t screenPal(uint8_t c) { return scr[c & 15]; }
void palt() {}
void palt(uint8_t, bool) {}
void color(int) {}
void cls(uint8_t) {}
void spr(int, int, int, int, int, bool, bool) {}
void sspr(int, int, int, int, int, int, int, int, bool, bool) {}
void rectfill(int, int, int, int, int) {}
void rect(int, int, int, int, int) {}
void circfill(int, int, int, int) {}
void line(int, int, int, int, int) {}
void pset(int, int, int) {}
void print(const char *, int, int, int) {}
void printNum(int, int, int, int) {}
void printInverted(const char *, int, int, int) {}
bool screenPalChanged() { return false; }
void screenPalRGB(uint16_t *) {}
}  // namespace p8

static char events[512];
static void event(const char *s) {
    size_t n = strlen(events);
    snprintf(events + n, sizeof events - n, "%s", s);
}

namespace snd {
void begin() {}
void sfx(int n, int) { char b[16]; snprintf(b, sizeof b, " sfx%d", n); event(b); }
void music(int n, int) { char b[16]; snprintf(b, sizeof b, " mus%d", n); event(b); }
}  // namespace snd

namespace cdata {
static num slot[64];
void begin() {}
num dget(int i) { return (unsigned)i < 64 ? slot[i] : num(0); }
void dset(int i, num v) { if ((unsigned)i < 64) slot[i] = v; }
void flush() {}
}  // namespace cdata

static double d(num v) {
#if BAS_NUM_DOUBLE
    return v;
#else
    return v.v / 65536.0;
#endif
}

static void put(double v) {
    if (v == 0) v = 0;       // no -0
    printf(" %.17g", v);
}
static void put(int v) { printf(" %d", v); }
static void putn(num v) { put(d(v)); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: trace <inputs>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static uint8_t in[1 << 20];
    size_t frames = fread(in, 1, sizeof in, f);
    fclose(f);

    rndSeed(1);
    game::boot();
    for (size_t i = 0; i < frames; i++) {
        events[0] = 0;
        game::buttons = in[i];
        game::update();
        game::draw();
        game::State s = game::state();
        printf("%zu%s |", i, events);
        put(s.scene); put(s.splashCount); put(s.splashFading);
        if (s.scene == 1) {
            put(s.state); put(s.cooldown); put(s.transitionScene);
            putn(s.playerX); putn(s.playerY); putn(s.jumpTilePos); putn(s.camY); putn(s.trapY); putn(s.trapOffset);
            put(s.tilePos); put(s.jumping); put(s.rightWall); put(s.playerSprite); put(s.score);
            put(s.sawCount); put(s.sawLefts); put(s.jfxCount); put(s.particles); put(s.gameOverAnim);
            put(s.transitionAnim); put(s.initialGone); put(s.goHighscore); put(s.best); put(s.jfxSprites);
        } else {
            for (int k = 0; k < 24; k++) put(0);
        }
        printf("\n");
    }
    return 0;
}
