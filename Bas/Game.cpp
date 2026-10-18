// BAS's Lua (assets/bas.p8) in C++: see Game.h. Class by class as the cart
// has them.
#include <string.h>
#include "Game.h"
#include "P8.h"
#include "Sounds.h"
#include "CartData.h"

namespace game {

uint8_t buttons;
static uint8_t pressed;             // btnp(): down this frame and not the last
static uint8_t lastButtons;

static bool btnp(int i) { return (pressed >> i) & 1; }
static num ceil_(num a) { return -flr(-a); }
static void sfx(int n) { snd::sfx(n, -1); }
static void music(int n, int fade = 0) { snd::music(n, fade); }

// The drawing calls take PICO-8's numbers and floor them, as PICO-8 does.
static void spr(num n, num x, num y, int w = 1, int h = 1, bool fx = false, bool fy = false) {
    p8::spr(toi(n), toi(x), toi(y), w, h, fx, fy);
}
static void sspr(num sx, num sy, num sw, num sh, num dx, num dy, num dw, num dh) {
    p8::sspr(toi(sx), toi(sy), toi(sw), toi(sh), toi(dx), toi(dy), toi(dw), toi(dh));
}
static void rectfill(num a, num b, num c, num d, int col = p8::PEN) { p8::rectfill(toi(a), toi(b), toi(c), toi(d), col); }
static void line(num a, num b, num c, num d, int col = p8::PEN) { p8::line(toi(a), toi(b), toi(c), toi(d), col); }
static void pset(num a, num b, int col = p8::PEN) { p8::pset(toi(a), toi(b), col); }
static void circfill(num x, num y, num r, int col = p8::PEN) { p8::circfill(toi(x), toi(y), toi(r), col); }
static void print(const char *s, num x, num y, int col = p8::PEN) { p8::print(s, toi(x), toi(y), col); }
static void printn(int v, num x, num y, int col = p8::PEN) { p8::printNum(v, toi(x), toi(y), col); }
using p8::pal;
using p8::palt;

// #tostr(n) for a whole number.
static int tostrLen(int v) {
    int n = v < 0 ? 2 : 1;
    if (v < 0) v = -v;
    while (v >= 10) { v /= 10; n++; }
    return n;
}

static num ternary(bool cond, num T, num F) { return cond ? T : F; }

// t = how far through the current movement you are
// b = where the movement starts
// c = the final change in value at the end.
// d = the total duration of the movement
static num easing_cubic_out(num t, num b, num c, num d) {
    t /= d;
    t -= 1;
    return c * (t * t * t + 1) + b;
}

static num easing_cubic_in(num t, num b, num c, num d) {
    t /= d;
    return c * t * t * t + b;
}

static num easing_cubic_in_out(num t, num b, num c, num d) {
    t /= d / 2;
    if (t < 1) return c / 2 * t * t * t + b;
    t -= 2;
    return c / 2 * (t * t * t + 2) + b;
}

// ---------------------------------------------------------------------------
// animatable: a class's named animations, the current one, its frame
// ---------------------------------------------------------------------------
struct AnimDef { uint8_t duration, n; int16_t list[6]; };
static const int16_t NIL = -32768;   // sprite nil (after a -1 in the list)

struct Anim {
    const AnimDef *defs;            // the class's add_animation() calls, in order
    int8_t current;                 // -1: nil
    uint8_t frame, count;
    int16_t sprite;
};

// animatable.new, then add_animation() of the class's first: it becomes current.
static void anim_new(Anim &a, const AnimDef *defs) {
    a.defs = defs;
    a.frame = 1;
    a.count = 1;
    a.sprite = 0;
    a.current = 0;
    a.sprite = defs[0].list[a.frame - 1];
}

static void play_animation(Anim &a, int name) {
    if (a.current != name) {
        a.current = (int8_t)name;
        a.count = 1;
        a.frame = 1;
        a.sprite = a.defs[a.current].list[a.frame - 1];
    }
}

static void update_animation(Anim &a) {
    if (a.current >= 0) {
        a.count += 1;
        const AnimDef &animation = a.defs[a.current];
        if (a.count > animation.duration) {
            a.count = 1;
            a.frame += 1;
            if (a.frame > animation.n) a.frame = 1;
        }
        a.sprite = a.defs[a.current].list[a.frame - 1];
        if (a.sprite == -1) {
            a.current = -1;
            a.sprite = NIL;
        }
    }
}

// ---------------------------------------------------------------------------
// collidable
// ---------------------------------------------------------------------------
struct Pos { num x, y; };
struct Box { int8_t x, y, w, h; };

static bool collide(const Pos &sp, const Box &sh, const Pos &op, const Box &oh) {
    return op.x + oh.x + oh.w > sp.x + sh.x &&
           op.y + oh.y + oh.h > sp.y + sh.y &&
           op.x + oh.x < sp.x + sh.x + sh.w &&
           op.y + oh.y < sp.y + sh.y + sh.h;
}

// ---------------------------------------------------------------------------
// splash_scene
// ---------------------------------------------------------------------------
struct Splash {
    const char *text;
    int fading, fadespeed, start_fadeout, count;
};

static int current_scene;           // 0 splash_scene, 1 game_scene
static Splash splash;
static void game_scene_new();

static void splash_new() {
    splash.text = "yokoboko";
    splash.fading = 0;
    splash.fadespeed = 5;
    splash.start_fadeout = 120;
    splash.count = 0;
}

static void splash_update() {
    Splash &s = splash;
    if (s.count < s.start_fadeout) {
        s.count += 1;
        if (s.count == s.start_fadeout) s.fading = 1;
    }
    if (s.fading == -1 && s.count == s.start_fadeout) {
        current_scene = 1;
        game_scene_new();
    }
}

static void splash_fadeout() {
    static const uint8_t fade[32] = { 0, 17, 18, 19, 20, 16, 22, 6, 24, 25, 9, 27, 28, 29, 29, 31,
                                      0, 0, 16, 17, 16, 16, 5, 0, 2, 4, 0, 3, 1, 18, 2, 4 };
    Splash &s = splash;
    s.fading += 1;
    if (s.fading % s.fadespeed == 1) {
        for (int i = 0; i <= 15; i++) {
            int c = p8::screenPal((uint8_t)i);
            if (c >= 128) c -= 112;
            int p = fade[c];
            if (p >= 16) p += 112;
            p8::palScreen((uint8_t)i, (uint8_t)p);
        }
        if (s.fading == 7 * s.fadespeed + 1) {
            p8::cls();
            pal();
            s.fading = -1;
        }
    }
}

static void splash_draw() {
    Splash &s = splash;
    p8::cls();
    if (s.fading > 0) splash_fadeout();
    if (s.fading < 0) return;
    int y = 47;
    spr(204, 47, y, 4, 4);
    print(s.text, 63 - (int)strlen(s.text) * 2, y + 34, 15);
}

// ---------------------------------------------------------------------------
// background
// ---------------------------------------------------------------------------
struct Background {
    int tile_width, tile_height, x, heigh;
    uint8_t n;
    int16_t tiles[8];               // the set of visible tile numbers
};

static void background_new(Background &b) {
    b.tile_width = 8;           //tiles
    b.tile_height = 4;          //tiles
    b.x = 32;
    b.heigh = b.tile_height * 8 * 3;    //pixels
    b.n = 0;
}

static void background_update(Background &b, num cam_y) {
    //visible
    int min_tile = toi(flr(cam_y / b.heigh));
    int max_tile = toi(ceil_((cam_y + 128) / b.heigh));
    //delete
    uint8_t k = 0;
    for (uint8_t i = 0; i < b.n; i++)
        if (!(b.tiles[i] < min_tile || b.tiles[i] > max_tile)) b.tiles[k++] = b.tiles[i];
    b.n = k;
    //create
    for (int i = min_tile; i <= max_tile; i++) {
        bool have = false;
        for (uint8_t j = 0; j < b.n; j++)
            if (b.tiles[j] == i) have = true;
        if (!have && b.n < 8) b.tiles[b.n++] = (int16_t)i;
    }
}

static void background_draw(Background &b) {
    for (uint8_t i = 0; i < b.n; i++) {
        num y = num(b.tiles[i]) * b.heigh;
        spr(136, b.x, y, b.tile_width, b.tile_height);
        spr(136, b.x, y + 32, b.tile_width, b.tile_height);
        spr(128, b.x, y + 64, b.tile_width, b.tile_height);
    }
}

// ---------------------------------------------------------------------------
// saw, jump_effect, columns
// ---------------------------------------------------------------------------
static const AnimDef SAW_ANIMS[] = { { 3, 2, { 68, 71 } } };
struct Saw {
    int idx, tile_width, tile_height;
    Anim anim;
    Pos pos;
    Box hitbox;
    bool left;
};

static void saw_new(Saw &o, int idx, int tile_width, int tile_height) {
    o.idx = idx; o.tile_width = tile_width; o.tile_height = tile_height;
    o.left = rnd(1) < num(0.5);
    //collidable
    int x = o.left ? o.tile_width - 8 : 128 - o.tile_width - 16;
    o.pos.x = x; o.pos.y = num(o.idx) * o.tile_height;
    o.hitbox = { 3, 3, 16, 16 };
    //animatable
    anim_new(o.anim, SAW_ANIMS);         // add_animation("spin",3,{68,71})
}

static void saw_draw(Saw &o) {
    palt(11, true);
    spr(o.anim.sprite, o.pos.x, o.pos.y, 3, 3, !o.left);
    palt();
}

static const AnimDef JFX_ANIMS[] = { { 3, 6, { 32, 33, 34, 35, 36, -1 } } };
struct Jfx {
    Anim anim;
    bool right_wall;
    num y;
    int left_x, right_x;
};

static void jfx_new(Jfx &o, bool right_wall, num y) {
    o.right_wall = right_wall;
    o.y = y;
    o.left_x = 16;
    o.right_x = 104;
    anim_new(o.anim, JFX_ANIMS);        // add_animation("fx",3,{32,33,34,35,36,-1})
}

static void jfx_draw(Jfx &o) {
    if (o.anim.sprite != NIL) {
        int x = o.right_wall ? o.right_x : o.left_x;
        spr(o.anim.sprite, x, o.y + 4, 1, 2, o.right_wall);
    }
}

struct Player;
struct ColTile { bool used; int idx; bool hasSaw, hasJfx; Saw saw; Jfx jfx; };
struct Columns { ColTile tiles[10]; };

static ColTile *col_tile(Columns &c, int idx) {
    for (auto &t : c.tiles)
        if (t.used && t.idx == idx) return &t;
    return nullptr;
}

static void columns_new(Columns &c) {
    for (auto &t : c.tiles) t.used = false;
}

// ---------------------------------------------------------------------------
// player
// ---------------------------------------------------------------------------
enum { P_IDLE, P_SCARED, P_WILL_DIE, P_FLYING };
static const AnimDef PLAYER_ANIMS[] = { { 20, 2, { 0, 2 } }, { 1, 1, { 4 } }, { 3, 3, { 10, 12, 14 } }, { 2, 2, { 6, 8 } } };
struct Player {
    Anim anim;
    Pos pos;
    Box hitbox;
    bool right_wall;
    int left_x, right_x, tile_height, tile_offset_y, tile_pos;
    bool jumping;
    num jump_speed, jump_speed_will_die, jump_tile_pos;
    bool jump_changes_direction;
    int jump_amp;
};

static void player_new(Player &o, int tile_height) {
    o.right_wall = rnd(1) < num(0.5);
    o.left_x = 12;
    o.right_x = 100;
    o.tile_height = tile_height;
    o.tile_offset_y = 4;            //in pixels
    o.tile_pos = 1362;              //start: 1362
    o.jumping = false;
    o.jump_speed = num(0.08);       // in tiles
    o.jump_speed_will_die = num(0.0042);    // in tiles
    o.jump_tile_pos = 0;            //in tiles
    o.jump_changes_direction = false;
    o.jump_amp = 8;                 //amplitude
    //collidable
    o.pos.x = o.left_x; o.pos.y = num(o.tile_pos) * o.tile_height + o.tile_offset_y;
    o.hitbox = { 3, 1, 8, 11 };
    //animatable
    anim_new(o.anim, PLAYER_ANIMS);
}

// action_right: 1 right, 0 left, -1 nil
static void player_update(Player &o, int action_right, num trap_y, bool will_die) {
    //jump
    if (action_right >= 0) {
        sfx(20);
        if (o.jumping) o.tile_pos -= 1;
        else o.jumping = true;
        o.jump_tile_pos = 0;
        o.jump_changes_direction = o.right_wall != (action_right == 1);
        o.right_wall = action_right == 1;
    }
    if (o.jumping) {
        num treshold = o.jump_changes_direction ? num(-0.7) : num(-0.275);
        num speed = ternary(will_die && o.jump_tile_pos < treshold, o.jump_speed_will_die, o.jump_speed);
        o.jump_tile_pos = mx(o.jump_tile_pos - speed, num(-1));
        if (o.jump_tile_pos == -1) {
            o.jumping = false;
            o.tile_pos -= 1;
            o.jump_tile_pos = 0;
        }
    }
    //position
    o.pos.y = (o.tile_pos + o.jump_tile_pos) * o.tile_height + o.tile_offset_y;
    if (o.jumping) {
        if (o.jump_changes_direction) {
            num offset = (o.right_x - o.left_x) * nabs(o.jump_tile_pos);
            o.pos.x = ternary(o.right_wall, o.left_x + offset, o.right_x - offset);
        } else {
            num offset = p8sin(o.jump_tile_pos / 2) * o.jump_amp;
            o.pos.x = ternary(o.right_wall, o.right_x - offset, o.left_x + offset);
        }
    } else {
        o.pos.x = ternary(o.right_wall, o.right_x, o.left_x);
    }
    //animation
    if (will_die) play_animation(o.anim, P_WILL_DIE);
    else if (o.jumping) play_animation(o.anim, P_FLYING);
    else if (trap_y - o.pos.y < 28) play_animation(o.anim, P_SCARED);
    else play_animation(o.anim, P_IDLE);
    update_animation(o.anim);
}

static void player_draw(Player &o) {
    bool facing_left = o.jumping ? !o.right_wall : o.right_wall;
    spr(o.anim.sprite, o.pos.x, o.pos.y, 2, 2, facing_left);
}

// ---------------------------------------------------------------------------
// columns (with the saws and jump effects in its tiles)
// ---------------------------------------------------------------------------
static void columns_update(Columns &c, int tile_width, int tile_height, num cam_y, Player &player, int action_right,
                           bool is_dead) {
    //visible
    int min_tile = toi(flr(cam_y / tile_height));
    int max_tile = toi(ceil_((cam_y + 128) / tile_height));
    //delete
    for (auto &t : c.tiles)
        if (t.used && (t.idx < min_tile || t.idx > max_tile)) t.used = false;
    //create, add jump effect and update
    for (int i = min_tile; i <= max_tile; i++) {
        ColTile *t = col_tile(c, i);
        if (!t) {
            //create [don't add saws to the first few tiles (first from bottom up is 1365)]
            for (auto &f : c.tiles)
                if (!f.used) { t = &f; break; }
            if (!t) continue;               // (never: 7 tiles are visible at most)
            t->used = true;
            t->idx = i;
            t->hasJfx = false;
            t->hasSaw = i < 1358;
            if (t->hasSaw) saw_new(t->saw, i, tile_width, tile_height);
        } else if (t->hasSaw) {
            //update saw
            update_animation(t->saw.anim);
        }
        if (!is_dead) {
            if (t->hasJfx) {
                //update jump effect
                update_animation(t->jfx.anim);
            } else if (action_right >= 0 && player.tile_pos == i) {
                //add jump effect
                t->hasJfx = true;
                jfx_new(t->jfx, player.pos.x > 63, num(i) * tile_height);
            }
        }
    }
}

static void columns_shift(Columns &c, int tile_height, num cam_y) {
    //visible
    int min_tile = toi(flr(cam_y / tile_height));
    int max_tile = toi(ceil_((cam_y + 128) / tile_height));
    const int shift = 2600;
    for (int i = min_tile; i <= max_tile; i++) {
        ColTile *t = col_tile(c, i);
        if (!t) continue;
        int idx = i + shift;
        t->idx = idx;
        t->saw.idx = idx;
        t->saw.pos.y = num(idx) * tile_height;
        if (t->hasJfx) t->jfx.y = num(idx) * tile_height;
    }
}

static void columns_draw(Columns &c, int tile_width, int tile_height) {
    for (auto &v : c.tiles) {
        if (!v.used) continue;
        if (v.hasSaw) saw_draw(v.saw);
        if (v.hasJfx) jfx_draw(v.jfx);
        int left_sprite = (v.hasSaw && v.saw.left) ? 66 : 64;
        int right_sprite = (v.hasSaw && !v.saw.left) ? 66 : 64;
        num y = num(v.idx) * tile_height;
        spr(left_sprite, 0, y, 2, 3);
        spr(right_sprite, 128 - tile_width, y, 2, 3, true);
    }
}

static bool columns_collide(Columns &c, Player &player) {
    for (auto &v : c.tiles)
        if (v.used && v.hasSaw && collide(v.saw.pos, v.saw.hitbox, player.pos, player.hitbox)) return true;
    return false;
}

static bool columns_will_collide(Columns &c, Player &player) {
    int target_tile = player.tile_pos - 1;
    ColTile *t = col_tile(c, target_tile);
    return t && t->hasSaw && t->saw.left != player.right_wall;
}

// ---------------------------------------------------------------------------
// game_camera
// ---------------------------------------------------------------------------
struct Camera { int offset, tracking, track_faster; num y; };

static void camera_new(Camera &o, num y) {
    o.offset = -84;                 //player y offset
    o.tracking = 26;                //slowdown player tracking
    o.track_faster = 74;            //track faster if player is less than this treshold
    o.y = y;
    o.y += o.offset;
}

static void camera_update(Camera &o, num player_y) {
    num cam_tracking = o.tracking;
    //track faster
    if (player_y - o.y < o.track_faster) cam_tracking /= 2;
    //shift
    o.y -= (o.y - (player_y + o.offset)) / cam_tracking;
}

static void camera_draw(Camera &o) { p8::camera(0, toi(o.y)); }

// ---------------------------------------------------------------------------
// higscore, controls_demo, game_initial
// ---------------------------------------------------------------------------
struct Highscore { int highscore; char highscore_string[16]; int width, height, x; num y; int offset_y; };

static void highscore_new(Highscore &o) {
    o.highscore = toi(cdata::dget(0));
    // "best "..o.highscore
    strcpy(o.highscore_string, "best ");
    char *d = o.highscore_string + 5;
    int v = o.highscore, n = 0;
    char tmp[12];
    bool neg = v < 0;
    if (neg) v = -v;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    if (neg) *d++ = '-';
    while (n) *d++ = tmp[--n];
    *d = 0;
    o.width = (int)strlen(o.highscore_string) * 4;
    o.height = 6;
    o.x = 64 - toi(ceil_(num(o.width) / 2));
    o.y = 0;
    o.offset_y = 54;
}

static void highscore_update(Highscore &o, num cam_y) {
    if (o.highscore == 0) return;
    o.y = cam_y + o.offset_y;
}

static void highscore_draw(Highscore &o) {
    if (o.highscore == 0) return;
    palt(0, false);
    palt(11, true);
    rectfill(o.x, o.y, o.x + o.width, o.y + o.height, 0);
    spr(115, o.x - 8, o.y, 1, 1, true);
    spr(115, o.x + o.width + 1, o.y);
    palt();
    print(o.highscore_string, o.x + 1, o.y + 1, 15);
}

//s: start; c: change; d: duration; out: ease out/in; lb: left button pressed; rb: right pressed; f: flip char at the end of the animation
struct DemoAnim { num s, c; int8_t d; bool out, lb, rb; int8_t f; };   // f: -1 nil
static const DemoAnim DEMO[] = {
    //jump left side
    { 0, num(0.3), 14, true, true, false, -1 },
    { num(0.3), num(-0.3), 14, false, false, false, -1 },
    { 0, 0, 8, true, false, false, -1 },
    { 0, num(0.3), 14, true, true, false, -1 },
    { num(0.3), num(-0.3), 14, false, false, false, -1 },
    { 0, 0, 8, true, false, false, -1 },
    { 0, num(0.3), 14, true, true, false, -1 },
    { num(0.3), num(-0.3), 14, false, false, false, -1 },
    { 0, 0, 8, true, false, false, -1 },
    //change direction
    { 0, 1, 24, true, false, true, 1 },
    { 1, 0, 24, true, false, false, -1 },
    //jump right side
    { 1, num(-0.3), 14, true, false, true, -1 },
    { num(0.7), num(0.3), 14, false, false, false, -1 },
    { 1, 0, 8, true, false, false, -1 },
    { 1, num(-0.3), 14, true, false, true, -1 },
    { num(0.7), num(0.3), 14, false, false, false, -1 },
    { 1, 0, 8, true, false, false, -1 },
    { 1, num(-0.3), 14, true, false, true, -1 },
    { num(0.7), num(0.3), 14, false, false, false, -1 },
    { 1, 0, 8, true, false, false, -1 },
    //change direction
    { 1, -1, 24, true, true, false, 0 },
    { 0, 0, 24, true, false, false, -1 },
};
static const int DEMO_N = sizeof DEMO / sizeof DEMO[0];

struct ControlsDemo {
    int width, height, x;
    num y;
    int offset_y, col, active_col, anim_idx, anim_t;
    num char_x;
    int char_width, char_height;
    bool char_flip;
    int lbtn_pressed, rbtn_pressed;
};

static void controls_demo_new(ControlsDemo &o) {
    o.width = 38;
    o.height = 22;
    o.x = 63 - toi(flr(num(o.width) / 2));
    o.y = 0;
    o.offset_y = 66;
    o.col = 13;
    o.active_col = 15;
    o.anim_idx = 1;
    o.anim_t = 0;
    o.char_x = 0;
    o.char_width = 8;
    o.char_height = 4;
    o.char_flip = false;
    o.lbtn_pressed = 0;
    o.rbtn_pressed = 0;
}

static void controls_demo_update(ControlsDemo &o, num pos_y) {
    o.y = pos_y + o.offset_y;
    //update animation index and time
    const DemoAnim *anim = &DEMO[o.anim_idx - 1];
    o.anim_t = o.anim_t + 1;
    if (num(o.anim_t) > num(anim->d) / 2 && anim->f >= 0) o.char_flip = anim->f == 1;
    if (o.anim_t >= anim->d) {
        o.anim_idx += 1;
        o.anim_t = 0;
        if (o.anim_idx > DEMO_N) o.anim_idx = 1;
        anim = &DEMO[o.anim_idx - 1];
        if (anim->lb) o.lbtn_pressed = 14;
        else if (anim->rb) o.rbtn_pressed = 14;
    } else {
        o.lbtn_pressed = toi(mx(num(o.lbtn_pressed - 1), 0));
        o.rbtn_pressed = toi(mx(num(o.rbtn_pressed - 1), 0));
    }
    //update rect_x
    num anim_value = ternary(anim->out, easing_cubic_out(o.anim_t, anim->s, anim->c, anim->d),
                             easing_cubic_in(o.anim_t, anim->s, anim->c, anim->d));
    o.char_x = anim_value * (o.width - o.char_width);
}

static void controls_demo_draw(ControlsDemo &o) {
    //vertical lines
    line(o.x, o.y, o.x, o.y + o.height - 4, o.active_col);
    line(o.x + o.width - 1, o.y, o.x + o.width - 1, o.y + o.height - 4, o.active_col);
    //char
    spr(37, o.x + o.char_x, o.y + 6, 1, 1, o.char_flip);
    //button icons
    static const char L[] = { p8::GL_LEFT, 0 }, R[] = { p8::GL_RIGHT, 0 }, O[] = { p8::GL_O, 0 }, X[] = { p8::GL_X, 0 };
    int left_col = o.lbtn_pressed > 0 ? o.active_col : o.col;
    palt(0, false);
    palt(11, true);
    print(L, o.x - 3, o.y + o.height, 0);
    print(L, o.x - 3, o.y + o.height - 1, left_col);
    print(O, o.x - 3, o.y + o.height + 7, 0);
    print(O, o.x - 3, o.y + o.height + 6, o.col);
    left_col = o.rbtn_pressed > 0 ? o.active_col : o.col;
    print(R, o.x + o.width - 4, o.y + o.height, 0);
    print(R, o.x + o.width - 4, o.y + o.height - 1, left_col);
    print(X, o.x + o.width - 4, o.y + o.height + 7, 0);
    print(X, o.x + o.width - 4, o.y + o.height + 6, o.col);
    palt();
}

struct GameInitial {
    num cam_y, y;
    bool anim, anim_finished;
    int anim_t, anim_d;
    Highscore highscore;
    ControlsDemo controls_demo;
};

static void game_initial_new(GameInitial &o) {
    o.cam_y = 0;
    o.y = 0;
    o.anim = false;
    o.anim_finished = false;
    o.anim_t = 0;
    o.anim_d = 32;
    highscore_new(o.highscore);
    controls_demo_new(o.controls_demo);
}

static void game_initial_update(GameInitial &o, num cam_y) {
    num offset_y = 0;
    if (o.anim && o.anim_finished == false) {
        offset_y = easing_cubic_in_out(o.anim_t, 0, 110, o.anim_d);
        o.anim_t += 1;
        if (o.anim_t > o.anim_d) o.anim_finished = true;
    }
    o.cam_y = cam_y + offset_y;
    highscore_update(o.highscore, o.cam_y + o.y);
    controls_demo_update(o.controls_demo, o.cam_y + o.y);
}

static void game_initial_draw(GameInitial &o) {
    static const uint8_t HIDE[16] = { 0, 1, 3, 2, 5, 13, 6, 2, 4, 2, 11, 12, 1, 4, 93, 0 };
    if (o.anim) p8::palTable(HIDE);
    //name sprite
    palt(0, false);
    palt(11, true);
    spr(192, 34, o.cam_y + 18, 8, 4);
    palt();
    //highscore and controls demo
    highscore_draw(o.highscore);
    controls_demo_draw(o.controls_demo);
    pal();
}

// ---------------------------------------------------------------------------
// game_over
// ---------------------------------------------------------------------------
struct Circle { int x, y, r, d; };
struct Particle { num x, y; int t; num die, dx, dy; bool grav, grow, shrink; num r; int c; const uint8_t *c_table; };

struct GameOver {
    int x, y;
    Circle circles[8];
    num cam_y, player_x, player_y;
    int anim_t;
    bool scoreSet;                  // score ~= nil
    int score;
    bool highscore;
    Particle particles[12];
    uint8_t np;
    int explode_size;
    int explode_amount;
};
static const uint8_t EXPLODE_COLORS[4] = { 8, 8, 8, 4 };

static void game_over_new(GameOver &o) {
    o.x = 63;
    o.y = 38;
    const Circle c[8] = {
        { o.x, o.y, 28, 16 },
        { o.x, o.y + 24, 8, 18 },
        { o.x - 20, o.y - 20, 8, 20 },
        { o.x - 20, o.y + 32, 2, 22 },
        { o.x + 20, o.y - 34, 1, 24 },
        { o.x + 20, o.y + 34, 4, 26 },
        { o.x - 39, o.y + 10, 3, 28 },
        { o.x - 32, o.y + 18, 1, 28 },
    };
    memcpy(o.circles, c, sizeof c);
    o.cam_y = 0;
    o.player_x = 0;
    o.player_y = 0;
    o.anim_t = 0;
    o.scoreSet = false;
    o.score = 0;
    o.highscore = false;
    o.np = 0;
    o.explode_size = 3;
    o.explode_amount = 10;
}

static void game_over_add_particle(GameOver &o, num x, num y, num die, num dx, num dy, bool grav, bool grow,
                                   bool shrink, num r, const uint8_t *c_table) {
    if (o.np >= 12) return;
    Particle &fx = o.particles[o.np++];
    fx.x = x; fx.y = y; fx.t = 0; fx.die = die; fx.dx = dx; fx.dy = dy;
    fx.grav = grav; fx.grow = grow; fx.shrink = shrink; fx.r = r; fx.c = 0; fx.c_table = c_table;
}

// explosion effect
static void game_over_explode(GameOver &o, num x, num y, int r, const uint8_t *c_table, int n) {
    for (int i = 0; i <= n; i++) {
        num die = 20 + rnd(10);             // the arguments in their order
        num dx = rnd(2) - 1;
        num dy = rnd(2) - 1;
        game_over_add_particle(o, x, y, die, dx, dy, false, false, true, r, c_table);
    }
}

static void game_over_update(GameOver &o, num cam_y, const Pos &player_pos, int score) {
    o.cam_y = cam_y;
    o.player_x = player_pos.x + 8;
    o.player_y = player_pos.y - cam_y + 8;
    //set score, highscore and explode
    if (!o.scoreSet) {
        o.scoreSet = true;
        o.score = score;
        int highscore = toi(cdata::dget(0));
        if (score > highscore) {
            cdata::dset(0, score);
            o.highscore = highscore != 0;
        } else {
            o.highscore = false;
        }
        game_over_explode(o, player_pos.x, player_pos.y, o.explode_size, EXPLODE_COLORS, o.explode_amount);
        sfx(25);
    }
    //update particles: for p in all(particles), del() on the way. all()
    //visits each once; a deleted particle's last changes are never seen.
    uint8_t i = 0;
    while (i < o.np) {
        Particle &p = o.particles[i];
        //lifetime
        p.t += 1;
        if (num(p.t) > p.die) {
            for (uint8_t k = i; k + 1 < o.np; k++) o.particles[k] = o.particles[k + 1];
            o.np--;
            continue;                           // the next one is now at i
        }
        //color depends on lifetime
        num frac = num(p.t) / p.die;
        if (frac < num(1) / 4) p.c = p.c_table[0];
        else if (frac < num(2) / 4) p.c = p.c_table[1];
        else if (frac < num(3) / 4) p.c = p.c_table[2];
        else p.c = p.c_table[3];
        //physics
        if (p.grav) p.dy += num(0.5);
        if (p.grow) p.r += num(0.1);
        if (p.shrink) p.r -= num(0.1);
        //move
        p.x += p.dx;
        p.y += p.dy;
        i++;
    }
}

static void game_over_draw(GameOver &o) {
    //explosion
    for (uint8_t i = 0; i < o.np; i++) {
        Particle &p = o.particles[i];
        //draw pixel for size 1, draw circle for larger
        if (p.r <= 1) pset(p.x, p.y, p.c);
        else circfill(p.x, p.y, p.r, p.c);
    }
    //circless
    o.anim_t = o.anim_t + 1 < 100 ? o.anim_t + 1 : 100;
    for (int k = 0; k < 8; k++) {
        Circle &c = o.circles[k];
        int t = o.anim_t < c.d ? o.anim_t : c.d;
        num x = easing_cubic_out(t, o.player_x, c.x - o.player_x, c.d);
        num y = easing_cubic_out(t, o.player_y, c.y - o.player_y, c.d);
        num r = easing_cubic_out(t, 0, c.r, c.d);
        circfill(x, o.cam_y + y, r, 8);
        circfill(x, o.cam_y + y, r, 8);
    }
    //text
    if (o.anim_t >= 10) {
        p8::printInverted("game over", toi(o.x - 17), toi(o.cam_y + o.y - 8), 2);    // "\^igame over\n"
        line(o.x - 19, o.cam_y + o.y - 8, o.x - 19, o.cam_y + o.y - 4, 2);
        line(o.x + 19, o.cam_y + o.y - 8, o.x + 19, o.cam_y + o.y - 4, 2);
        if (o.highscore) {
            print("new", o.x - 6, o.cam_y + o.y + 2);
            print("highscore", o.x - 18, o.cam_y + o.y + 8);
            printn(o.score, 64 - toi(flr(num(tostrLen(o.score) * 4) / 2)), o.cam_y + o.y + 16);
        } else {
            print("score", o.x - 9, o.cam_y + o.y + 7);
            printn(o.score, 64 - toi(flr(num(tostrLen(o.score) * 4) / 2)), o.cam_y + o.y + 15);
        }
    }
}

// ---------------------------------------------------------------------------
// trap
// ---------------------------------------------------------------------------
static const AnimDef TRAP_ANIMS[] = { { 3, 2, { 74, 77 } } };
struct Trap {
    Anim anim;
    num speed_start, speed_top;
    int speed_top_treshold;
    num offset;
    int offset_min;
    Pos pos;
    Box hitbox;
};
static const uint8_t TRAP_TILES[12] = { 122, 123, 124, 125, 123, 124, 125, 123, 124, 125, 123, 126 };
static const uint8_t TRAP_FILL[12] = { 112, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 114 };

static void trap_new(Trap &o) {
    o.speed_start = num(0.5);
    o.speed_top = num(1.7);
    o.speed_top_treshold = 200;     //game score points when we reach top speed
    o.offset = 128;
    o.offset_min = 120;
    o.speed_top = o.speed_top - o.speed_start;     //exclude start from top speed to ensure max is correct
    //collidable
    o.pos.x = 16; o.pos.y = 32767;
    o.hitbox = { 0, 6, 95, 9 };
    //animatable
    anim_new(o.anim, TRAP_ANIMS);
}

static void trap_update(Trap &o, num camera_y, int score, bool) {
    update_animation(o.anim);
    num speed = mn(num(score) / o.speed_top_treshold, 1);    //0 to 1
    num shift = o.speed_start + o.speed_top * speed;
    o.pos.y = mx(mn(o.pos.y - shift, camera_y + o.offset), camera_y + 72);
    //animate in the trap when the game starts
    o.offset = mx(o.offset - num(0.2), num(o.offset_min));
}

static void trap_draw(Trap &o) {
    for (int i = 0; i <= 3; i++) spr(o.anim.sprite, 16 + i * 24, o.pos.y, 3, 3);
    palt(0, false);
    palt(11, true);
    for (int i = 1; i <= 12; i++) {
        spr(TRAP_TILES[i - 1], 8 + i * 8, o.pos.y + 16, 1, 1);
        spr(TRAP_TILES[i - 1], 8 + i * 8, o.pos.y + 16, 1, 1);
        spr(TRAP_FILL[i - 1], 8 + i * 8, o.pos.y + 24, 1, 1);
        spr(TRAP_FILL[i - 1], 8 + i * 8, o.pos.y + 32, 1, 1);
        spr(TRAP_FILL[i - 1], 8 + i * 8, o.pos.y + 40, 1, 1);
        spr(TRAP_FILL[i - 1], 8 + i * 8, o.pos.y + 48, 1, 1);
    }
    palt();
}

// ---------------------------------------------------------------------------
// transition
// ---------------------------------------------------------------------------
static const AnimDef TRANSITION_ANIMS[] = { { 1, 2, { 68, 71 } } };
enum { T_NIL, T_START, T_FINISH };
struct Transition {
    Anim anim;
    num cam_y;
    int saw_size;
    int transition;
    num y;
    int offset, anim_t, anim_d;
};

static void transition_finish(Transition &o) {
    sfx(24);
    o.transition = T_FINISH;
    o.y = 0;
    o.anim_t = 0;
}

static void transition_new(Transition &o) {
    o.cam_y = 0;
    o.saw_size = 54;
    o.transition = T_NIL;
    o.y = 0;
    o.offset = 128 + toi(ceil_(num(o.saw_size) / 2));
    o.anim_t = 0;
    o.anim_d = 20;
    //animatable
    anim_new(o.anim, TRANSITION_ANIMS);
    transition_finish(o);
}

static void transition_update(Transition &o, num cam_y) {
    if (o.transition == T_NIL) return;
    o.cam_y = cam_y;
    update_animation(o.anim);
    o.anim_t = o.anim_t + 1 < o.anim_d ? o.anim_t + 1 : o.anim_d;
    if (o.transition == T_START) o.y = o.offset - easing_cubic_out(o.anim_t, 0, o.offset, o.anim_d);
    else o.y = -easing_cubic_out(o.anim_t, 0, o.offset, o.anim_d);
}

static void transition_draw(Transition &o) {
    static const uint8_t BLACK[16] = { 0 };
    if (o.transition == T_NIL) return;
    p8::palTable(BLACK);
    palt(0, false);
    palt(11, true);
    //rect
    rectfill(0, o.cam_y + o.y, 128, o.cam_y + o.y + 128, 0);
    //saws
    int sx = o.anim.sprite == 68 ? 32 : 56;
    num posy = o.cam_y + o.y + (o.transition == T_START ? 0 : 116) - toi(flr(num(o.saw_size - 12) / 2));
    sspr(sx, 32, 24, 24, -4, posy, o.saw_size, o.saw_size);
    sspr(sx, 32, 24, 24, 40, posy, o.saw_size, o.saw_size);
    sspr(sx, 32, 24, 24, 80, posy, o.saw_size, o.saw_size);
    palt();
    pal();
    //end transition
    if (o.anim_t >= o.anim_d) o.transition = T_NIL;
}

static void transition_start(Transition &o) {
    sfx(23);
    o.transition = T_START;
    o.y = o.offset;
    o.anim_t = 0;
}

// ---------------------------------------------------------------------------
// score
// ---------------------------------------------------------------------------
struct Score { int score; num x, y; int offset_y, width, height, anim_t, anim_d; num anim_y; int anim_target; };

static void score_new(Score &o) {
    o.score = 0;
    o.x = 0;
    o.y = 0;
    o.offset_y = 4;
    o.width = 0;
    o.height = 6;
    o.anim_t = 0;
    o.anim_d = 24;
    o.anim_y = 0;
    o.anim_target = 10;
}

static void score_update(Score &o, num cam_y, bool jumped) {
    if (jumped) o.score += 1;
    o.width = tostrLen(o.score) * 4;
    o.x = 64 - ceil_(num(o.width) / 2);
    o.y = cam_y + o.offset_y;
    //slide animation
    if (o.anim_t < o.anim_d) {
        o.anim_t += 1;
        o.anim_y = o.anim_target - easing_cubic_out(o.anim_t, 0, o.anim_target, o.anim_d);
    }
}

static void score_draw(Score &o) {
    num y = o.y - o.anim_y;
    palt(0, false);
    palt(11, true);
    rectfill(o.x, y, o.x + o.width, y + o.height, 0);
    spr(115, o.x - 8, y, 1, 1, true);
    spr(115, o.x + o.width + 1, y);
    palt();
    printn(o.score, o.x + 1, y + 1, 15);
}

// ---------------------------------------------------------------------------
// game_scene
// ---------------------------------------------------------------------------
enum { S_INITIAL, S_PLAYING, S_WILL_DIE, S_DEAD };
struct GameScene {
    int tile_width, tile_height;
    int state, buttons_cooldown;
    bool transition_scene;
    Background background;
    Columns columns;
    Trap trap;
    Player player;
    Score score;
    bool hasInitial;                // game_initial ~= nil
    GameInitial game_initial;
    GameOver game_over;
    Transition transition;
    Camera camera;
};
static GameScene gs;

static void game_scene_new() {
    GameScene &o = gs;
    //constants
    o.tile_width = 16;
    o.tile_height = 24;
    //init
    o.state = S_INITIAL;
    o.buttons_cooldown = 0;
    o.transition_scene = false;
    //layers
    background_new(o.background);
    columns_new(o.columns);
    trap_new(o.trap);
    player_new(o.player, o.tile_height);
    score_new(o.score);
    o.hasInitial = true;
    game_initial_new(o.game_initial);
    game_over_new(o.game_over);
    transition_new(o.transition);
    camera_new(o.camera, o.player.pos.y);
    //music
    music(0, 600);
}

// `1` if right button is pressed, `0` if left, `-1` (nil) if none
static int game_scene_button_action_right(GameScene &o) {
    if (o.state == S_WILL_DIE || o.transition_scene) return -1;
    if (btnp(0) || btnp(4)) return 0;
    else if (btnp(1) || btnp(5)) return 1;
    return -1;
}

static void game_scene_update() {
    GameScene &o = gs;
    //temporary overflow fix
    if (o.player.tile_pos == -1300 && o.player.jump_tile_pos == 0) {
        columns_shift(o.columns, o.tile_height, o.camera.y);
        o.player.tile_pos = 1300;
        o.camera.y += num(2600) * o.tile_height;
        o.trap.pos.y += num(2600) * o.tile_height;
    }
    //get button action
    int action_right = game_scene_button_action_right(o);
    if (o.buttons_cooldown > 0) {
        o.buttons_cooldown -= 1;
        action_right = -1;
    }
    //restart game
    if (o.state == S_DEAD && action_right >= 0) {
        o.transition_scene = true;
        transition_start(o.transition);
        return;
    }
    //update layers
    if (o.state != S_DEAD) player_update(o.player, action_right, o.trap.pos.y, o.state == S_WILL_DIE);
    if (o.state != S_INITIAL) trap_update(o.trap, o.camera.y, o.score.score, o.state == S_DEAD);
    camera_update(o.camera, o.player.pos.y);
    columns_update(o.columns, o.tile_width, o.tile_height, o.camera.y, o.player, action_right, o.state == S_DEAD);
    background_update(o.background, o.camera.y);
    //update state
    if (action_right >= 0 && columns_will_collide(o.columns, o.player)) {
        o.state = S_WILL_DIE;
        music(-1, 300);
        sfx(21);
    }
    if (o.state == S_INITIAL && action_right >= 0) {
        o.state = S_PLAYING;
        o.game_initial.anim = true;     // game_initial:hide()
        music(2);
        sfx(26);
    } else if (o.state != S_DEAD && (columns_collide(o.columns, o.player) ||
                                     collide(o.trap.pos, o.trap.hitbox, o.player.pos, o.player.hitbox))) {
        if (o.state != S_WILL_DIE) sfx(21);
        sfx(27);
        o.buttons_cooldown = 22;
        o.state = S_DEAD;
        music(-1, 300);
    }
    //update score
    if (o.state != S_INITIAL && o.state != S_DEAD) score_update(o.score, o.camera.y, action_right >= 0);
    //game initial
    if (o.hasInitial && o.game_initial.anim_finished) o.hasInitial = false;
    if (o.hasInitial) game_initial_update(o.game_initial, o.camera.y);
    //game over
    if (o.state == S_DEAD) game_over_update(o.game_over, o.camera.y, o.player.pos, o.score.score);
    //transition
    if (o.state == S_DEAD && o.transition_scene && o.transition.transition == T_NIL) {
        game_scene_new();       // current_scene=game_scene:new(); the old scene's
        return;                 // transition:update() that follows returns at once (its transition is nil)
    }
    transition_update(o.transition, o.camera.y);
}

static void game_scene_draw() {
    static const uint8_t DEAD[16] = { 0, 1, 3, 2, 5, 13, 6, 8, 4, 10, 11, 12, 1, 14, 9, 0 };
    GameScene &o = gs;
    p8::cls();
    //fade out game scene if the player is dead
    if (o.state == S_DEAD) p8::palTable(DEAD);
    camera_draw(o.camera);
    background_draw(o.background);
    columns_draw(o.columns, o.tile_width, o.tile_height);
    trap_draw(o.trap);
    if (o.state != S_INITIAL && o.state != S_DEAD) score_draw(o.score);
    if (o.hasInitial) game_initial_draw(o.game_initial);
    if (o.state != S_DEAD && o.transition.anim_t > 0) player_draw(o.player);
    pal();
    //game over
    if (o.state == S_DEAD) game_over_draw(o.game_over);
    //transition
    transition_draw(o.transition);
}

// ---------------------------------------------------------------------------
void boot() {
    // poke(0X5F5C, 255): no btnp repeat (btnp() here never repeats)
    cdata::begin();             // cartdata("bas_data_1")
    // _init()
    current_scene = 0;
    splash_new();
}

void update() {
    pressed = buttons & (uint8_t)~lastButtons;
    lastButtons = buttons;
    if (current_scene == 0) splash_update();
    else game_scene_update();
}

void draw() {
    if (current_scene == 0) splash_draw();
    else game_scene_draw();
}

State state() {
    State s = {};
    s.scene = current_scene;
    s.splashCount = splash.count;
    s.splashFading = splash.fading;
    if (current_scene == 1) {
        GameScene &o = gs;
        s.state = o.state;
        s.cooldown = o.buttons_cooldown;
        s.transitionScene = o.transition_scene;
        s.playerX = o.player.pos.x;
        s.playerY = o.player.pos.y;
        s.jumpTilePos = o.player.jump_tile_pos;
        s.camY = o.camera.y;
        s.trapY = o.trap.pos.y;
        s.trapOffset = o.trap.offset;
        s.tilePos = o.player.tile_pos;
        s.jumping = o.player.jumping;
        s.rightWall = o.player.right_wall;
        s.playerSprite = o.player.anim.sprite;
        s.score = o.score.score;
        for (auto &t : o.columns.tiles) {
            if (!t.used) continue;
            if (t.hasSaw) { s.sawCount++; if (t.saw.left) s.sawLefts += t.idx; }
            if (t.hasJfx) { s.jfxCount++; s.jfxSprites += t.jfx.anim.sprite == NIL ? -1 : t.jfx.anim.sprite; }
        }
        s.particles = o.game_over.np;
        s.gameOverAnim = o.game_over.anim_t;
        s.transitionAnim = o.transition.anim_t * 10 + o.transition.transition;
        s.initialGone = !o.hasInitial;
        s.goHighscore = o.game_over.highscore;
        s.best = toi(cdata::dget(0));
    }
    return s;
}

}  // namespace game
