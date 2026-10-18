// cartdata() on CHGame's flash save (CartData.h): BAS uses slot 0 only.
#include <CHGame.h>
#include "CartData.h"

namespace cdata {

static const uint32_t MAGIC = save::magic("CBAS");     // its own: every game shares the pages
static const uint8_t VERSION = 1;
static const uint8_t SLOTS = 1;

struct Data { int32_t v[SLOTS]; };    // PICO-8 numbers, as their 16.16 bits
static Data data;
static bool dirty;

void begin() {
    if (!save::load(MAGIC, VERSION, data)) memset(&data, 0, sizeof data);
}

num dget(int i) { return (unsigned)i < SLOTS ? num::bits(data.v[i]) : num(0); }

void dset(int i, num v) {
    if ((unsigned)i >= SLOTS || data.v[i] == v.v) return;
    data.v[i] = v.v;
    dirty = true;
}

void flush() {
    if (dirty) dirty = !save::store(MAGIC, VERSION, data);
}

}  // namespace cdata
