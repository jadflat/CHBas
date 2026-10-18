# BAS for CHGame

yokoboko's PICO-8 game BAS, rewritten in C++ for the CHGame handheld: the
cart's Lua translated class by class, with its own sprites and sound, at the
cart's 60 frames a second. See [Bas/README.md](Bas/README.md) for the game,
[BUILDING.md](BUILDING.md) to build it, and [Bas/NOTES.md](Bas/NOTES.md) for
how the port was made and checked.

## Licence

- **The game** ([BAS](https://www.lexaloffle.com/bbs/?tid=54986) by yokoboko:
  its code as translated here, art, map, music and sound) is licensed under
  [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/), as the
  original cart is. Non-commercial use only, with credit to yokoboko, and
  changes shared under the same licence. See [Bas/NOTICE](Bas/NOTICE).
- **The port's framework and tools** come from
  [Cheleste](https://github.com/bateske/CHeleste) and are MIT
  ([LICENSE](LICENSE)).
- **The CHGame libraries** in `chgame/` are under their own licences
  ([chgame/LICENSE](chgame/LICENSE), [chgame/NOTICE](chgame/NOTICE)).
