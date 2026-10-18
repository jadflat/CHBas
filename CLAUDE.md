# BAS: working notes for Claude

BAS (yokoboko's PICO-8 game) ported to CHGame in C++, on Cheleste's
framework. Read `Bas/NOTES.md` first.

## Rules

- `Bas/Game.cpp` is the cart's Lua (`assets/bas.p8`) translated in order. No
  CHGame calls in it (it also builds for the PC in `tools/ref/`), no changes
  to the cart's behaviour: a change is a `config.h` switch in the `.ino`.
- After touching `Game.cpp`, `Fix.*` or `P8.cpp`, run `python tools/ref/compare.py`
  (all must agree; P8 changes: compare the scripts' pictures) and `./cl check`.
- Numbers are `num` (Fix.h): never `float`/`double` in device code.
- `Bas/src/` is generated (`tools/p8extract.py`, `tools/p8audio.py`).
- Every frame must flush; draw only between `gfx_wait()` and `gfx_flushAsync()`.
- Budgets: flash <= 50,432 B, static RAM <= 18,416 B; 60 fps: update + render < 16.7 ms.

## Commands

`./cl build | upload | test | check | audio out/audio`,
`./cl run Bas/tools/scripts/<s>.txt out/<s>`. The cart file:
`python <CHGame>/tools/chgame.py --sketch Bas export` (this vendored copy of
the tools has no `chcart`), then `chgame cart prepare` for the card.
