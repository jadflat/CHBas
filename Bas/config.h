// BAS's build switches: every one is here (BAS_*). A build sets them with
// -D (build.extra_flags), or leaves the defaults.
#pragma once

#define BAS_VERSION "1.0"

// num as a double instead of PICO-8's 16.16 (host only: tools/ref/'s traces
// against the cart's Lua).
#ifndef BAS_NUM_DOUBLE
#define BAS_NUM_DOUBLE 0
#endif

// The cart's music (one voice: the lead line). 0: sound effects only.
#ifndef BAS_MUSIC
#define BAS_MUSIC 1
#endif

// START pauses, as PICO-8's pause button did: CONTINUE and EXIT TO MENU
// (PICO-8's RESET CART: under the menu bootloader a reset is the menu).
#ifndef BAS_PAUSE
#define BAS_PAUSE 1
#endif

// A and B swapped: A (PICO-8's O) jumps to the left wall, B (X) to the right.
// 0: the original mapping, B left and A right.
#ifndef BAS_SWAP_AB
#define BAS_SWAP_AB 1
#endif
