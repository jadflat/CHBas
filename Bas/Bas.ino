// BAS for CHGame: yokoboko's PICO-8 game.
//
// The cart's Lua rewritten in C++ (Game.cpp) with the cart's own sprites and
// sound (tools/p8extract.py, tools/p8audio.py). PICO-8's screen is this one,
// 128x128 in 16 colours, and the game runs at the cart's 60 frames a second
// (_update60).
//
// The frame: the buttons, the cart's _update60() (while the last frame goes
// out to the panel by DMA), then gfx_wait() and the cart's _draw() into the
// framebuffer, then gfx_flushAsync(). The splash's fade changes PICO-8's
// screen palette: it goes to the panel's palette here.
//
// The buttons: LEFT or A (PICO-8's O) jumps to the left wall, RIGHT or B
// (X) to the right (BAS_SWAP_AB; 0: B left, A right). START is PICO-8's
// pause button (BAS_PAUSE). START held 3 s goes back to the CHGame menu, as
// in every CHGame game.
#include <CHGame.h>
#include "config.h"
#include "Game.h"
#include "P8.h"
#include "Sounds.h"
#include "CartData.h"
#include "src/assets/Cart.h"

// The CHGame's buttons as the cart's (btn(0) ... btn(5)).
static uint8_t cartButtons() {
    uint8_t b = 0;
    if (chgame.pressed(LEFT_BUTTON)) b |= 1 << 0;
    if (chgame.pressed(RIGHT_BUTTON)) b |= 1 << 1;
    if (chgame.pressed(UP_BUTTON)) b |= 1 << 2;
    if (chgame.pressed(DOWN_BUTTON)) b |= 1 << 3;
#if BAS_SWAP_AB
    if (chgame.pressed(A_BUTTON)) b |= 1 << 4;      // O
    if (chgame.pressed(B_BUTTON)) b |= 1 << 5;      // X
#else
    if (chgame.pressed(B_BUTTON)) b |= 1 << 4;      // O
    if (chgame.pressed(A_BUTTON)) b |= 1 << 5;      // X
#endif
    return b;
}

// START held 3 s: back to the CHGame menu, timed in real time (the
// library counts polls; this keeps it 3 s whatever the frame rate).
static uint32_t startDown;          // millis() when START went down; 0 = not held
static void startHold() {
    if (chgame.pressed(START_BUTTON)) {
        uint32_t now = millis();
        if (!startDown) startDown = now ? now : 1;
        else if (now - startDown >= 3000) chgame.exitToMenu();
    } else {
        startDown = 0;
    }
}

// PICO-8's screen palette to the panel, when the cart has changed it.
static void screenPalette() {
    if (!p8::screenPalChanged()) return;
    uint16_t rgb[16];
    p8::screenPalRGB(rgb);
    gfx_setPalette(rgb, 16);
}

#if BAS_PAUSE
static bool paused;
static uint8_t choice;
static uint8_t holdOff;             // the menu closed: the game sees no button until all are let go

// PICO-8's pause menu: CONTINUE, and EXIT TO MENU in place of RESET CART.
static void drawPause() {
    static const char *const names[2] = { "continue", "exit to menu" };
    p8::camera();
    p8::rectfill(24, 50, 103, 77, 0);
    p8::rect(24, 50, 103, 77, 7);
    for (uint8_t i = 0; i < 2; i++) {
        int y = 57 + i * 8;
        if (i == choice) p8::print(">", 30, y, 7);
        p8::print(names[i], 38, y, i == choice ? 7 : 6);
    }
}

// The menu's buttons; false when it has closed.
static bool pauseInput() {
    if (chgame.justPressed(START_BUTTON)) {
        holdOff = 1;
        return false;
    }
    if (chgame.justPressed(UP_BUTTON) || chgame.justPressed(DOWN_BUTTON)) choice ^= 1;
    if (chgame.justPressed(A_BUTTON) || chgame.justPressed(B_BUTTON)) {
        if (choice == 1) chgame.exitToMenu();
        holdOff = 1;
        return false;
    }
    return true;
}
#endif

void setup() {
    chgame.boot();
    chgame.startExits = false;          // timed in real time: startHold()
    dbg::begin("CBAS " BAS_VERSION);
    gfx_begin(GFX_DIV2, GFX_12BPP);
    gfx_setPalette(cart::PALETTE, 16);
    snd::begin();
    chgame.setFrameRate(60);
    game::boot();
}

void loop() {
    dbg::poll();
    if (!chgame.nextFrame()) return;

    dbg::markUpdateStart();
    chgame.pollButtons();
    startHold();
#if BAS_PAUSE
    if (paused) {
        paused = pauseInput();
        if (paused) {
            audio::update();
            dbg::markRenderStart();
            gfx_wait();
            drawPause();                // over the held game's last picture (its draw changes state: not run)
            dbg::markRenderEnd();
            gfx_flushAsync();
            return;
        }
    } else if (chgame.justReleased(START_BUTTON) && !holdOff) {
        paused = true;                  // on release, so a hold to exit never opens it
        choice = 0;
    }
    game::buttons = cartButtons();
    if (holdOff) {
        if (!chgame.buttons()) holdOff = 0;
        game::buttons = 0;
    }
#else
    game::buttons = cartButtons();
#endif
    game::update();
    audio::update();

    dbg::markRenderStart();
    gfx_wait();
    cdata::flush();                     // the best score, between frames
    game::draw();
    screenPalette();
    dbg::markRenderEnd();
    gfx_flushAsync();
}
