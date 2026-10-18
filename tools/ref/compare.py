"""Golden traces: the port (Game.cpp) against the cart's own Lua, frame by frame.

    python tools/ref/compare.py [--fixed] [--frames N] [--seeds K]

From boot (the splash, the title, games, deaths, restarts), the same
made-up presses go to both:
  - the cart's Lua (assets/bas.p8), run by Lua 5.4 (pip install lupa) under
    a PICO-8 shim: btnp() without repeat, all()/del(), cartdata in memory,
    the screen palette for the splash's peek(), the same rnd() generator as
    Fix.cpp, no drawing (but _draw() runs: the game over's circles, the
    transition's end and the splash's fade move on there);
  - tools/ref/trace.cpp: Game.cpp built for the PC with num a double
    (BAS_NUM_DOUBLE), or with --fixed the device's 16.16.
The scene, the player, camera, trap, score, saws, jump effects, particles
and animations' states, and the sounds, are compared after every frame; the
first difference is printed with the frames before it.

With doubles the two must agree (to 1e-9). With --fixed (PICO-8's own
number format) the eased camera and jumps part a little from the Lua's
doubles; the rest is still compared, within 2 %.

Not reached here: the "overflow fix" at 2,600 jumps up, which relies on
16.16 wrapping that the Lua's doubles don't do.
"""
import argparse
import random
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / "Bas"
BUILD = ROOT / "out" / "ref"

SHIM = r"""
__rng = 1
function rnd(x)
  if x == nil then x = 1 end
  if not (x > 0) then return 0 end
  local s = __rng
  s = (s ~ (s << 13)) & 0xffffffff
  s = (s ~ (s >> 17)) & 0xffffffff
  s = (s ~ (s << 5)) & 0xffffffff
  __rng = s
  if __fixed then return (s % math.floor(x * 65536 + 0.5)) / 65536 end
  return (s / 4294967296.0) * x
end
function sin(a) return -math.sin(a * 6.283185307179586) end
function cos(a) return math.cos(a * 6.283185307179586) end
flr = math.floor
ceil = math.ceil
abs = math.abs
function min(a,b) if a < b then return a else return b end end
function max(a,b) if a > b then return a else return b end end
function add(c, x) c[#c+1] = x return x end
function del(c, v)
  local n = #c
  for i=1,n do
    if c[i] == v then
      for j=i,n do c[j] = c[j+1] end
      return v
    end
  end
end
function count(c) return #c end
function all(c)
  if c == nil or #c == 0 then return function() end end
  local i, prev = 1, nil
  return function()
    if c[i] == prev then i = i + 1 end
    while i <= #c and c[i] == nil do i = i + 1 end
    prev = c[i]
    return prev
  end
end
function foreach(c, f) for v in all(c) do f(v) end end
sub = string.sub
function tostr(v)
  if type(v) == "number" and v == math.floor(v) then return string.format("%d", v) end
  return tostring(v)
end
__btn, __prev = 0, 0
function btn(k) if k == nil then return __btn end return (__btn >> k) & 1 == 1 end
function btnp(k) return ((__btn & ~__prev) >> k) & 1 == 1 end
__cart = {}
function cartdata() end
function dget(i) return __cart[i] or 0 end
function dset(i, v) __cart[i] = v end
__scr = {}
for i=0,15 do __scr[i] = i end
function peek(a) if a >= 0x5f10 and a < 0x5f20 then return __scr[a - 0x5f10] end return 0 end
function poke() end
function pal(c, p, s)
  if c == nil then for i=0,15 do __scr[i] = i end return end
  if s == 1 then __scr[c] = p end
end
__events = ""
function sfx(n) __events = __events .. " sfx" .. n end
function music(n) __events = __events .. " mus" .. n end
function stat() return 0 end
function printh() end
for _, f in ipairs({"spr","sspr","rectfill","rect","circfill","line","pset","print","palt","camera","cls"}) do
  _G[f] = function() end
end
"""

DUMP = r"""
local function put(v)
  if v == nil then return " nil" end
  if v == true then return " 1" end
  if v == false then return " 0" end
  if v == 0 then v = 0 end
  return string.format(" %.17g", v)
end
local STATES = {initial=0, playing=1, will_die=2, dead=3}
function __frame(i, inp)
  __events = ""
  __btn = inp
  _update60()
  _draw()
  __prev = inp
  local sc = current_scene
  local t = {string.format("%d", i), __events, " |"}
  local isgame = getmetatable(sc) == game_scene
  local v = {isgame and 1 or 0, __splash.count, __splash.fading}
  if isgame then
    local saws, lefts, jfx, jfxs = 0, 0, 0, 0
    for _, c in pairs(sc.columns.tiles) do
      if c.saw then saws = saws + 1 if c.saw.left then lefts = lefts + c.idx end end
      if c.jfx then jfx = jfx + 1 jfxs = jfxs + (c.jfx.sprite or -1) end
    end
    local tr = sc.transition
    local code = tr.transition == "start" and 1 or (tr.transition == "finish" and 2 or 0)
    for _, x in ipairs({STATES[sc.state], sc.buttons_cooldown, sc.transition_scene and 1 or 0,
                        sc.player.pos.x, sc.player.pos.y, sc.player.jump_tile_pos, sc.camera.y, sc.trap.pos.y,
                        sc.trap.offset, sc.player.tile_pos, sc.player.jumping and 1 or 0,
                        sc.player.right_wall and 1 or 0, sc.player.sprite, sc.score.score, saws, lefts, jfx,
                        #sc.game_over.particles, sc.game_over.anim_t, tr.anim_t * 10 + code,
                        sc.game_initial == nil and 1 or 0, sc.game_over.highscore and 1 or 0, dget(0), jfxs}) do
      v[#v+1] = x
    end
  else
    for k=1,24 do v[#v+1] = 0 end
  end
  for _, x in ipairs(v) do t[#t+1] = put(x) end
  return table.concat(t)
end
"""

GLYPHS = {"⬅️": "\\139", "➡️": "\\145", "⬆️": "\\148", "⬇️": "\\131", "🅾️": "\\142", "❎": "\\151", "♥": "\\135"}


def shorthand_if(line):
    """PICO-8's `if (cond) stmt` (no then) as `if cond then stmt end`."""
    m = re.match(r"^(\s*)if\s*\(", line)
    if not m:
        return line
    i, depth = m.end() - 1, 0
    for j in range(i, len(line)):
        if line[j] == "(":
            depth += 1
        elif line[j] == ")":
            depth -= 1
            if depth == 0:
                rest = line[j + 1:]
                if re.search(r"\bthen\b", rest) or not rest.strip() or rest.strip().startswith(("and", "or")):
                    return line
                return f"{m.group(1)}if {line[i:j + 1]} then {rest.strip()} end"
    return line


def pico_to_lua(src):
    """The PICO-8 extensions this cart uses, as plain Lua."""
    for g, esc in GLYPHS.items():
        src = src.replace(g, esc)
    src = src.replace("\\^", "\\\\^")        # P8SCII's \^ (print control): a plain backslash here
    out = []
    for line in src.splitlines():
        line = line.replace("!=", "~=")
        line = shorthand_if(line)
        m = re.search(r"([\w.\[\]]+)\s*([-+*/%])=\s*", line)
        if m:
            rest = line[m.end():]
            k = re.search(r"\s+(return|end|then|else)\b", rest)
            rhs, tail = (rest[:k.start()], rest[k.start():]) if k else (rest, "")
            line = line[:m.start()] + f"{m.group(1)} = {m.group(1)} {m.group(2)} ({rhs.strip()})" + tail
        out.append(line)
    return "\n".join(out)


def lua_source(cart):
    text = Path(cart).read_text(encoding="utf-8")
    return pico_to_lua(text.split("__lua__", 1)[1].split("__gfx__", 1)[0])


class LuaGame:
    def __init__(self, cart, fixed):
        from lupa.lua54 import LuaRuntime
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.lua.execute(SHIM)
        g = self.lua.globals()
        g["__fixed"] = fixed
        self.lua.execute(lua_source(cart))
        self.lua.execute(DUMP)
        g["_init"]()
        g["__splash"] = g["current_scene"]
        self.frame = g["__frame"]

    def run(self, inputs):
        return [self.frame(i, b) for i, b in enumerate(inputs)]

    def bot(self, rng, n, care):
        """Play by looking at the game: jump to the wall away from the next
        saw, every few frames; with probability 1-care, the wrong way. Returns
        (inputs, lines)."""
        g = self.lua.globals()
        inputs, lines, wait = [], [], 170
        for i in range(n):
            b = 0
            sc = g["current_scene"]
            if wait > 0:
                wait -= 1
            elif self.lua.eval("getmetatable(current_scene) == game_scene"):
                if sc["state"] == "dead":
                    b, wait = 1 << rng.choice([0, 1, 4, 5]), rng.randint(30, 90)
                elif sc["player"]["jumping"]:
                    pass                                # a press now would jump on from the next tile
                else:
                    nxt = sc["columns"]["tiles"]["t" + str(int(sc["player"]["tile_pos"]) - 1)]
                    if nxt is not None:                 # (not yet scrolled into view: wait for it)
                        saw = nxt["saw"]
                        right = rng.random() < 0.5 if saw is None else bool(saw["left"])   # away from the saw
                        if rng.random() > care:
                            right = not right
                        b = 1 << rng.choice([1, 5] if right else [0, 4])
                        wait = rng.randint(0, 6)
            inputs.append(b)
            lines.append(self.frame(i, b))
        return inputs, lines


def build_trace(fixed):
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / ("trace_fixed" if fixed else "trace")
    srcs = [ROOT / "tools" / "ref" / "trace.cpp", SKETCH / "Game.cpp", SKETCH / "Fix.cpp",
            SKETCH / "src" / "assets" / "Sine.cpp"]
    newest = max(p.stat().st_mtime for p in srcs + list(SKETCH.glob("*.h")))
    if exe.exists() and exe.stat().st_mtime > newest:
        return exe
    cmd = ["g++", "-std=c++17", "-O1", "-w", f"-DBAS_NUM_DOUBLE={0 if fixed else 1}", f"-I{SKETCH}",
           "-o", str(exe)] + [str(s) for s in srcs]
    subprocess.run(cmd, check=True)
    return exe


def cpp_run(exe, inputs):
    f = BUILD / "inputs.bin"
    f.write_bytes(bytes(inputs))
    out = subprocess.run([str(exe), str(f)], capture_output=True, text=True, check=True)
    return out.stdout.splitlines()


def presses(rng, n):
    """Made-up presses: taps of a side (LEFT/O or RIGHT/X) with gaps, now
    quick (the saws catch you), now slow (the trap does)."""
    out = [0] * 170                      # the splash
    while len(out) < n:
        b = 1 << rng.choice([0, 1, 4, 5])
        out += [b] * rng.randint(1, 4)
        out += [0] * rng.choice([rng.randint(2, 12), rng.randint(10, 40), rng.randint(40, 120)])
    return out[:n]


def nums(s):
    return [x for x in re.split(r"[ |]+", s) if x]


def same(a, b, fixed):
    if a == b:
        return True
    ta, tb = a.split(" |"), b.split(" |")
    if len(ta) != len(tb) or ta[0] != tb[0]:
        return False
    xa, xb = nums(" |".join(ta[1:])), nums(" |".join(tb[1:]))
    if len(xa) != len(xb):
        return False
    for x, y in zip(xa, xb):
        if x == y:
            continue
        try:
            fx, fy = float(x), float(y)
        except ValueError:
            return False
        tol = 0.02 if fixed else 1e-9
        if abs(fx - fy) > tol * max(1, abs(fx), abs(fy)):
            return False
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cart", default=str(ROOT / "assets" / "bas.p8"))
    ap.add_argument("--fixed", action="store_true")
    ap.add_argument("--frames", type=int, default=4000)
    ap.add_argument("--seeds", type=int, default=12)
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--bot", type=int, default=6, help="also this many runs played by the bot")
    a = ap.parse_args(argv)
    exe = build_trace(a.fixed)
    bad = 0
    runs = [("tap", s) for s in range(a.seeds)] + [("bot", s) for s in range(a.bot)]
    for kind, seed in runs:
        if kind == "tap":
            inputs = presses(random.Random(seed), a.frames)
            lua = LuaGame(a.cart, a.fixed).run(inputs)
        else:
            rng = random.Random(1000 + seed)
            inputs, lua = LuaGame(a.cart, a.fixed).bot(rng, a.frames * 3, 0.97 + 0.03 * (seed % 2))
        cpp = cpp_run(exe, inputs)
        deaths = sum(1 for l in cpp if " sfx27" in l.split(" |")[0])
        best = max(int(l.split(" |")[1].split()[16]) for l in cpp)
        for i, (x, y) in enumerate(zip(lua, cpp)):
            if not same(x, y, a.fixed):
                bad += 1
                print(f"{kind} {seed}: frame {i} differs")
                for k in range(max(0, i - 2), i + 1):
                    print("  lua", lua[k][:300])
                    print("  c++", cpp[k][:300])
                break
        else:
            if len(lua) != len(cpp):
                bad += 1
                print(f"{kind} {seed}: {len(lua)} Lua frames, {len(cpp)} C++")
            elif not a.quiet:
                print(f"{kind} {seed}: {len(lua)} frames agree ({deaths} deaths, best score {best})")
    print(f"{len(runs) - bad} of {len(runs)} runs agree")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
