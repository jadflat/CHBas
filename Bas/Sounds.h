// The cart's sfx() and music(), on CHGame's piezo (Sounds.cpp).
#pragma once
#include <stdint.h>

namespace snd {

void begin();
// sfx(n, ch): one of the cart's 64 effects; sfx(-1, ch) stops the channel.
// The piezo has one voice, so the channel only matters for stopping.
void sfx(int n, int ch);
void music(int n, int fadeMs);      // music(n, fade): -1 stops

}  // namespace snd
