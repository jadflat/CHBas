# Building BAS

Needs Python 3 (with Pillow and ziglang, for the simulator), arduino-cli and
the CHGame board package 0.3.0, the same as any CHGame game (`chgame/tools/README.md`).
The CHGame tools are vendored in `chgame/` (`chgame/VENDORED.md` names the
commit; `python tools/vendor_chgame.py <CHGame checkout>` refreshes them).

`cl` (Git Bash) and `cl.cmd` (Windows) run the CHGame tools on the sketch:

```bash
./cl build
```

| Command | What |
|---|---|
| `./cl build` | the release image, with its flash and RAM |
| `./cl build --debug` | with the debug protocol (scripts on the board) |
| `./cl upload` | onto the board |
| `./cl run Bas/tools/scripts/smoke.txt out/smoke` | a script in the simulator (pictures in `Bas/out/smoke`) |
| `./cl test` | the host tests |
| `./cl check` | everything that needs no board: tests, every script twice, the device build |
| `./cl gif` | the README's `docs/gameplay.gif` |
| `./cl audio out/audio` | the effects and songs as WAV |

## From the cart

The cart is not in the repository; the files made from it are. To make them
again, put `bas.p8` in `assets/` (in PICO-8: `load #bas-9`, then
`save bas.p8`; or from `bas.p8.png`: `shrinko8 bas.p8.png bas.p8`) and run:

```bash
python tools/p8extract.py      # Bas/src/assets: the sheet, the map's corner, OPAQUE, the palette
python tools/p8audio.py        # Bas/src/audio: the effects and songs
```

## Checking against the original

With the cart in `assets/` and `pip install lupa`:

```bash
python tools/ref/compare.py
```

That runs the cart's own Lua and the port side by side, frame by frame
(`./cl test` runs a short version). Bas/NOTES.md has what it checks.

## The cart for the card

This vendored copy of the CHGame tools has no `chcart`; use a full CHGame
checkout's:

```bash
python <CHGame>/tools/chgame.py --sketch Bas export          # Bas/build/Bas.chgame
python <CHGame>/tools/chgame.py cart prepare Bas/build/Bas.chgame out/card
```

`out/card/GAMES/BAS.CHG` alone can be copied into an existing card's
`GAMES/`: the menu lists CHG files it finds after the cart's own.
