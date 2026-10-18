// PICO-8's cartdata(): numbers that outlive the cart (BAS keeps its best
// score in slot 0). Save.cpp keeps them in CHGame's flash save; the trace
// harness keeps them in memory.
#pragma once
#include "Fix.h"

namespace cdata {

void begin();                       // cartdata(...): read what was kept
num dget(int i);
void dset(int i, num v);
void flush();                       // write what changed (board: between gfx_wait() and the flush)

}  // namespace cdata
