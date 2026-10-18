// BAS (yokoboko's PICO-8 game), ported from the cart's Lua (assets/bas.p8).
//
// Game.cpp follows the Lua class by class, in its order, with its names:
// read the two side by side. What differs is only what C++ needs:
// - the classes (setmetatable/extend) are structs; animatable's named
//   animations are a fixed table per class;
// - the tables keyed "t"..i (the visible tiles) are small arrays searched by
//   index; nothing depends on pairs()' order (the tiles don't overlap);
// - numbers are PICO-8's (Fix.h), including its wrap past 32767, which the
//   cart's "overflow fix" and its first frames' drawing rely on.
//
// Game.cpp knows nothing of CHGame: it draws through P8.h, plays through
// Sounds.h, keeps its best score through CartData.h and reads buttons from
// game::buttons, so the same file runs on the board, in the simulator and in
// tools/ref/'s trace harness.
#pragma once
#include <stdint.h>
#include "Fix.h"

namespace game {

// btn(0) ... btn(5): left, right, up, down, O, X; as bits of game::buttons.
// btnp() is a press this frame (the cart turns PICO-8's repeat off).
extern uint8_t buttons;

void boot();        // the cart's top level (poke, cartdata), then _init()
void update();      // _update60()
void draw();        // _draw()

// What the traces compare (tools/ref/).
struct State {
    int scene;                      // 0 splash, 1 game
    int splashCount, splashFading;
    int state, cooldown, transitionScene;
    num playerX, playerY, jumpTilePos, camY, trapY, trapOffset;
    int tilePos, jumping, rightWall, playerSprite, score, sawCount, sawLefts, jfxCount;
    int particles, gameOverAnim, transitionAnim, initialGone;
    int goHighscore, best, jfxSprites;      // the game over's "new highscore"; dget(0); the jump effects' sprites (nil: -1)
};
State state();

}  // namespace game
