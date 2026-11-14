// =====================================================================
//  ПОЗЫВНОЙ: ГРОМ — военный шутер от первого лица
//  Один файл, C++14 + SDL2. Программный 3D-рендер, процедурный звук,
//  встроенный кириллический шрифт. Без внешних ассетов.
//  Сборка: g++ -std=c++14 -O2 grom.cpp $(sdl2-config --cflags --libs) -o grom
// =====================================================================
#if defined(__has_include)
#  if __has_include(<SDL.h>)
#    include <SDL.h>
#  else
#    include <SDL2/SDL.h>
#  endif
#else
#  include <SDL.h>
#endif
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <vector>
#include <string>
#include <algorithm>

// ------------------------------------------------------------ математика
static const float PI_F = 3.14159265358979f;
static const float DEG = PI_F / 180.0f;

struct V3 {
    float x, y, z;
    V3() : x(0), y(0), z(0) {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return V3(x + o.x, y + o.y, z + o.z); }
    V3 operator-(const V3& o) const { return V3(x - o.x, y - o.y, z - o.z); }
    V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
    V3 operator/(float s) const { return V3(x / s, y / s, z / s); }
    V3 operator-() const { return V3(-x, -y, -z); }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
static inline V3 norm(V3 a) { float l = len(a); return l > 1e-6f ? a / l : V3(0, 0, 0); }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline V3 lerpv(V3 a, V3 b, float t) { return a + (b - a) * t; }
static inline float wrapPi(float a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }
static inline float dist2D(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static inline V3 dirYP(float yaw, float pitch) { return V3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)); }
static inline float yawTo(V3 f, V3 t) { return atan2f(t.x - f.x, t.z - f.z); }
static inline float approach(float v, float target, float step) {
    if (v < target) return std::min(v + step, target);
    return std::max(v - step, target);
}

static uint32_t g_rng = 0x2545F491u;
static inline uint32_t rnd() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static inline float frand() { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static inline float frange(float a, float b) { return a + (b - a) * frand(); }
static inline uint32_t hash2(int x, int z) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}

// ------------------------------------------------------------ цвета
static inline uint32_t RGBc(int r, int g, int b) {
    return 0xFF000000u | ((uint32_t)clampi(r, 0, 255) << 16) | ((uint32_t)clampi(g, 0, 255) << 8) | (uint32_t)clampi(b, 0, 255);
}
static inline uint32_t ARGBc(int a, int r, int g, int b) {
    return ((uint32_t)clampi(a, 0, 255) << 24) | ((uint32_t)clampi(r, 0, 255) << 16) | ((uint32_t)clampi(g, 0, 255) << 8) | (uint32_t)clampi(b, 0, 255);
}
static inline uint32_t shadeC(uint32_t c, float s) {
    return RGBc((int)(((c >> 16) & 255) * s), (int)(((c >> 8) & 255) * s), (int)((c & 255) * s));
}
static inline uint32_t mixC(uint32_t a, uint32_t b, float t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGBc(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t), ab + (int)((bb - ab) * t));
}
static inline uint32_t withA(uint32_t c, int a) { return (c & 0xFFFFFFu) | ((uint32_t)clampi(a, 0, 255) << 24); }

// ------------------------------------------------------------ разрешения
static const int UIH = 360, UIW_MAX = 1000;
static int UIW = 640;   // ширина холста интерфейса: 640 для 16:9, шире — для вытянутых экранов телефонов
static int RW = 320, RH = 180;

// =====================================================================
//  ШРИФТ 5x7 (латиница, цифры, кириллица). Строчные выводятся заглавными.
// =====================================================================
struct GlyphSrc { int cp; const char* s; };
static const GlyphSrc kGlyphSrc[] = {
    {' ', "..... ..... ..... ..... ..... ..... ....."},
    {'!', "..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#.."},
    {'"', ".#.#. .#.#. ..... ..... ..... ..... ....."},
    {'#', ".#.#. .#.#. ##### .#.#. ##### .#.#. .#.#."},
    {'%', "##..# ##..# ...#. ..#.. .#... #..## #..##"},
    {'\'', "..#.. ..#.. ..... ..... ..... ..... ....."},
    {'(', "...#. ..#.. .#... .#... .#... ..#.. ...#."},
    {')', ".#... ..#.. ...#. ...#. ...#. ..#.. .#..."},
    {'*', "..... #.#.# .###. ##### .###. #.#.# ....."},
    {'+', "..... ..#.. ..#.. ##### ..#.. ..#.. ....."},
    {',', "..... ..... ..... ..... .##.. ..#.. .#..."},
    {'-', "..... ..... ..... .###. ..... ..... ....."},
    {'.', "..... ..... ..... ..... ..... .##.. .##.."},
    {'/', "....# ...#. ...#. ..#.. .#... .#... #...."},
    {'0', ".###. #...# #..## #.#.# ##..# #...# .###."},
    {'1', "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###."},
    {'2', ".###. #...# ....# ...#. ..#.. .#... #####"},
    {'3', "##### ...#. ..#.. ...#. ....# #...# .###."},
    {'4', "...#. ..##. .#.#. #..#. ##### ...#. ...#."},
    {'5', "##### #.... ####. ....# ....# #...# .###."},
    {'6', "..##. .#... #.... ####. #...# #...# .###."},
    {'7', "##### ....# ...#. ..#.. .#... .#... .#..."},
    {'8', ".###. #...# #...# .###. #...# #...# .###."},
    {'9', ".###. #...# #...# .#### ....# ...#. .##.."},
    {':', "..... .##.. .##.. ..... .##.. .##.. ....."},
    {';', "..... .##.. .##.. ..... .##.. ..#.. .#..."},
    {'<', "...#. ..#.. .#... #.... .#... ..#.. ...#."},
    {'=', "..... ..... ##### ..... ##### ..... ....."},
    {'>', ".#... ..#.. ...#. ....# ...#. ..#.. .#..."},
    {'?', ".###. #...# ....# ...#. ..#.. ..... ..#.."},
    {'A', ".###. #...# #...# ##### #...# #...# #...#"},
    {'B', "####. #...# #...# ####. #...# #...# ####."},
    {'C', ".###. #...# #.... #.... #.... #...# .###."},
    {'D', "###.. #..#. #...# #...# #...# #..#. ###.."},
    {'E', "##### #.... #.... ####. #.... #.... #####"},
    {'F', "##### #.... #.... ####. #.... #.... #...."},
    {'G', ".###. #...# #.... #.### #...# #...# .####"},
    {'H', "#...# #...# #...# ##### #...# #...# #...#"},
    {'I', ".###. ..#.. ..#.. ..#.. ..#.. ..#.. .###."},
    {'J', "..### ...#. ...#. ...#. ...#. #..#. .##.."},
    {'K', "#...# #..#. #.#.. ##... #.#.. #..#. #...#"},
    {'L', "#.... #.... #.... #.... #.... #.... #####"},
    {'M', "#...# ##.## #.#.# #.#.# #...# #...# #...#"},
    {'N', "#...# #...# ##..# #.#.# #..## #...# #...#"},
    {'O', ".###. #...# #...# #...# #...# #...# .###."},
    {'P', "####. #...# #...# ####. #.... #.... #...."},
    {'Q', ".###. #...# #...# #...# #.#.# #..#. .##.#"},
    {'R', "####. #...# #...# ####. #.#.. #..#. #...#"},
    {'S', ".#### #.... #.... .###. ....# ....# ####."},
    {'T', "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#.."},
    {'U', "#...# #...# #...# #...# #...# #...# .###."},
    {'V', "#...# #...# #...# #...# #...# .#.#. ..#.."},
    {'W', "#...# #...# #...# #.#.# #.#.# #.#.# .#.#."},
    {'X', "#...# #...# .#.#. ..#.. .#.#. #...# #...#"},
    {'Y', "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#.."},
    {'Z', "##### ....# ...#. ..#.. .#... #.... #####"},
    {'[', ".###. .#... .#... .#... .#... .#... .###."},
    {']', ".###. ...#. ...#. ...#. ...#. ...#. .###."},
    {'_', "..... ..... ..... ..... ..... ..... #####"},
    {0xAB, "..... ..#.# .#.#. #.#.. .#.#. ..#.# ....."},
    {0xBB, "..... #.#.. .#.#. ..#.# .#.#. #.#.. ....."},
    {0xB0, ".##.. #..#. .##.. ..... ..... ..... ....."},
    {0x2014, "..... ..... ..... ##### ..... ..... ....."},
    {0x2013, "..... ..... ..... .###. ..... ..... ....."},
    {0x2026, "..... ..... ..... ..... ..... ..... #.#.#"},
    {0x401, ".#.#. ##### #.... ####. #.... #.... #####"},
    {0x411, "##### #.... #.... ####. #...# #...# ####."},
    {0x413, "##### #.... #.... #.... #.... #.... #...."},
    {0x414, "..##. .#.#. .#.#. .#.#. .#.#. ##### #...#"},
    {0x416, "#.#.# #.#.# .###. ..#.. .###. #.#.# #.#.#"},
    {0x417, ".###. #...# ....# ..##. ....# #...# .###."},
    {0x418, "#...# #...# #..## #.#.# ##..# #...# #...#"},
    {0x419, "..#.. #...# #..## #.#.# ##..# #...# #...#"},
    {0x41B, "..### .#..# .#..# .#..# .#..# .#..# #...#"},
    {0x41F, "##### #...# #...# #...# #...# #...# #...#"},
    {0x423, "#...# #...# #...# .#### ....# #...# .###."},
    {0x424, "..#.. .###. #.#.# #.#.# #.#.# .###. ..#.."},
    {0x426, "#..#. #..#. #..#. #..#. #..#. ##### ....#"},
    {0x427, "#...# #...# #...# .#### ....# ....# ....#"},
    {0x428, "#.#.# #.#.# #.#.# #.#.# #.#.# #.#.# #####"},
    {0x429, "#.#.# #.#.# #.#.# #.#.# #.#.# ##### ....#"},
    {0x42A, "##... .#... .#... .###. .#..# .#..# .###."},
    {0x42B, "#...# #...# #...# ##..# #.#.# #.#.# ##..#"},
    {0x42C, "#.... #.... #.... ####. #...# #...# ####."},
    {0x42D, ".###. #...# ....# .#### ....# #...# .###."},
    {0x42E, "#..#. #.#.# #.#.# ###.# #.#.# #.#.# #..#."},
    {0x42F, ".#### #...# #...# .#### ..#.# .#..# #...#"},
};
static uint8_t g_gl[128][7];
static int g_glN = 0;
static int16_t g_gmap[0x2030];

static void fontInit() {
    for (int i = 0; i < 0x2030; i++) g_gmap[i] = -1;
    int n = (int)(sizeof(kGlyphSrc) / sizeof(kGlyphSrc[0]));
    for (int i = 0; i < n && g_glN < 128; i++) {
        const char* s = kGlyphSrc[i].s;
        int row = 0, col = 0; uint8_t bits[7] = {0, 0, 0, 0, 0, 0, 0};
        for (; *s && row < 7; s++) {
            if (*s == '#' || *s == '.') {
                if (*s == '#') bits[row] |= (uint8_t)(1 << (4 - col));
                if (++col == 5) { col = 0; row++; }
            }
        }
        memcpy(g_gl[g_glN], bits, 7);
        g_gmap[kGlyphSrc[i].cp] = (int16_t)g_glN++;
    }
    static const int alias[][2] = {{0x410, 'A'}, {0x412, 'B'}, {0x415, 'E'}, {0x41A, 'K'}, {0x41C, 'M'},
                                   {0x41D, 'H'}, {0x41E, 'O'}, {0x420, 'P'}, {0x421, 'C'}, {0x422, 'T'}, {0x425, 'X'}};
    for (auto& a : alias) g_gmap[a[0]] = g_gmap[a[1]];
}
static int glyphFor(int cp) {
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    else if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;
    else if (cp == 0x451) cp = 0x401;
    if (cp < 0 || cp >= 0x2030) return g_gmap[(int)'?'];
    int g = g_gmap[cp];
    return g < 0 ? g_gmap[(int)'?'] : g;
}
static int utf8Next(const char*& s) {
    unsigned char c = (unsigned char)*s;
    if (!c) return 0;
    if (c < 0x80) { s++; return c; }
    if ((c >> 5) == 6 && s[1]) { int cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); s += 2; return cp; }
    if ((c >> 4) == 14 && s[1] && s[2]) { int cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); s += 3; return cp; }
    s++; return '?';
}
static int utf8Len(const char* s) { int n = 0; while (utf8Next(s)) n++; return n; }

// =====================================================================
//  UI-слой 640x360 с альфой
// =====================================================================
static uint32_t g_ui[UIW_MAX * UIH];
static inline void uiBlend(uint32_t& d, uint32_t s) {
    uint32_t sa = s >> 24;
    if (sa == 0) return;
    uint32_t da = d >> 24;
    if (sa == 255 || da == 0) { d = s; return; }
    uint32_t ws = sa * 255, wd = da * (255 - sa), tot = ws + wd;
    uint32_t oa = sa + (da * (255 - sa)) / 255;
    uint32_t r = (((s >> 16) & 255) * ws + ((d >> 16) & 255) * wd) / tot;
    uint32_t g = (((s >> 8) & 255) * ws + ((d >> 8) & 255) * wd) / tot;
    uint32_t b = ((s & 255) * ws + (d & 255) * wd) / tot;
    d = (oa << 24) | (r << 16) | (g << 8) | b;
}
static int g_uiOX = 0;   // сдвиг экранов, свёрстанных под 640, к центру широкого экрана
static inline void uiPix(int x, int y, uint32_t c) {
    x += g_uiOX;
    if ((unsigned)x >= (unsigned)UIW || (unsigned)y >= (unsigned)UIH) return;
    uiBlend(g_ui[y * UIW + x], c);
}
static void uiRect(int x, int y, int w, int h, uint32_t c) {
    x += g_uiOX;
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(UIW, x + w), y1 = std::min(UIH, y + h);
    for (int j = y0; j < y1; j++) { uint32_t* row = g_ui + j * UIW; for (int i = x0; i < x1; i++) uiBlend(row[i], c); }
}
static void uiFrame(int x, int y, int w, int h, uint32_t c, int t = 1) {
    uiRect(x, y, w, t, c); uiRect(x, y + h - t, w, t, c);
    uiRect(x, y + t, t, h - 2 * t, c); uiRect(x + w - t, y + t, t, h - 2 * t, c);
}
static void uiCircle(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++) {
        int w = (int)sqrtf((float)(r * r - j * j));
        uiRect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}
static void uiRing(int cx, int cy, int r, int th, uint32_t c) {
    int r2o = r * r, ri = std::max(0, r - th), r2i = ri * ri;
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) { int d = i * i + j * j; if (d <= r2o && d >= r2i) uiPix(cx + i, cy + j, c); }
}
static void uiLine(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 4000; guard++) {
        uiPix(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void uiArc(int cx, int cy, int r, float a0, float a1, int th, uint32_t c) {
    // угол 0 — вверх, по часовой
    int steps = (int)(fabsf(a1 - a0) * r) + 2;
    for (int s = 0; s <= steps; s++) {
        float a = a0 + (a1 - a0) * s / steps;
        for (int k = 0; k < th; k++) {
            float rr = (float)(r - k);
            uiPix(cx + (int)lrintf(sinf(a) * rr), cy - (int)lrintf(cosf(a) * rr), c);
        }
    }
}
static void uiText(int x, int y, const char* s, uint32_t col, int sc = 1, bool shadow = true) {
    if (shadow) uiText(x + sc, y + sc, s, ARGBc((int)(col >> 24) * 3 / 4, 0, 0, 0), sc, false);
    int cx = x;
    const char* p = s;
    for (;;) {
        int cp = utf8Next(p);
        if (!cp) break;
        if (cp == '\n') { cx = x; y += 9 * sc; continue; }
        int g = glyphFor(cp);
        if (g >= 0) {
            for (int r = 0; r < 7; r++) {
                uint8_t bits = g_gl[g][r];
                for (int c = 0; c < 5; c++) if (bits & (1 << (4 - c))) {
                    if (sc == 1) uiPix(cx + c, y + r, col); else uiRect(cx + c * sc, y + r * sc, sc, sc, col);
                }
            }
        }
        cx += 6 * sc;
    }
}
static int uiTextW(const char* s, int sc = 1) { int n = utf8Len(s); return n > 0 ? (n * 6 - 1) * sc : 0; }
static void uiTextC(int cx, int y, const char* s, uint32_t col, int sc = 1) { uiText(cx - uiTextW(s, sc) / 2, y, s, col, sc); }
static void uiTextR(int rx, int y, const char* s, uint32_t col, int sc = 1) { uiText(rx - uiTextW(s, sc), y, s, col, sc); }

static std::vector<std::string> wrapText(const std::string& s, int maxW, int sc = 1) {
    std::vector<std::string> out;
    int maxC = std::max(4, (maxW / sc + 1) / 6);
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string cand = line.empty() ? word : line + " " + word;
        if (utf8Len(cand.c_str()) > maxC && !line.empty()) { out.push_back(line); line = word; }
        else line = cand;
        word.clear();
    };
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); out.push_back(line); line.clear(); }
        else word += c;
    }
    flushWord();
    if (!line.empty()) out.push_back(line);
    return out;
}
static std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
static std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap; va_start(ap, f); vsnprintf(buf, sizeof(buf), f, ap); va_end(ap);
    return std::string(buf);
}

// =====================================================================
//  ПРОЦЕДУРНЫЙ ЗВУК
// =====================================================================
enum SndType { ST_SHOT, ST_EXPLO, ST_STEP, ST_RADIO, ST_CLICK, ST_BEEP, ST_WHIZ, ST_IMPACT, ST_BARK, ST_JET, ST_THROW, ST_BOLT, ST_HURT };
struct Voice {
    int type; bool on;
    float t, dur, vol, gl, gr, echo;
    float p0, p1, p2, p3;
    float e1, e2, e3, k1, k2, k3;
    float s1, s2, ph;
};
struct AmbState { float ring, rain, wind, breath, heli, heliRate, echoAmt, master, heart; };
static const int MAXV = 40;
static Voice g_v[MAXV];
static SDL_AudioDeviceID g_adev = 0;
static float g_arate = 22050.0f;
static AmbState g_amb = {0, 0, 0, 0, 0, 5, 0.3f, 0.8f, 0};   // игровая сторона
static AmbState g_ambA = {0, 0, 0, 0, 0, 5, 0.3f, 0.8f, 0};  // копия аудиопотока
static uint32_t g_arng = 0x9E3779B9u;
static const int ECHO_N = 16384;
static float g_echoL[ECHO_N], g_echoR[ECHO_N];
static int g_echoPos = 0;
static float g_aState[16];
static inline float anoise() { g_arng ^= g_arng << 13; g_arng ^= g_arng >> 17; g_arng ^= g_arng << 5; return (float)(int32_t)g_arng * (1.0f / 2147483648.0f); }

static float synthVoice(Voice& v, float dt) {
    float s = 0, t = v.t;
    switch (v.type) {
    case ST_SHOT: {
        float n = anoise();
        v.s1 += (n - v.s1) * v.p1;
        float f = v.p2 * (1.0f + 2.5f * v.e3);
        v.ph += f * dt; if (v.ph > 1) v.ph -= 1;
        float th = sinf(v.ph * 2 * PI_F) * v.e2;
        s = v.s1 * v.e1 * 1.1f + th * 0.75f + (t < 0.004f ? n * v.p3 : 0.0f);
        v.e1 *= v.k1; v.e2 *= v.k2; v.e3 *= v.k3;
    } break;
    case ST_EXPLO: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.07f; v.s2 += (v.s1 - v.s2) * 0.3f;
        v.ph += 42.0f * dt; if (v.ph > 1) v.ph -= 1;
        s = v.s2 * v.e1 * 3.0f + sinf(v.ph * 2 * PI_F) * v.e2 * 0.9f + (t < 0.02f ? n * 0.8f : 0.0f);
        v.e1 *= v.k1; v.e2 *= v.k2;
    } break;
    case ST_STEP: {
        float n = anoise();
        v.s1 += (n - v.s1) * v.p1;
        s = v.s1 * v.e1;
        if (v.p2 > 0) { v.ph += v.p2 * dt; if (v.ph > 1) v.ph -= 1; s += sinf(v.ph * 2 * PI_F) * v.e2 * 0.3f; }
        v.e1 *= v.k1; v.e2 *= v.k2;
    } break;
    case ST_RADIO: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.55f; v.s2 += (v.s1 - v.s2) * 0.08f;
        float a = (t < 0.15f) ? 1.0f : 0.0f;
        s = (v.s1 - v.s2) * a * 0.9f;
        if (t > 0.15f && t < 0.22f) { v.ph += 1400.0f * dt; s += (v.ph - floorf(v.ph) < 0.5f ? 0.25f : -0.25f); }
    } break;
    case ST_CLICK: {
        float n = anoise();
        float a = (t < 0.01f) ? 1.0f : ((t > v.p0 && t < v.p0 + 0.012f) ? 0.8f : 0.0f);
        v.s1 += (n - v.s1) * 0.8f;
        s = (n - v.s1) * a;
    } break;
    case ST_BEEP: {
        v.ph += v.p2 * dt; if (v.ph > 1) v.ph -= 1;
        float a = clampf(t * 200.0f, 0, 1) * clampf((v.dur - t) * 200.0f, 0, 1);
        s = sinf(v.ph * 2 * PI_F) * a * 0.5f;
    } break;
    case ST_WHIZ: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.3f;
        float a = sinf(clampf(t / v.dur, 0, 1) * PI_F);
        v.ph += (2200.0f - 1400.0f * t / v.dur) * dt; if (v.ph > 1) v.ph -= 1;
        s = ((n - v.s1) * 0.7f + sinf(v.ph * 2 * PI_F) * 0.3f) * a * a;
    } break;
    case ST_IMPACT: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.45f;
        s = v.s1 * v.e1;
        if (v.p2 > 0) { v.ph += v.p2 * dt; if (v.ph > 1) v.ph -= 1; s += sinf(v.ph * 2 * PI_F) * v.e2 * 0.4f; }
        v.e1 *= v.k1; v.e2 *= v.k2;
    } break;
    case ST_BARK: {
        float lt = fmodf(t, 0.32f);
        float a = lt < 0.16f ? sinf(lt / 0.16f * PI_F) : 0.0f;
        float f = 330.0f - lt * 900.0f;
        v.ph += f * dt; if (v.ph > 1) v.ph -= 1;
        s = ((v.ph * 2 - 1) * 0.6f + anoise() * 0.4f) * a;
    } break;
    case ST_JET: {
        float n = anoise();
        float x = t / v.dur;
        float a = expf(-powf((x - 0.45f) * 4.0f, 2.0f));
        float c = 0.05f + 0.4f * a;
        v.s1 += (n - v.s1) * c;
        s = v.s1 * a * 2.0f;
    } break;
    case ST_THROW: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.2f;
        s = v.s1 * sinf(clampf(t / v.dur, 0, 1) * PI_F) * 0.8f;
    } break;
    case ST_BOLT: {
        float n = anoise();
        float a = (t < 0.015f) ? 1.0f : ((t > 0.28f && t < 0.3f) ? 1.0f : 0.0f);
        v.s1 += (n - v.s1) * 0.7f;
        v.ph += 1900.0f * dt; if (v.ph > 1) v.ph -= 1;
        float ring = (t < 0.1f ? expf(-t * 40) : (t > 0.28f ? expf(-(t - 0.28f) * 40) : 0.0f));
        s = (n - v.s1) * a * 0.9f + sinf(v.ph * 2 * PI_F) * ring * 0.2f;
    } break;
    case ST_HURT: {
        float n = anoise();
        v.s1 += (n - v.s1) * 0.08f;
        v.ph += 70.0f * dt; if (v.ph > 1) v.ph -= 1;
        s = (v.s1 * 1.5f + sinf(v.ph * 2 * PI_F) * 0.8f) * v.e1;
        v.e1 *= v.k1;
    } break;
    default: break;
    }
    v.t += dt;
    if (v.t >= v.dur) v.on = false;
    return s * v.vol;
}

static void SDLCALL audioCB(void* ud, Uint8* stream, int len) {
    (void)ud;
    int16_t* out = (int16_t*)stream;
    int frames = len / 4;
    float dt = 1.0f / g_arate;
    AmbState& A = g_ambA;
    int dl = (int)(0.23f * g_arate), dr = (int)(0.31f * g_arate);
    float* st = g_aState;
    for (int i = 0; i < frames; i++) {
        float L = 0, R = 0, E = 0;
        for (int k = 0; k < MAXV; k++) {
            Voice& v = g_v[k];
            if (!v.on) continue;
            float s = synthVoice(v, dt);
            L += s * v.gl; R += s * v.gr; E += s * v.echo;
        }
        // окружение
        if (A.rain > 0.001f) {
            float n = anoise(); st[0] += (n - st[0]) * 0.6f; st[1] += (st[0] - st[1]) * 0.03f;
            float r = (st[0] - st[1]) * A.rain * 0.35f;
            L += r; R += r * 0.9f;
        }
        if (A.wind > 0.001f) {
            float n = anoise(); st[2] += (n - st[2]) * 0.015f;
            st[3] += dt * 0.13f; if (st[3] > 1) st[3] -= 1;
            float w = st[2] * A.wind * (0.8f + 0.6f * sinf(st[3] * 2 * PI_F)) * 2.5f;
            L += w; R += w;
        }
        if (A.heli > 0.001f) {
            st[4] += A.heliRate * dt; if (st[4] > 1) st[4] -= 1;
            float n = anoise(); st[5] += (n - st[5]) * 0.05f;
            float h = st[5] * expf(-st[4] * 9.0f) * A.heli * 3.0f;
            L += h; R += h;
        }
        if (A.breath > 0.001f) {
            st[6] += 0.33f * dt; if (st[6] > 1) st[6] -= 1;
            float env = sinf(st[6] * 2 * PI_F); env = env > 0 ? env * env : env * env * 0.6f;
            float n = anoise(); st[7] += (n - st[7]) * 0.12f;
            float b = st[7] * env * A.breath * 0.7f;
            L += b; R += b;
        }
        if (A.heart > 0.001f) {
            st[8] += 1.25f * dt; if (st[8] > 1) st[8] -= 1;
            float p = st[8];
            float env = expf(-p * 30.0f) + 0.7f * expf(-fabsf(p - 0.22f) * 30.0f);
            st[9] += 48.0f * dt; if (st[9] > 1) st[9] -= 1;
            float h = sinf(st[9] * 2 * PI_F) * env * A.heart * 0.6f;
            L += h; R += h;
        }
        // эхо
        int pl = (g_echoPos - dl + ECHO_N) % ECHO_N, pr = (g_echoPos - dr + ECHO_N) % ECHO_N;
        float eL = g_echoL[pl], eR = g_echoR[pr];
        g_echoL[g_echoPos] = E * A.echoAmt + eR * 0.33f;
        g_echoR[g_echoPos] = E * A.echoAmt + eL * 0.33f;
        g_echoPos = (g_echoPos + 1) % ECHO_N;
        L += eL; R += eR;
        // глушение и звон в ушах
        float ring = clampf(A.ring, 0, 1);
        float mc = 1.0f - 0.9f * ring;
        st[10] += (L - st[10]) * mc; st[11] += (R - st[11]) * mc;
        L = st[10]; R = st[11];
        if (ring > 0.001f) {
            st[12] += 3600.0f * dt; if (st[12] > 1) st[12] -= 1;
            float tone = sinf(st[12] * 2 * PI_F) * ring * 0.12f;
            L += tone; R += tone;
        }
        L *= A.master; R *= A.master;
        L = L / (1.0f + fabsf(L)); R = R / (1.0f + fabsf(R));
        out[i * 2] = (int16_t)(L * 30000.0f);
        out[i * 2 + 1] = (int16_t)(R * 30000.0f);
    }
}

static void audioInit() {
    memset(g_v, 0, sizeof(g_v));
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { g_adev = 0; return; }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 512; want.callback = audioCB;
    g_adev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!g_adev) return;
    if (have.format != AUDIO_S16SYS || have.channels != 2) { SDL_CloseAudioDevice(g_adev); g_adev = 0; return; }
    g_arate = (float)have.freq;
    SDL_PauseAudioDevice(g_adev, 0);
}
static void audioSync() {
    if (!g_adev) return;
    SDL_LockAudioDevice(g_adev);
    g_ambA = g_amb;
    SDL_UnlockAudioDevice(g_adev);
}

static V3 g_lisPos; static float g_lisYaw = 0;
static void playSnd(int type, float vol, float pan, float echo, float dur, float p0 = 0, float p1 = 0, float p2 = 0, float p3 = 0) {
    if (!g_adev || vol < 0.003f) return;
    SDL_LockAudioDevice(g_adev);
    int best = -1; float bestT = -1;
    for (int i = 0; i < MAXV; i++) {
        if (!g_v[i].on) { best = i; break; }
        float tt = g_v[i].t / std::max(0.001f, g_v[i].dur);
        if (tt > bestT) { bestT = tt; best = i; }
    }
    Voice& v = g_v[best];
    memset(&v, 0, sizeof(v));
    v.type = type; v.on = true; v.dur = dur; v.vol = vol; v.echo = echo;
    pan = clampf(pan, -1, 1);
    v.gl = sqrtf(0.5f * (1 - pan)) * 1.2f; v.gr = sqrtf(0.5f * (1 + pan)) * 1.2f;
    v.p0 = p0; v.p1 = p1; v.p2 = p2; v.p3 = p3;
    v.e1 = v.e2 = v.e3 = 1;
    float r = g_arate;
    switch (type) {
    case ST_SHOT: v.k1 = expf(-p0 / r); v.k2 = expf(-p0 * 0.55f / r); v.k3 = expf(-40.0f / r); break;
    case ST_EXPLO: v.k1 = expf(-2.4f / r); v.k2 = expf(-3.5f / r); break;
    case ST_STEP: v.k1 = expf(-45.0f / r); v.k2 = expf(-20.0f / r); break;
    case ST_IMPACT: v.k1 = expf(-55.0f / r); v.k2 = expf(-18.0f / r); break;
    case ST_HURT: v.k1 = expf(-14.0f / r); break;
    default: v.k1 = v.k2 = v.k3 = 1; break;
    }
    SDL_UnlockAudioDevice(g_adev);
}
static void spatial(V3 p, float& vol, float& pan, float& d) {
    V3 dv = p - g_lisPos;
    d = len(dv);
    float a = wrapPi(atan2f(dv.x, dv.z) - g_lisYaw);
    pan = sinf(a) * clampf(d / 3.0f, 0, 1) * 0.85f;
    vol = 1.0f / (1.0f + d * 0.06f);
}
// параметры выстрела: [тип оружия] = {затухание, яркость, частота удара, треск, громкость}
static const float kShotP[7][5] = {
    {24, 0.55f, 105, 0.6f, 0.95f}, // ШВ-5
    {36, 0.68f, 140, 0.5f, 0.75f}, // ПП-9
    {9, 0.42f, 62, 0.9f, 1.10f},   // СВК
    {13, 0.33f, 75, 0.8f, 1.10f},  // дробовик
    {42, 0.72f, 185, 0.4f, 0.65f}, // пистолет
    {20, 0.47f, 95, 0.6f, 0.90f},  // АК противника
    {60, 0.30f, 300, 0.1f, 0.35f}, // глушитель
};
static void sndShot(int kind, V3 p, bool player) {
    float vol, pan, d;
    if (player) { vol = 1.0f; pan = 0; d = 0; } else spatial(p, vol, pan, d);
    const float* k = kShotP[clampi(kind, 0, 6)];
    float bright = k[1] / (1.0f + d * 0.02f);
    playSnd(ST_SHOT, vol * k[4] * (player ? 0.85f : 0.9f), pan, 0.6f, 0.6f + (kind == 2 ? 0.5f : 0.0f), k[0], bright, k[2], k[3]);
}
static void sndAt(int type, V3 p, float vol, float dur, float p0 = 0, float p1 = 0, float p2 = 0, float echo = 0.1f) {
    float v, pan, d; spatial(p, v, pan, d);
    playSnd(type, vol * v, pan, echo, dur, p0, p1, p2, 0);
}

// =====================================================================
//  ПРОГРАММНЫЙ 3D-РЕНДЕР: треугольники + z-буфер (1/z), плоское затенение,
//  туман по дальности в пост-проходе.
// =====================================================================
static std::vector<uint32_t> g_fb;
static std::vector<float> g_zb;
static std::vector<uint8_t> g_vig;

struct Cam { V3 pos, R, U, F; float focal, cx, cy, nearZ, farZ, th, tv, kx, ky; } g_cam;
struct Env {
    V3 sun; float amb, sunI;
    uint32_t fog, skyTop, skyHor;
    float fogS, fogE, flash;
    float vmLight;
    float cloud;      // облачность 0..1
} g_env;
static float g_skyTime = 0;   // время для движения облаков

static float g_baseAmb = 0.5f;
struct Basis { V3 r, u, f; };
static Basis basisYPR(float yaw, float pitch, float roll) {
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), cr = cosf(roll), sr = sinf(roll);
    V3 F(sy * cp, sp, cy * cp), R0(cy, 0, -sy), U0(-sy * sp, cp, -cy * sp);
    Basis b; b.f = F; b.r = R0 * cr + U0 * sr; b.u = U0 * cr - R0 * sr;
    return b;
}
static inline V3 bx(const Basis& b, V3 l) { return b.r * l.x + b.u * l.y + b.f * l.z; }
static Basis bmul(const Basis& p, const Basis& l) { Basis o; o.r = bx(p, l.r); o.u = bx(p, l.u); o.f = bx(p, l.f); return o; }
static Basis rotX(float a) { Basis b; b.r = V3(1, 0, 0); b.u = V3(0, cosf(a), sinf(a)); b.f = V3(0, -sinf(a), cosf(a)); return b; }
static Basis rotZ(float a) { Basis b; b.r = V3(cosf(a), sinf(a), 0); b.u = V3(-sinf(a), cosf(a), 0); b.f = V3(0, 0, 1); return b; }

static void rendResize(int w, int h) {
    RW = w; RH = h;
    g_fb.assign((size_t)RW * RH, 0);
    g_zb.assign((size_t)RW * RH, 0.0f);
    g_vig.assign((size_t)RW * RH, 0);
    for (int y = 0; y < RH; y++)
        for (int x = 0; x < RW; x++) {
            float dx = (x - RW * 0.5f) / (RW * 0.5f), dy = (y - RH * 0.5f) / (RH * 0.5f);
            float d = sqrtf(dx * dx * 0.8f + dy * dy);
            g_vig[y * RW + x] = (uint8_t)(clampf((d - 0.55f) / 0.6f, 0, 1) * 255);
        }
}

static void setCam(V3 pos, float yaw, float pitch, float roll, float fovH, float farZ) {
    Basis b = basisYPR(yaw, pitch, roll);
    g_cam.pos = pos; g_cam.R = b.r; g_cam.U = b.u; g_cam.F = b.f;
    g_cam.focal = (RH * (16.0f / 9.0f) * 0.5f) / tanf(fovH * 0.5f);
    g_cam.cx = RW * 0.5f; g_cam.cy = RH * 0.5f;
    g_cam.nearZ = 0.05f; g_cam.farZ = farZ;
    g_cam.th = (RW * 0.5f) / g_cam.focal; g_cam.tv = (RH * 0.5f) / g_cam.focal;
    g_cam.kx = 1.0f / sqrtf(1 + g_cam.th * g_cam.th); g_cam.ky = 1.0f / sqrtf(1 + g_cam.tv * g_cam.tv);
}
static inline V3 toCam(V3 p) { V3 d = p - g_cam.pos; return V3(dot(d, g_cam.R), dot(d, g_cam.U), dot(d, g_cam.F)); }
static bool sphereVis(V3 c, float r) {
    V3 p = toCam(c);
    if (p.z < -r || p.z - r > g_cam.farZ) return false;
    if ((p.x - g_cam.th * p.z) * g_cam.kx > r) return false;
    if ((-p.x - g_cam.th * p.z) * g_cam.kx > r) return false;
    if ((p.y - g_cam.tv * p.z) * g_cam.ky > r) return false;
    if ((-p.y - g_cam.tv * p.z) * g_cam.ky > r) return false;
    return true;
}

enum { RM_OPAQUE = 0, RM_SHADOW = 1, RM_BLEND = 2, RM_ADD = 3 };
struct SV { float x, y, z; };
static inline uint32_t addC(uint32_t a, uint32_t b) {
    uint32_t r = std::min(255u, ((a >> 16) & 255) + ((b >> 16) & 255));
    uint32_t g = std::min(255u, ((a >> 8) & 255) + ((b >> 8) & 255));
    uint32_t bb = std::min(255u, (a & 255) + (b & 255));
    return 0xFF000000u | (r << 16) | (g << 8) | bb;
}
static inline uint32_t avgC(uint32_t a, uint32_t b) { return 0xFF000000u | (((a & 0xFEFEFEu) >> 1) + ((b & 0xFEFEFEu) >> 1)); }

static void rasterTri(SV a, SV b, SV c, uint32_t col, int mode) {
    float det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (fabsf(det) < 1e-5f) return;
    float A = ((b.z - a.z) * (c.y - a.y) - (c.z - a.z) * (b.y - a.y)) / det;
    float B = ((b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z)) / det;
    float C = a.z - A * a.x - B * a.y;
    if (a.y > b.y) std::swap(a, b);
    if (b.y > c.y) std::swap(b, c);
    if (a.y > b.y) std::swap(a, b);
    if (c.y - a.y < 1e-6f) return;
    int ys = std::max(0, (int)ceilf(a.y - 0.5f)), ye = std::min(RH - 1, (int)ceilf(c.y - 0.5f) - 1);
    float sAC = (c.x - a.x) / (c.y - a.y);
    float sAB = (b.y - a.y) > 1e-6f ? (b.x - a.x) / (b.y - a.y) : 0.0f;
    float sBC = (c.y - b.y) > 1e-6f ? (c.x - b.x) / (c.y - b.y) : 0.0f;
    uint32_t* fb = g_fb.data(); float* zb = g_zb.data();
    for (int y = ys; y <= ye; y++) {
        float py = y + 0.5f;
        float xa = a.x + (py - a.y) * sAC;
        float xb = (py < b.y) ? a.x + (py - a.y) * sAB : b.x + (py - b.y) * sBC;
        if (xa > xb) std::swap(xa, xb);
        int xs = std::max(0, (int)ceilf(xa - 0.5f)), xe = std::min(RW - 1, (int)ceilf(xb - 0.5f) - 1);
        if (xs > xe) continue;
        float z = A * (xs + 0.5f) + B * py + C;
        int i = y * RW + xs;
        switch (mode) {
        case RM_OPAQUE:
            for (int x = xs; x <= xe; x++, i++, z += A) if (z > zb[i]) { zb[i] = z; fb[i] = col; }
            break;
        case RM_SHADOW:
            for (int x = xs; x <= xe; x++, i++, z += A) if (z > zb[i]) fb[i] = 0xFF000000u | ((fb[i] & 0xFEFEFEu) >> 1);
            break;
        case RM_BLEND:
            for (int x = xs; x <= xe; x++, i++, z += A) if (z > zb[i]) fb[i] = avgC(fb[i], col);
            break;
        default:
            for (int x = xs; x <= xe; x++, i++, z += A) if (z > zb[i]) fb[i] = addC(fb[i], col);
            break;
        }
    }
}

// полигон в пространстве камеры (до 4–6 вершин)

// ------------------------------------------------------------ батчи для видеокарты
// Вершина: мировые координаты, нормаль, цвет RGBA, uv (метры), id текстуры, база затенения, флаг
// флаг: 0 — освещаемая, 1 — светящаяся, 2 — уже освещённая (только туман), 3 — частица, 4 — вода
struct GV { float x, y, z, nx, ny, nz; uint32_t c; float u, v, t, ao, fl; };
static std::vector<GV> g_bOpq, g_bShd, g_bBld, g_bAdd, g_bVmO, g_bVmA;
static bool g_useGL = false, g_inVM = false, g_glShadows = false;
static float g_vmFocal = 1;
static SDL_Rect g_dst = {0, 0, 1280, 720};
static float g_post[8];
static void glScene();
static inline uint32_t glCol(uint32_t argb, float a) {
    return ((argb >> 16) & 255) | (argb & 0xFF00u) | ((argb & 255) << 16) | ((uint32_t)clampi((int)(a * 255), 0, 255) << 24);
}
static inline std::vector<GV>& glBatch(int mode) {
    if (g_inVM) return mode == RM_ADD ? g_bVmA : g_bVmO;
    switch (mode) { case RM_SHADOW: return g_bShd; case RM_BLEND: return g_bBld; case RM_ADD: return g_bAdd; default: return g_bOpq; }
}
static inline void glPush(std::vector<GV>& B, V3 p, V3 n, uint32_t c, float u, float v, float t, float ao, float fl) {
    GV g; g.x = p.x; g.y = p.y; g.z = p.z; g.nx = n.x; g.ny = n.y; g.nz = n.z; g.c = c; g.u = u; g.v = v; g.t = t; g.ao = ao; g.fl = fl;
    B.push_back(g);
}
static inline V3 camToWorld(V3 c) { return g_cam.pos + g_cam.R * c.x + g_cam.U * c.y + g_cam.F * c.z; }
static inline V3 camDirToWorld(V3 c) { return g_cam.R * c.x + g_cam.U * c.y + g_cam.F * c.z; }
static void glPolyCam(const V3* p, int n, uint32_t col, int mode) {
    if (mode == RM_SHADOW && g_glShadows) return;
    std::vector<GV>& B = glBatch(mode);
    V3 w[12];
    for (int i = 0; i < n && i < 12; i++) w[i] = camToWorld(p[i]);
    V3 nr = norm(cross(w[1] - w[0], w[2] - w[0]));
    float a = mode == RM_BLEND ? 0.5f : 1.0f;
    uint32_t c = glCol(col, a);
    for (int i = 1; i + 1 < n; i++) {
        glPush(B, w[0], nr, c, 0, 0, 0, -1000, 2); glPush(B, w[i], nr, c, 0, 0, 0, -1000, 2); glPush(B, w[i + 1], nr, c, 0, 0, 0, -1000, 2);
    }
}
// грань из 4 мировых точек с освещением
static inline void glQuadLit(std::vector<GV>& B, const V3* q, V3 n, uint32_t c, const float* U, const float* Vv, float tex, float ao, float fl) {
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) { int i = idx[k]; glPush(B, q[i], n, c, U ? U[i] : 0.0f, U ? Vv[i] : 0.0f, tex, ao, fl); }
}
static void drawPolyCam(const V3* p, int n, uint32_t col, int mode) {
    if (g_useGL) { glPolyCam(p, n, col, mode); return; }
    V3 o[12]; int m = 0;
    const float nz = g_cam.nearZ;
    bool anyFront = false;
    for (int i = 0; i < n; i++) if (p[i].z >= nz) { anyFront = true; break; }
    if (!anyFront) return;
    for (int i = 0; i < n && m < 11; i++) {
        V3 a = p[i], b = p[(i + 1) % n];
        bool ia = a.z >= nz, ib = b.z >= nz;
        if (ia) o[m++] = a;
        if (ia != ib) { float t = (nz - a.z) / (b.z - a.z); o[m++] = a + (b - a) * t; }
    }
    if (m < 3) return;
    SV s[12];
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < m; i++) {
        float iz = 1.0f / o[i].z;
        s[i].x = g_cam.cx + o[i].x * g_cam.focal * iz;
        s[i].y = g_cam.cy - o[i].y * g_cam.focal * iz;
        s[i].z = iz;
        minx = std::min(minx, s[i].x); maxx = std::max(maxx, s[i].x);
        miny = std::min(miny, s[i].y); maxy = std::max(maxy, s[i].y);
    }
    if (maxx < 0 || minx > RW || maxy < 0 || miny > RH) return;
    for (int i = 1; i + 1 < m; i++) rasterTri(s[0], s[i], s[i + 1], col, mode);
}
static void drawTriW(V3 a, V3 b, V3 c, uint32_t col, int mode) {
    V3 p[3] = {toCam(a), toCam(b), toCam(c)};
    drawPolyCam(p, 3, col, mode);
}
static void drawQuadW(V3 a, V3 b, V3 c, V3 d, uint32_t col, int mode) {
    V3 p[4] = {toCam(a), toCam(b), toCam(c), toCam(d)};
    drawPolyCam(p, 4, col, mode);
}

// ------------------------------------------------------------ процедурные текстуры
enum TexId { TX_NONE, TX_PLASTER, TX_BRICK, TX_CONCRETE, TX_WOOD, TX_METAL, TX_ROOF, TX_ROCK, TX_GROUND, TX_SANDBAG, TX_ASPHALT, TX_ROAD, TX_PAVING, TX_TILES, TX_DIRT };
static uint8_t g_noise[256 * 256];
static uint8_t g_fine[256 * 256];
static uint8_t g_noise2[256 * 256];   // шум с решёткой 64x64 — детали облаков
static void texInit() {
    uint32_t s = 0x1234567u;
    auto rr = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
    float grid[17][17];
    for (int y = 0; y < 17; y++) for (int x = 0; x < 17; x++) grid[y][x] = (rr() & 255) / 255.0f;
    for (int x = 0; x < 17; x++) grid[16][x] = grid[0][x];
    for (int y = 0; y < 17; y++) grid[y][16] = grid[y][0];
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float fx = x / 16.0f, fy = y / 16.0f; int ix = (int)fx, iy = (int)fy; float tx = fx - ix, ty = fy - iy;
            tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
            float a = grid[iy][ix] + (grid[iy][ix + 1] - grid[iy][ix]) * tx;
            float b = grid[iy + 1][ix] + (grid[iy + 1][ix + 1] - grid[iy + 1][ix]) * tx;
            g_noise[y * 256 + x] = (uint8_t)((a + (b - a) * ty) * 255);
            g_fine[y * 256 + x] = (uint8_t)(rr() & 255);
        }
    static float g2[65][65];
    for (int y = 0; y < 65; y++) for (int x = 0; x < 65; x++) g2[y][x] = (rr() & 255) / 255.0f;
    for (int i = 0; i < 65; i++) { g2[64][i] = g2[0][i]; g2[i][64] = g2[i][0]; }
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float fx = x / 4.0f, fy = y / 4.0f; int ix = (int)fx, iy = (int)fy; float tx = fx - ix, ty = fy - iy;
            tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
            float a = g2[iy][ix] + (g2[iy][ix + 1] - g2[iy][ix]) * tx, b = g2[iy + 1][ix] + (g2[iy + 1][ix + 1] - g2[iy + 1][ix]) * tx;
            g_noise2[y * 256 + x] = (uint8_t)((a + (b - a) * ty) * 255);
        }
}
static inline int NZ(int x, int y) { return g_noise[((y & 255) << 8) | (x & 255)]; }
static inline int FN(int x, int y) { return g_fine[((y & 255) << 8) | (x & 255)]; }
// множитель яркости (256 = 1.0) для точки поверхности (u, v в метрах)
static inline int texMul(int tex, float u, float v, float dist) {
    int iu = (int)floorf(clampf(u, -50000.0f, 50000.0f) * 16.0f), iv = (int)floorf(clampf(v, -50000.0f, 50000.0f) * 16.0f);
    int m = 256;
    switch (tex) {
    case TX_PLASTER: m = 238 + ((NZ(iu >> 1, iv >> 1) - 128) >> 2) + ((FN(iu, iv) & 15) - 8); break;
    case TX_BRICK: {
        int row = iv / 3, off = (row & 1) * 4, cu = iu + off;
        if (iv % 3 == 0 || (cu & 7) == 0) m = 175;
        else m = 232 + ((int)(hash2(cu >> 3, row) & 31) - 16) + ((FN(iu, iv) & 7) - 4);
    } break;
    case TX_CONCRETE: m = ((iu % 24) == 0 || (iv % 24) == 0) ? 190 : 236 + ((NZ(iu, iv) - 128) >> 3) + ((FN(iu, iv) & 7) - 4); break;
    case TX_WOOD: m = (iv % 3 == 0) ? 175 : 228 + ((NZ(iu >> 2, iv * 6) - 128) >> 2); break;
    case TX_METAL: m = ((iu & 3) < 2 ? 246 : 206) + ((NZ(iu >> 2, iv >> 2) - 128) >> 3); break;
    case TX_ROOF: { int r4 = iv & 3; m = r4 == 0 ? 165 : 240 - r4 * 9 + ((int)(hash2(iu / 5 + (iv >> 2), iv >> 2) & 15) - 8); } break;
    case TX_ROCK: m = 226 + ((NZ(iu >> 2, iv >> 2) - 128) >> 1) + ((NZ(iu, iv) - 128) >> 3); break;
    case TX_GROUND: m = 238 + ((NZ(iu >> 3, iv >> 3) - 128) >> 2) + ((NZ(iu, iv) - 128) >> 3) + ((FN(iu, iv) & 15) - 8); break;
    case TX_SANDBAG: { int row = iv / 4, cu = iu + (row & 1) * 3; int ly = iv % 4, lx = cu % 7; m = (ly == 0 || lx == 0) ? 180 : 238 - abs(ly - 2) * 6 - abs(lx - 3) * 4; } break;
    case TX_ASPHALT: case TX_ROAD: m = 240 + ((NZ(iu >> 2, iv >> 2) - 128) >> 3) + ((FN(iu, iv) & 15) - 8) - ((hash2(iu / 27, iv / 69) & 15) == 0 ? 26 : 0); break;
    case TX_PAVING: { int row = iv / 4, cu = iu + (row & 1) * 2; m = ((iv & 3) == 0 || (cu % 3) == 0) ? 178 : 226 + ((int)(hash2(cu / 3, row) & 31) - 16) + ((FN(iu, iv) & 7) - 4); } break;
    case TX_TILES: m = ((iu & 7) == 0 || (iv & 7) == 0) ? 200 : 244 + ((int)(hash2(iu >> 3, iv >> 3) & 15) - 8); break;
    case TX_DIRT: m = 236 + ((NZ(iu >> 2, iv >> 2) - 128) >> 2) + ((FN(iu, iv) & 15) - 8); break;
    default: break;
    }
    float f = dist < 18.0f ? 1.0f : (dist > 45.0f ? 0.0f : (45.0f - dist) / 27.0f);
    return 236 + (int)((m - 236) * f);
}
// текстурированный треугольник: z = 1/глубина, pu = u/глубина, pv = v/глубина
struct SVT { float x, y, z, pu, pv; };
static int g_texId = 0; static float g_aoBase = -1000.0f; static bool g_texVert = false;
static void rasterTriTex(SVT a, SVT b, SVT c, uint32_t col) {
    float det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (fabsf(det) < 1e-5f) return;
    float idet = 1.0f / det;
    auto plane = [&](float za, float zb, float zc, float& A, float& B, float& C) {
        A = ((zb - za) * (c.y - a.y) - (zc - za) * (b.y - a.y)) * idet;
        B = ((b.x - a.x) * (zc - za) - (c.x - a.x) * (zb - za)) * idet;
        C = za - A * a.x - B * a.y;
    };
    float Az, Bz, Cz, Au, Bu, Cu, Av, Bv, Cv;
    plane(a.z, b.z, c.z, Az, Bz, Cz); plane(a.pu, b.pu, c.pu, Au, Bu, Cu); plane(a.pv, b.pv, c.pv, Av, Bv, Cv);
    if (a.y > b.y) std::swap(a, b);
    if (b.y > c.y) std::swap(b, c);
    if (a.y > b.y) std::swap(a, b);
    if (c.y - a.y < 1e-6f) return;
    int ys = std::max(0, (int)ceilf(a.y - 0.5f)), ye = std::min(RH - 1, (int)ceilf(c.y - 0.5f) - 1);
    float sAC = (c.x - a.x) / (c.y - a.y);
    float sAB = (b.y - a.y) > 1e-6f ? (b.x - a.x) / (b.y - a.y) : 0.0f;
    float sBC = (c.y - b.y) > 1e-6f ? (c.x - b.x) / (c.y - b.y) : 0.0f;
    int cr = (col >> 16) & 255, cg = (col >> 8) & 255, cb = col & 255;
    uint32_t* fb = g_fb.data(); float* zb = g_zb.data();
    int tex = g_texId; float aob = g_aoBase; bool vert = g_texVert;
    for (int y = ys; y <= ye; y++) {
        float py = y + 0.5f;
        float xa = a.x + (py - a.y) * sAC;
        float xb = (py < b.y) ? a.x + (py - a.y) * sAB : b.x + (py - b.y) * sBC;
        if (xa > xb) std::swap(xa, xb);
        int xs = std::max(0, (int)ceilf(xa - 0.5f)), xe = std::min(RW - 1, (int)ceilf(xb - 0.5f) - 1);
        if (xs > xe) continue;
        float fx = xs + 0.5f;
        float z = Az * fx + Bz * py + Cz, pu = Au * fx + Bu * py + Cu, pv = Av * fx + Bv * py + Cv;
        int i = y * RW + xs;
        for (int x = xs; x <= xe; x++, i++, z += Az, pu += Au, pv += Av) {
            if (z <= zb[i]) continue;
            zb[i] = z;
            float d = 1.0f / z, u = pu * d, v = pv * d;
            int m = texMul(tex, u, v, d);
            if (vert) { float ao = (v - aob) * 1.4f + 0.6f; if (ao < 1.0f) m = (int)(m * (ao < 0.6f ? 0.6f : ao)); }
            int r = (cr * m) >> 8, g = (cg * m) >> 8, bb = (cb * m) >> 8;
            fb[i] = 0xFF000000u | ((uint32_t)(r > 255 ? 255 : r) << 16) | ((uint32_t)(g > 255 ? 255 : g) << 8) | (uint32_t)(bb > 255 ? 255 : bb);
        }
    }
}
// полигон в пространстве камеры с координатами текстуры
static void drawPolyCamT(const V3* p, const float* U, const float* Vv, int n, uint32_t col) {
    if (g_useGL) { glPolyCam(p, n, col, RM_OPAQUE); return; }
    V3 o[12]; float ou[12], ov[12]; int m = 0;
    const float nz = g_cam.nearZ;
    bool anyFront = false;
    for (int i = 0; i < n; i++) if (p[i].z >= nz) { anyFront = true; break; }
    if (!anyFront) return;
    for (int i = 0; i < n && m < 11; i++) {
        int j = (i + 1) % n;
        bool ia = p[i].z >= nz, ib = p[j].z >= nz;
        if (ia) { o[m] = p[i]; ou[m] = U[i]; ov[m] = Vv[i]; m++; }
        if (ia != ib) { float t = (nz - p[i].z) / (p[j].z - p[i].z); o[m] = p[i] + (p[j] - p[i]) * t; ou[m] = U[i] + (U[j] - U[i]) * t; ov[m] = Vv[i] + (Vv[j] - Vv[i]) * t; m++; }
    }
    if (m < 3) return;
    SVT s[12];
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < m; i++) {
        float iz = 1.0f / o[i].z;
        s[i].x = g_cam.cx + o[i].x * g_cam.focal * iz; s[i].y = g_cam.cy - o[i].y * g_cam.focal * iz;
        s[i].z = iz; s[i].pu = ou[i] * iz; s[i].pv = ov[i] * iz;
        minx = std::min(minx, s[i].x); maxx = std::max(maxx, s[i].x); miny = std::min(miny, s[i].y); maxy = std::max(maxy, s[i].y);
    }
    if (maxx < 0 || minx > RW || maxy < 0 || miny > RH) return;
    for (int i = 1; i + 1 < m; i++) rasterTriTex(s[0], s[i], s[i + 1], col);
}
static inline float faceShade(V3 n) {
    float s = g_env.amb + g_env.sunI * std::max(0.0f, dot(n, g_env.sun));
    if (n.y < -0.5f) s *= 0.7f;
    return s;
}
static const int kFace[6][4] = {{1, 3, 7, 5}, {0, 2, 6, 4}, {2, 3, 7, 6}, {0, 1, 5, 4}, {4, 5, 7, 6}, {0, 1, 3, 2}};
static const V3 kFaceN[6] = {V3(1, 0, 0), V3(-1, 0, 0), V3(0, 1, 0), V3(0, -1, 0), V3(0, 0, 1), V3(0, 0, -1)};

static void drawBoxW(V3 mn, V3 mx, uint32_t col, bool glow = false, int mode = RM_OPAQUE, int tex = 0) {
    if (g_useGL) {
        V3 w[8];
        for (int i = 0; i < 8; i++) w[i] = V3((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        std::vector<GV>& B = glBatch(mode);
        for (int f = 0; f < 6; f++) { V3 q[4] = {w[kFace[f][0]], w[kFace[f][1]], w[kFace[f][2]], w[kFace[f][3]]}; glQuadLit(B, q, kFaceN[f], glCol(col, mode == RM_BLEND ? 0.5f : 1.0f), nullptr, nullptr, (float)tex, mn.y, glow ? 1.0f : 0.0f); }
        return;
    }
    V3 c = (mn + mx) * 0.5f, h = (mx - mn) * 0.5f;
    if (!sphereVis(c, len(h))) return;
    V3 w[8], cp[8];
    for (int i = 0; i < 8; i++) {
        w[i] = V3((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        cp[i] = toCam(w[i]);
    }
    const V3& e = g_cam.pos;
    bool vis[6] = {e.x > mx.x, e.x < mn.x, e.y > mx.y, e.y < mn.y, e.z > mx.z, e.z < mn.z};
    bool textured = tex != 0 && mode == RM_OPAQUE && !glow;
    if (textured) {
        // дальние коробки рисуем без текстуры
        float dd = len(c - e) - len(h);
        if (dd > 50.0f) textured = false;
    }
    for (int f = 0; f < 6; f++) {
        if (!vis[f]) continue;
        V3 q[4] = {cp[kFace[f][0]], cp[kFace[f][1]], cp[kFace[f][2]], cp[kFace[f][3]]};
        uint32_t fc = glow ? col : shadeC(col, faceShade(kFaceN[f]));
        if (textured) {
            float U[4], Vv[4];
            for (int k = 0; k < 4; k++) {
                const V3& wp = w[kFace[f][k]];
                if (f < 2) { U[k] = wp.z; Vv[k] = wp.y; } else if (f < 4) { U[k] = wp.x; Vv[k] = wp.z; } else { U[k] = wp.x; Vv[k] = wp.y; }
            }
            g_texId = tex; g_texVert = (f < 2 || f >= 4); g_aoBase = mn.y;
            if (f == 3) g_texVert = false;
            drawPolyCamT(q, U, Vv, 4, fc);
        } else drawPolyCam(q, 4, fc, mode);
    }
}
// ориентированный параллелепипед; camSpace — для модели оружия в руках
static void drawOBB(V3 c, V3 a0, V3 a1, V3 a2, V3 h, uint32_t col, bool camSpace = false, int mode = RM_OPAQUE) {
    if (g_useGL) {
        if (camSpace) { c = camToWorld(c); a0 = camDirToWorld(a0); a1 = camDirToWorld(a1); a2 = camDirToWorld(a2); }
        else if (!sphereVis(c, len(h))) return;
        V3 ax[3] = {a0 * h.x, a1 * h.y, a2 * h.z}, w[8];
        for (int i = 0; i < 8; i++) w[i] = c + ((i & 1) ? ax[0] : -ax[0]) + ((i & 2) ? ax[1] : -ax[1]) + ((i & 4) ? ax[2] : -ax[2]);
        V3 nrm[6] = {a0, -a0, a1, -a1, a2, -a2};
        std::vector<GV>& B = glBatch(mode);
        uint32_t gc = glCol(col, mode == RM_BLEND ? 0.5f : 1.0f);
        float fl = mode == RM_ADD ? 1.0f : 0.0f;
        for (int f = 0; f < 6; f++) { V3 q[4] = {w[kFace[f][0]], w[kFace[f][1]], w[kFace[f][2]], w[kFace[f][3]]}; glQuadLit(B, q, nrm[f], gc, nullptr, nullptr, 0, -1000, fl); }
        return;
    }
    if (!camSpace && !sphereVis(c, len(h))) return;
    V3 ax[3] = {a0 * h.x, a1 * h.y, a2 * h.z};
    V3 cp[8];
    for (int i = 0; i < 8; i++) {
        V3 w = c + ((i & 1) ? ax[0] : -ax[0]) + ((i & 2) ? ax[1] : -ax[1]) + ((i & 4) ? ax[2] : -ax[2]);
        cp[i] = camSpace ? w : toCam(w);
    }
    V3 eye = camSpace ? V3(0, 0, 0) : g_cam.pos;
    V3 nrm[6] = {a0, -a0, a1, -a1, a2, -a2};
    float hs[6] = {h.x, h.x, h.y, h.y, h.z, h.z};
    for (int f = 0; f < 6; f++) {
        V3 fc = c + nrm[f] * hs[f];
        if (dot(eye - fc, nrm[f]) <= 0) continue;
        V3 q[4] = {cp[kFace[f][0]], cp[kFace[f][1]], cp[kFace[f][2]], cp[kFace[f][3]]};
        float s;
        if (camSpace) s = g_env.vmLight * (0.55f + 0.45f * std::max(0.0f, dot(nrm[f], norm(V3(0.4f, 0.8f, -0.45f)))));
        else s = faceShade(nrm[f]);
        drawPolyCam(q, 4, mode == RM_OPAQUE ? shadeC(col, s) : col, mode);
    }
}
static void drawPart(const Basis& B, V3 org, V3 lc, V3 h, uint32_t col, bool cam = false) {
    drawOBB(org + bx(B, lc), B.r, B.u, B.f, h, col, cam);
}
static void drawLine3D(V3 a, V3 b, uint32_t col, int mode) {
    if (g_useGL) {
        V3 d = b - a; if (len(d) < 1e-4f) return;
        V3 mid = (a + b) * 0.5f;
        float w = 0.012f + 0.0015f * len(mid - g_cam.pos);
        V3 s = norm(cross(d, g_cam.pos - mid)) * w;
        V3 q[4] = {a - s, b - s, b + s, a + s};
        std::vector<GV>& B = glBatch(mode == RM_OPAQUE ? RM_ADD : mode);
        glQuadLit(B, q, V3(0, 1, 0), glCol(col, mode == RM_BLEND ? 0.5f : 1.0f), nullptr, nullptr, 0, -1000, 1);
        return;
    }
    V3 ca = toCam(a), cb = toCam(b);
    float nz = g_cam.nearZ;
    if (ca.z < nz && cb.z < nz) return;
    if (ca.z < nz) ca = ca + (cb - ca) * ((nz - ca.z) / (cb.z - ca.z));
    if (cb.z < nz) cb = cb + (ca - cb) * ((nz - cb.z) / (ca.z - cb.z));
    float iza = 1 / ca.z, izb = 1 / cb.z;
    float xa = g_cam.cx + ca.x * g_cam.focal * iza, ya = g_cam.cy - ca.y * g_cam.focal * iza;
    float xb = g_cam.cx + cb.x * g_cam.focal * izb, yb = g_cam.cy - cb.y * g_cam.focal * izb;
    int steps = (int)std::max(fabsf(xb - xa), fabsf(yb - ya)) + 1;
    if (steps > 2000) steps = 2000;
    for (int s = 0; s <= steps; s++) {
        float t = (float)s / steps;
        int x = (int)(xa + (xb - xa) * t), y = (int)(ya + (yb - ya) * t);
        if ((unsigned)x >= (unsigned)RW || (unsigned)y >= (unsigned)RH) continue;
        float z = iza + (izb - iza) * t;
        int i = y * RW + x;
        if (z <= g_zb[i]) continue;
        if (mode == RM_ADD) g_fb[i] = addC(g_fb[i], col);
        else if (mode == RM_BLEND) g_fb[i] = avgC(g_fb[i], col);
        else { g_fb[i] = col; g_zb[i] = z; }
    }
}
static void drawSprite(V3 p, float size, uint32_t col, int mode) {
    if (g_useGL) {
        V3 c = toCam(p);
        if (c.z < 0.15f || c.z > g_cam.farZ) return;
        float s = std::max(size * 0.5f, 0.004f * c.z);
        V3 R = g_cam.R * s, U = g_cam.U * s;
        V3 q[4] = {p - R - U, p + R - U, p + R + U, p - R + U};
        float uu[4] = {-1, 1, 1, -1}, vv[4] = {-1, -1, 1, 1};
        std::vector<GV>& B = glBatch(mode);
        glQuadLit(B, q, -g_cam.F, glCol(col, mode == RM_BLEND ? 0.55f : 1.0f), uu, vv, 0, -1000, 3);
        return;
    }
    V3 c = toCam(p);
    if (c.z < 0.15f || c.z > g_cam.farZ) return;
    float iz = 1.0f / c.z;
    float sx = g_cam.cx + c.x * g_cam.focal * iz, sy = g_cam.cy - c.y * g_cam.focal * iz;
    float r = std::min(size * g_cam.focal * iz * 0.5f, RH * 0.6f);
    if (r < 0.5f) r = 0.5f;
    int x0 = std::max(0, (int)(sx - r)), x1 = std::min(RW - 1, (int)(sx + r));
    int y0 = std::max(0, (int)(sy - r)), y1 = std::min(RH - 1, (int)(sy + r));
    float r2 = r * r;
    for (int y = y0; y <= y1; y++) {
        float dy = y + 0.5f - sy;
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - sx;
            if (dx * dx + dy * dy > r2 && r > 1.5f) continue;
            int i = y * RW + x;
            if (iz <= g_zb[i]) continue;
            if (mode == RM_ADD) g_fb[i] = addC(g_fb[i], col);
            else if (mode == RM_BLEND) g_fb[i] = avgC(g_fb[i], col);
            else { g_fb[i] = col; g_zb[i] = iz; }
        }
    }
}
// небо и туман по z-буферу
static void postSkyFog() {
    float fs = g_env.fogS, inv = 1.0f / std::max(1.0f, g_env.fogE - g_env.fogS);
    uint32_t fog = g_env.fog;
    int fr = (fog >> 16) & 255, fg = (fog >> 8) & 255, fbb = fog & 255;
    uint32_t top = g_env.skyTop, hor = g_env.skyHor;
    if (g_env.flash > 0) {
        top = mixC(top, RGBc(220, 225, 255), g_env.flash);
        hor = mixC(hor, RGBc(220, 225, 255), g_env.flash);
    }
    float f2 = g_cam.focal * g_cam.focal;
    bool cloudOn = g_env.cloud > 0.02f;
    float thr = 1.0f - g_env.cloud, windU = fmodf(g_skyTime * 0.0021f * 256.0f, 256.0f), windV = fmodf(g_skyTime * 0.0009f * 256.0f, 256.0f);
    uint32_t sunC = RGBc(clampi((int)(g_env.sunI * 255), 0, 255), clampi((int)(g_env.sunI * 230), 0, 255), clampi((int)(g_env.sunI * 200), 0, 255));
    uint32_t cloudLit = addC(shadeC(hor, 0.75f), shadeC(sunC, 0.6f)), cloudDark = shadeC(mixC(top, hor, 0.5f), g_env.cloud > 0.6f ? 0.42f : 0.55f);
    for (int y = 0; y < RH; y++) {
        float vy = g_cam.cy - y - 0.5f;
        for (int x = 0; x < RW; x++) {
            int i = y * RW + x;
            float z = g_zb[i];
            if (z <= 0) {
                float vx = x + 0.5f - g_cam.cx;
                float dy = g_cam.F.y * g_cam.focal + g_cam.R.y * vx + g_cam.U.y * vy;
                float il = 1.0f / sqrtf(f2 + vx * vx + vy * vy);
                float t = dy * il;
                if (t <= 0) { g_fb[i] = fog; continue; }
                uint32_t c = mixC(hor, top, 1.0f - expf(-t * 3.2f));
                if (cloudOn) {   // облака: тот же шум, что и в шейдере
                    float dx = (g_cam.F.x * g_cam.focal + g_cam.R.x * vx + g_cam.U.x * vy) * il, dz = (g_cam.F.z * g_cam.focal + g_cam.R.z * vx + g_cam.U.z * vy) * il;
                    float k = 1.0f / (t + 0.11f) * 0.085f * 256.0f;
                    int u = (int)(dx * k + windU), v = (int)(dz * k + windV);
                    float f = (NZ(u, v) * 0.47f + NZ(u * 2 + 95, v * 2 + 95) * 0.23f + g_noise2[((v & 255) << 8) | (u & 255)] * 0.18f + g_noise2[(((v * 3 + 33) & 255) << 8) | ((u * 3 + 33) & 255)] * 0.12f) / 255.0f;
                    float cv = clampf((f - (thr - 0.05f)) / 0.29f, 0, 1); cv = cv * cv * (3 - 2 * cv);
                    cv *= clampf(t / 0.22f, 0, 1);
                    if (cv > 0.01f) c = mixC(c, mixC(cloudDark, cloudLit, clampf(0.5f + (f - 0.5f) * 1.5f, 0, 1)), cv * 0.95f);
                }
                float hz = 1.0f - t; hz *= hz; hz *= hz * hz;
                g_fb[i] = mixC(c, fog, hz * 0.6f);
            } else {
                float f = (1.0f / z - fs) * inv;
                if (f <= 0) continue;
                if (f >= 1) { g_fb[i] = fog; continue; }
                int w = (int)(f * 256);
                uint32_t c = g_fb[i];
                int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                r += ((fr - r) * w) >> 8; g += ((fg - g) * w) >> 8; b += ((fbb - b) * w) >> 8;
                g_fb[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }
}
// экранные эффекты: красная кромка, вспышка, затемнение, маска прицела
static void postEffects(float red, float white, float dark, float scope, uint32_t tint, float tintAmt) {
    if (g_useGL) {
        g_post[0] = clampf(red, 0, 1); g_post[1] = clampf(white, 0, 1); g_post[2] = clampf(dark, 0, 1); g_post[3] = scope;
        g_post[4] = ((tint >> 16) & 255) / 255.0f; g_post[5] = ((tint >> 8) & 255) / 255.0f; g_post[6] = (tint & 255) / 255.0f; g_post[7] = clampf(tintAmt, 0, 1);
        return;
    }
    if (red <= 0.01f && white <= 0.01f && dark <= 0.01f && scope <= 0.01f && tintAmt <= 0.01f) return;
    int wr = (int)(clampf(red, 0, 1) * 255), ww = (int)(clampf(white, 0, 1) * 255), wd = (int)(clampf(dark, 0, 1) * 255);
    int tr = (tint >> 16) & 255, tg = (tint >> 8) & 255, tb = tint & 255, wt = (int)(clampf(tintAmt, 0, 1) * 255);
    float sr = RH * 0.47f, sr2 = sr * sr;
    for (int y = 0; y < RH; y++)
        for (int x = 0; x < RW; x++) {
            int i = y * RW + x;
            uint32_t c = g_fb[i];
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            if (scope > 0.01f) {
                float dx = x + 0.5f - RW * 0.5f, dy = y + 0.5f - RH * 0.5f;
                float d2 = dx * dx + dy * dy;
                if (d2 > sr2) { r = g = b = 0; }
                else if (d2 > sr2 * 0.8f) { float k = (sr2 - d2) / (sr2 * 0.2f); r = (int)(r * k); g = (int)(g * k); b = (int)(b * k); }
            }
            if (wt) { r += ((tr * r / 255 - r) * wt) >> 8; g += ((tg * g / 255 - g) * wt) >> 8; b += ((tb * b / 255 - b) * wt) >> 8; }
            if (wr) {
                int v = (g_vig[i] * wr) >> 8;
                r += ((140 - r) * v) >> 8; g += ((0 - g) * v) >> 8; b += ((0 - b) * v) >> 8;
            }
            if (wd) { int v = std::max(wd, (int)g_vig[i] * wd / 160); v = std::min(255, v); r -= (r * v) >> 8; g -= (g * v) >> 8; b -= (b * v) >> 8; }
            if (ww) { r += ((255 - r) * ww) >> 8; g += ((255 - g) * ww) >> 8; b += ((255 - b) * ww) >> 8; }
            g_fb[i] = 0xFF000000u | ((uint32_t)clampi(r, 0, 255) << 16) | ((uint32_t)clampi(g, 0, 255) << 8) | (uint32_t)clampi(b, 0, 255);
        }
}

// =====================================================================
//  МИР, СУЩНОСТИ, СОСТОЯНИЕ ИГРЫ
// =====================================================================
enum Mat { MT_CONCRETE, MT_WOOD, MT_METAL, MT_GLASS, MT_CLOTH, MT_INVIS };
struct Box { V3 mn, mx; uint32_t col; uint8_t mat; bool solid, cover, vis, glow; uint8_t tex; };
struct Prism { V3 mn, mx; bool alongX; uint32_t col; uint32_t gable; };
static int g_defTex = -1;
struct CoverPt { V3 p, n; int box, occ; };
struct Grass { V3 mn, mx; std::vector<V3> tufts; uint32_t col; };

enum WeapId { W_AR, W_SMG, W_SNIPER, W_SHOTGUN, W_PISTOL, W_COUNT };
struct WeaponDef {
    const char* name; int mag; float dmg, rpm; bool autoFire;
    float hip, ads, moveAdd, recV, recH, rTac, rEmpty, adsFov, adsTime;
    int pellets; float pen; int snd; float range; bool ballistic; int reserveMax; bool shellReload; float cycle;
};
static const WeaponDef WD[W_COUNT] = {
    {"ШВ-5", 30, 34, 750, true, 3.6f, 0.30f, 3.0f, 0.70f, 0.30f, 2.1f, 2.8f, 50.f, 0.22f, 1, 0.35f, 0, 350, false, 210, false, 0},
    {"ПП-9", 32, 25, 900, true, 2.8f, 0.55f, 2.0f, 0.45f, 0.35f, 1.7f, 2.3f, 58.f, 0.16f, 1, 0.18f, 1, 120, false, 256, false, 0},
    {"СВК-338", 5, 160, 45, false, 7.0f, 0.0f, 6.0f, 4.5f, 0.6f, 3.0f, 3.8f, 9.f, 0.32f, 1, 0.9f, 2, 1600, true, 35, false, 1.1f},
    {"ДР-12", 8, 17, 75, false, 5.0f, 3.5f, 1.5f, 3.6f, 1.0f, 0.5f, 0.8f, 60.f, 0.18f, 9, 0.08f, 3, 45, false, 40, true, 0.75f},
    {"П-17", 15, 30, 380, false, 2.4f, 0.7f, 1.5f, 1.4f, 0.5f, 1.4f, 1.9f, 62.f, 0.12f, 1, 0.12f, 4, 90, false, 90, false, 0},
};
struct WState { int def = -1, mag = 0, reserve = 0; float reloadT = 0, reloadDur = 0; bool reloadEmpty = false; float fireCD = 0, boltT = 0; bool semiLock = false; bool silenced = false; };

struct Player {
    V3 pos; float vy = 0, yaw = 0, pitch = 0; int stance = 0; float eyeH = 1.62f; bool onGround = true;
    float hp = 100, sinceHit = 10, stamina = 1; bool sprint = false; float sprintLock = 0;
    float lean = 0; int leanDir = 0; float adsT = 0; int slot = 0; WState w[2]; int nade[3] = {0, 0, 0}; int tac = 1;
    float cookT = 0; int cookType = -1; float throwAnim = 0; float bobPh = 0, bobAmt = 0;
    float recP = 0, recY = 0, kick = 0, bloom = 0;
    float breath = 1; bool holding = false; float breathLock = 0; float swayT = 0; float switchT = 0; int pendingSlot = -1;
    int carry = -1; float flash = 0, shake = 0, suppress = 0, stepT = 0, hitMarker = 0, muzzle = 0;
    V3 vel; float speed = 0; bool dead = false; float deadT = 0; float useT = 0; bool ghillie = false;
    bool tank = false, tankTrig = false; float hull = 0, turret = 0, tspeed = 0, treload = 0, mgCD = 0;
    int shell = 0, loaded = 0; int shells[2] = {16, 30};   // 0 — БОПС (бронебойный), 1 — ОФС (фугасный)
};

enum AKind { AK_SOLDIER, AK_TARGET, AK_CIVTARGET, AK_DOG, AK_CONTACT, AK_ALLY, AK_VIP };
enum AState { AS_IDLE, AS_PATROL, AS_MOVE, AS_COVER, AS_FIGHT, AS_SCRIPT, AS_FLEE, AS_DOWN, AS_CARRIED };
enum Uni { U_GROM, U_SYND, U_MILITIA, U_RAVEN, U_CIV, U_SOVA, U_DOC };
struct Actor {
    int kind = AK_SOLDIER, team = 1, uni = U_SYND; const char* name = "";
    V3 pos, vel; float vy = 0, yaw = 0, aimPitch = 0;
    float hp = 100, maxhp = 100; bool alive = true; float deadT = 0; int stance = 0;
    int state = AS_IDLE; float st = 0;
    int cover = -1; V3 target; float speed = 3.6f;
    int tgt = -2; V3 lastKnown; float lastSeen = 99; bool sees = false; float react = 0;
    float fireCD = 0; int burst = 0; int ammo = 30; float reloadT = 0;
    int grenades = 1; float grenCD = 10; float suppressed = 0, flashed = 0;
    int role = 0; float peek = 0; bool peeking = false; float relocT = 6, stuckT = 0, avoidT = 0, avoidDir = 0;
    std::vector<V3> path; int pathI = 0; bool loopPath = false; float waitT = 0;
    float alert = 0; bool alerted = false; float walkPh = 0, hitFlash = 0;
    float accMul = 1, dmgMul = 1; int weapon = W_AR; bool stationary = false, invuln = false;
    int tag = 0; bool retreat = false; float callT = 0; bool looted = false; bool counted = false;
    float speedNow = 0; int formation = 0; float fallDir = 0; float muzzle = 0; int handler = -1; float barkT = 0;
    V3 home; bool scripted = false; bool hidden = false; float sayT = 0; float lastPos = 0;
};
enum VKind { VK_TRUCK, VK_TECH, VK_JEEP, VK_HELI, VK_JET, VK_TEL, VK_TANK };
struct Vehicle {
    int kind = VK_TRUCK, team = 1; V3 pos; float yaw = 0, pitch = 0, roll = 0, speed = 8;
    std::vector<V3> path; int pi = 0; float hp = 400, maxhp = 400; bool alive = true, active = true, arrived = false;
    float t = 0, fireCD = 2, rotor = 0, burn = 0; int cargo = 0; int mode = 0; V3 orbitC; float orbitR = 30, orbitA = 0, orbitH = 18;
    bool unloaded = false, spot = false; V3 spotP; int tag = 0, aimAt = -1; float fallV = 0; bool solidAdded = false; float leave = 0;
};
struct Grenade { V3 p, v; int type; float fuse; int team; bool rest; float spin; bool owner; };
struct Bullet { V3 p, v; float dmg, pen, life; int team, shooter; bool player; };
struct Part { V3 p, v; float life, maxLife, size, grow, grav, drag; uint32_t col; uint8_t mode; };
struct Smoke { V3 p; float r, life, emit; };
struct Tracer { V3 a, b; float t; uint32_t col; };
struct HitInd { float yaw, t; };
struct Sub { std::string who, text; float t, dur; uint32_t col; bool radio; };
struct Settings { float lookSens = 1.0f, padSens = 1.0f; bool invertY = false; int volume = 8; bool vibration = true; int quality = 1; int difficulty = 1; bool showFps = false; int renderer = 0; bool aimAssist = true; };
struct Progress { int unlocked = 1; float bestKH = 0; int cpMission = -1, cpStage = 0; bool done = false; };

enum Screen { SCR_MENU, SCR_SELECT, SCR_SETTINGS, SCR_BRIEF, SCR_PLAY, SCR_PAUSE, SCR_DEAD, SCR_DEBRIEF, SCR_EPILOGUE, SCR_CUTSCENE, SCR_DEFUSE };

static int g_lastDev = 2; // последнее устройство ввода: 0 тач, 1 геймпад, 2 клавиатура/мышь
struct MTri { V3 a, b, c; uint32_t col; };                 // статичный треугольник окружения (горы, воронки)
struct MGroup { V3 c; float r; int t0, t1; };               // группа треугольников для отсечения
struct Prop { V3 c; Basis B; V3 h; uint32_t col; };         // повёрнутый брус (ежи, проволока, обломки)
struct Plume { V3 p; float size, acc; bool fire; };         // столб дыма на горизонте/пожар
struct Tuft { V3 p; float h; uint32_t col; };              // кустик травы на открытой земле (программный режим)
// маска травяного ковра уровня: 1 текстель на метр; R — густота, G — сухость
static std::vector<uint8_t> g_gmask; static int g_gmW = 0, g_gmH = 0, g_gmSerial = 0; static float g_gmX0 = 0, g_gmZ0 = 0;
static float g_lawnDen = 0; static uint32_t g_lawnA = 0, g_lawnB = 0;
struct Game {
    int screen = SCR_MENU, prevScreen = SCR_MENU; int mission = 0, stage = 0; float stageT = 0, missionT = 0;
    std::vector<Box> box; std::vector<CoverPt> cover; std::vector<Grass> grass;
    std::vector<Prism> prisms;
    std::vector<MTri> mtri; std::vector<MGroup> mgrp; std::vector<Prop> props; std::vector<Plume> plumes; std::vector<Tuft> lawn; bool war = false; float warT = 3;
    std::vector<Actor> act; std::vector<Vehicle> veh; std::vector<Grenade> gren; std::vector<Bullet> bul;
    std::vector<Part> parts; std::vector<Smoke> smoke; std::vector<Tracer> tracers; std::vector<HitInd> hits; std::vector<Sub> subs;
    Player P;
    float groundY = 0; bool hasGround = true; uint32_t gcol1 = 0, gcol2 = 0; bool sea = false; V3 bmin, bmax; float farZ = 160;
    bool snow = false; std::string warn; float warnT = 0;
    bool rain = false; float roll = 0, tilt = 0, waterY = -100; bool waterOn = false; V3 waterMn, waterMx; bool lightning = false;
    float lightT = 0;
    std::string objective; float objT = 0; V3 wp; bool wpOn = false;
    float timer = 0; bool timerOn = false; std::string timerLabel; float progress = 0; bool progOn = false; std::string progLabel;
    float fl[24]; int iv[24];
    std::vector<std::string> story; float storyT = 0, storyDur = 0; int storyNext = -1; bool storyFreeze = false; uint32_t storyBg = 0;
    bool stealth = false, detected = false;
    int kills = 0, shots = 0, hitsN = 0, heads = 0;
    float callContact = 0, callReload = 0, callKill = 0, callNade = 0;
    int frame = 0; float time = 0;
    bool failed = false; std::string failMsg; float endT = -1; bool missionDone = false;
    int cpStage = 0; V3 cpPos; float cpYaw = 0;
    std::string debrief; float fadeIn = 0; float shakeAll = 0;
    int briefLine = 0; float briefT = 0; int menuSel = 0; int epiT = 0; float epiTime = 0;
    float useProg = 0; std::string useHint; int useTarget = -1;
    float fps = 0;
};
static Game G;
static Settings S;
static Progress PR;
static void say(const char* who, const char* text, bool radio = false, bool low = false);
static void rumble(float lo, float hi, int ms);

// ------------------------------------------------------------ геометрия
static int addBox(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t col, int mat = MT_CONCRETE, bool cover = false) {
    Box b;
    b.mn = V3(std::min(x0, x1), std::min(y0, y1), std::min(z0, z1));
    b.mx = V3(std::max(x0, x1), std::max(y0, y1), std::max(z0, z1));
    b.col = col; b.mat = (uint8_t)mat; b.solid = true; b.cover = cover; b.vis = (mat != MT_INVIS); b.glow = false;
    b.tex = (uint8_t)(g_defTex >= 0 ? g_defTex : (mat == MT_CONCRETE ? TX_CONCRETE : mat == MT_WOOD ? TX_WOOD : mat == MT_METAL ? TX_METAL : TX_NONE));
    G.box.push_back(b);
    return (int)G.box.size() - 1;
}
static void addDeco(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t col, bool glow = false) {
    int i = addBox(x0, y0, z0, x1, y1, z1, col);
    G.box[i].solid = false; G.box[i].glow = glow; G.box[i].tex = (uint8_t)(g_defTex >= 0 ? g_defTex : TX_NONE);
}
static bool rayAABB(V3 o, V3 d, V3 mn, V3 mx, float& t0, float& t1) {
    float tmin = -1e30f, tmax = 1e30f;
    for (int a = 0; a < 3; a++) {
        float oo = o[a], dd = d[a], lo = mn[a], hi = mx[a];
        if (fabsf(dd) < 1e-8f) { if (oo < lo || oo > hi) return false; }
        else {
            float ia = 1.0f / dd, ta = (lo - oo) * ia, tb = (hi - oo) * ia;
            if (ta > tb) std::swap(ta, tb);
            tmin = std::max(tmin, ta); tmax = std::min(tmax, tb);
            if (tmin > tmax) return false;
        }
    }
    t0 = tmin; t1 = tmax;
    return tmax >= 0;
}
// mask: 0 — пули (без невидимых), 1 — видимость
static bool worldRay(V3 o, V3 d, float maxT, int& idx, float& tin, float& tout, int mask) {
    float best = maxT; idx = -1;
    for (int i = 0; i < (int)G.box.size(); i++) {
        const Box& b = G.box[i];
        if (!b.solid || b.mat == MT_INVIS) continue;
        if (mask == 0 && b.mat == MT_CLOTH && b.mn.y > 1.9f) continue;
        float t0, t1;
        if (rayAABB(o, d, b.mn, b.mx, t0, t1)) {
            float te = std::max(0.0f, t0);
            if (te < best) { best = te; idx = i; tin = te; tout = t1; }
        }
    }
    return idx >= 0;
}
static bool smokeBlocks(V3 a, V3 b) {
    V3 d = b - a; float L = len(d); if (L < 1e-4f) return false; d = d / L;
    for (auto& s : G.smoke) {
        if (s.r < 0.5f) continue;
        float t = clampf(dot(s.p - a, d), 0, L);
        if (len(a + d * t - s.p) < s.r * 0.85f) return true;
    }
    return false;
}
static bool losClear(V3 a, V3 b, bool smoke = true) {
    V3 d = b - a; float L = len(d); if (L < 1e-3f) return true; d = d / L;
    int idx; float t0, t1;
    if (worldRay(a, d, L - 0.05f, idx, t0, t1, 1)) return false;
    if (smoke && smokeBlocks(a, b)) return false;
    return true;
}
static const float STEP_H = 0.45f;
static bool bodyBlocked(V3 f, float rad, float h) {
    for (const Box& b : G.box) {
        if (!b.solid) continue;
        if (b.mx.y <= f.y + STEP_H || b.mn.y >= f.y + h) continue;
        if (f.x + rad > b.mn.x && f.x - rad < b.mx.x && f.z + rad > b.mn.z && f.z - rad < b.mx.z) return true;
    }
    return false;
}
static float groundAt(V3 f, float rad) {
    float gy = G.hasGround ? G.groundY : -1000.0f;
    float r = rad * 0.6f;
    for (const Box& b : G.box) {
        if (!b.solid || b.mat == MT_INVIS) continue;
        if (b.mx.y > f.y + STEP_H + 0.01f || b.mx.y <= gy) continue;
        if (f.x + r > b.mn.x && f.x - r < b.mx.x && f.z + r > b.mn.z && f.z - r < b.mx.z) gy = b.mx.y;
    }
    return gy;
}
static bool moveBody(V3& p, float& vy, V3 delta, float rad, float h, float dt) {
    bool blocked = false;
    V3 t = p; t.x += delta.x;
    if (!bodyBlocked(t, rad, h)) p.x = t.x; else blocked = true;
    t = p; t.z += delta.z;
    if (!bodyBlocked(t, rad, h)) p.z = t.z; else blocked = true;
    float gy = groundAt(p, rad);
    if (p.y > gy + 0.02f || vy > 0) {
        vy -= 18.0f * dt; p.y += vy * dt;
        if (p.y <= gy) { p.y = gy; vy = 0; }
    } else { p.y = gy; vy = 0; }
    return blocked;
}
static bool inGrass(V3 p) {
    for (auto& g : G.grass) if (p.x > g.mn.x && p.x < g.mx.x && p.z > g.mn.z && p.z < g.mx.z) return true;
    return false;
}

// ------------------------------------------------------------ укрытия
static void buildCover() {
    G.cover.clear();
    for (int i = 0; i < (int)G.box.size(); i++) {
        const Box& b = G.box[i];
        if (!b.cover) continue;
        float h = b.mx.y - b.mn.y;
        if (h < 0.6f || h > 1.6f) continue;
        for (int s = 0; s < 4; s++) {
            V3 n = s == 0 ? V3(1, 0, 0) : s == 1 ? V3(-1, 0, 0) : s == 2 ? V3(0, 0, 1) : V3(0, 0, -1);
            float l = (s < 2) ? (b.mx.z - b.mn.z) : (b.mx.x - b.mn.x);
            int cnt = std::max(1, (int)(l / 2.2f));
            for (int k = 0; k < cnt; k++) {
                float u = (k + 0.5f) / cnt;
                V3 p;
                if (s == 0) p = V3(b.mx.x + 0.5f, b.mn.y, lerpf(b.mn.z, b.mx.z, u));
                else if (s == 1) p = V3(b.mn.x - 0.5f, b.mn.y, lerpf(b.mn.z, b.mx.z, u));
                else if (s == 2) p = V3(lerpf(b.mn.x, b.mx.x, u), b.mn.y, b.mx.z + 0.5f);
                else p = V3(lerpf(b.mn.x, b.mx.x, u), b.mn.y, b.mn.z - 0.5f);
                p.y = groundAt(V3(p.x, b.mn.y + 0.3f, p.z), 0.3f);
                if (fabsf(p.y - b.mn.y) > 0.5f) continue;
                if (bodyBlocked(p, 0.28f, 1.2f)) continue;
                CoverPt c; c.p = p; c.n = n; c.box = i; c.occ = -1;
                G.cover.push_back(c);
            }
        }
    }
}

// ------------------------------------------------------------ частицы
static void spawnPart(V3 p, V3 v, float life, float size, uint32_t col, int mode, float grav = 0, float grow = 0, float drag = 0) {
    if (G.parts.size() > 1200) return;
    Part q; q.p = p; q.v = v; q.life = life; q.maxLife = life; q.size = size; q.col = col; q.mode = (uint8_t)mode; q.grav = grav; q.grow = grow; q.drag = drag;
    G.parts.push_back(q);
}
static void impactFx(V3 p, V3 n, int mat) {
    uint32_t c = mat == MT_WOOD ? RGBc(150, 110, 70) : mat == MT_METAL ? RGBc(255, 220, 140) : RGBc(170, 165, 150);
    for (int i = 0; i < 4; i++)
        spawnPart(p + n * 0.03f, (n + V3(frange(-0.6f, 0.6f), frange(0, 0.8f), frange(-0.6f, 0.6f))) * frange(1.5f, 4.0f), 0.35f, 0.05f, c,
                  mat == MT_METAL ? RM_ADD : RM_OPAQUE, 9.0f);
    spawnPart(p + n * 0.05f, V3(0, 0.4f, 0), 0.6f, 0.15f, RGBc(150, 145, 135), RM_BLEND, 0, 0.5f);
}
static void bloodFx(V3 p, V3 d) {
    for (int i = 0; i < 5; i++)
        spawnPart(p, (d * 1.5f + V3(frange(-1, 1), frange(-0.2f, 1.2f), frange(-1, 1))) * 1.4f, 0.5f, 0.06f, RGBc(120, 10, 10), RM_OPAQUE, 9.0f);
    spawnPart(p, d * 0.5f, 0.3f, 0.25f, RGBc(130, 20, 20), RM_BLEND, 0, 0.6f);
}
static void explosionFx(V3 p) {
    for (int i = 0; i < 26; i++) {
        V3 v(frange(-1, 1), frange(0.2f, 1.4f), frange(-1, 1));
        spawnPart(p + V3(0, 0.3f, 0), v * frange(2, 9), frange(0.2f, 0.6f), frange(0.3f, 0.8f), RGBc(255, (int)frange(120, 200), 40), RM_ADD, 4, 1.5f, 1.5f);
    }
    for (int i = 0; i < 14; i++) {
        V3 v(frange(-1, 1), frange(0.3f, 1.2f), frange(-1, 1));
        spawnPart(p + V3(0, 0.5f, 0), v * frange(1, 3), frange(1.5f, 3.0f), frange(0.8f, 1.6f), RGBc(70, 66, 60), RM_BLEND, -0.4f, 1.2f, 0.8f);
    }
    for (int i = 0; i < 12; i++)
        spawnPart(p, V3(frange(-1, 1), frange(0.5f, 1.5f), frange(-1, 1)) * frange(4, 10), 1.2f, 0.08f, RGBc(60, 50, 40), RM_OPAQUE, 12);
}

// ------------------------------------------------------------ хитбоксы
static float stanceH(int st) { return st == 0 ? 1.8f : (st == 1 ? 1.25f : 0.5f); }
static void actorBoxes(const Actor& a, V3* mn, V3* mx) {
    V3 p = a.pos;
    if (a.kind == AK_DOG) {
        V3 f(sinf(a.yaw), 0, cosf(a.yaw));
        V3 hc = p + f * 0.55f + V3(0, 0.55f, 0), bc = p + V3(0, 0.45f, 0);
        mn[0] = hc - V3(0.15f, 0.15f, 0.15f); mx[0] = hc + V3(0.15f, 0.15f, 0.15f);
        mn[1] = bc - V3(0.4f, 0.18f, 0.4f); mx[1] = bc + V3(0.4f, 0.18f, 0.4f);
        mn[2] = p + V3(-0.3f, 0, -0.3f); mx[2] = p + V3(0.3f, 0.28f, 0.3f);
        return;
    }
    if (a.stance == 2 || a.state == AS_DOWN) {
        V3 f(sinf(a.yaw), 0, cosf(a.yaw));
        V3 hc = p + f * 0.8f + V3(0, 0.25f, 0), tc = p + f * 0.2f + V3(0, 0.22f, 0), lc = p - f * 0.55f + V3(0, 0.15f, 0);
        mn[0] = hc - V3(0.15f, 0.15f, 0.15f); mx[0] = hc + V3(0.15f, 0.15f, 0.15f);
        mn[1] = tc - V3(0.36f, 0.2f, 0.36f); mx[1] = tc + V3(0.36f, 0.2f, 0.36f);
        mn[2] = lc - V3(0.35f, 0.15f, 0.35f); mx[2] = lc + V3(0.35f, 0.15f, 0.35f);
        return;
    }
    float h = stanceH(a.stance);
    if (a.kind == AK_TARGET || a.kind == AK_CIVTARGET) h = 1.75f;
    mn[0] = V3(p.x - 0.13f, p.y + h - 0.27f, p.z - 0.13f); mx[0] = V3(p.x + 0.13f, p.y + h, p.z + 0.13f);
    mn[1] = V3(p.x - 0.25f, p.y + h * 0.5f, p.z - 0.25f); mx[1] = V3(p.x + 0.25f, p.y + h - 0.27f, p.z + 0.25f);
    mn[2] = V3(p.x - 0.18f, p.y, p.z - 0.18f); mx[2] = V3(p.x + 0.18f, p.y + h * 0.5f, p.z + 0.18f);
}
static bool actorRay(const Actor& a, V3 o, V3 d, float maxT, float& t, int& zone) {
    V3 mn[3], mx[3]; actorBoxes(a, mn, mx);
    bool hit = false; t = maxT;
    for (int z = 0; z < 3; z++) {
        float t0, t1;
        if (rayAABB(o, d, mn[z], mx[z], t0, t1) && t0 >= 0 && t0 < t) { t = t0; zone = z; hit = true; }
    }
    return hit;
}
static V3 actorChest(const Actor& a) {
    if (a.kind == AK_DOG) return a.pos + V3(0, 0.45f, 0);
    if (a.stance == 2 || a.state == AS_DOWN) return a.pos + V3(0, 0.25f, 0);
    return a.pos + V3(0, stanceH(a.stance) * 0.72f, 0);
}
static V3 actorEye(const Actor& a) {
    if (a.kind == AK_DOG) return a.pos + V3(0, 0.6f, 0);
    if (a.stance == 2) return a.pos + V3(0, 0.35f, 0);
    return a.pos + V3(0, stanceH(a.stance) - 0.12f, 0);
}
static float playerHeight() { return G.P.stance == 0 ? 1.8f : (G.P.stance == 1 ? 1.25f : 0.5f); }
static V3 playerEye() { return G.P.pos + V3(0, G.P.eyeH, 0); }
static V3 playerChest() { return G.P.pos + V3(0, G.P.eyeH * 0.8f, 0); }
static bool playerRay(V3 o, V3 d, float maxT, float& t) {
    float h = G.P.tank ? 2.4f : playerHeight(); V3 p = G.P.pos;
    float r = G.P.tank ? 1.8f : (G.P.stance == 2 ? 0.45f : 0.28f);
    float t0, t1;
    if (rayAABB(o, d, V3(p.x - r, p.y, p.z - r), V3(p.x + r, p.y + h, p.z + r), t0, t1) && t0 >= 0 && t0 < maxT) { t = t0; return true; }
    return false;
}
static bool vehRay(const Vehicle& v, V3 o, V3 d, float maxT, float& t) {
    if (!v.alive || !v.active || v.kind == VK_JET) return false;
    V3 h = v.kind == VK_HELI ? V3(2.2f, 1.4f, 2.2f) : (v.kind == VK_JEEP ? V3(1.4f, 0.9f, 1.4f) : (v.kind == VK_TANK ? V3(2.6f, 1.2f, 2.6f) : V3(2.4f, 1.5f, 2.4f)));
    V3 c = v.pos + V3(0, v.kind == VK_HELI ? 0.0f : h.y, 0);
    float t0, t1;
    if (rayAABB(o, d, c - h, c + h, t0, t1) && t0 >= 0 && t0 < maxT) { t = t0; return true; }
    return false;
}

// ------------------------------------------------------------ урон
static void damagePlayer(float dmg, V3 from);
static void onActorKilled(int i, int attacker, int zone);
static void damageActor(int i, float dmg, int zone, int attacker, V3 dir) {
    Actor& a = G.act[i];
    if (!a.alive) return;
    a.hitFlash = 0.15f;
    if (a.team == 0 && a.tag == 77 && !a.invuln) { // штурмовая группа: бойцы могут погибнуть
        static const float kAllyDmg[3] = {0.6f, 0.85f, 1.1f};
        a.hp -= dmg * kAllyDmg[clampi(S.difficulty, 0, 2)];
        a.suppressed = std::min(3.0f, a.suppressed + 0.6f); a.callT = 0;
        if (a.hp <= 0) {
            a.alive = false; a.deadT = 0; a.fallDir = atan2f(dir.x, dir.z);
            if (a.cover >= 0 && a.cover < (int)G.cover.size()) G.cover[a.cover].occ = -1;
            static std::string line; line = std::string(a.name) + " убит!";
            say("Седой", line.c_str(), true, true);
        }
        return;
    }
    if (a.team == 0) { // союзники — сюжетная броня
        if (!a.invuln) a.hp = std::max(20.0f, a.hp - dmg * 0.3f);
        a.suppressed = std::min(3.0f, a.suppressed + 0.6f);
        return;
    }
    if (a.invuln) return;
    a.hp -= dmg;
    a.suppressed = std::min(3.0f, a.suppressed + 1.0f);
    if (!a.alerted && a.kind != AK_TARGET && a.kind != AK_CIVTARGET) { a.alerted = true; a.lastSeen = 0; a.lastKnown = attacker == -1 ? G.P.pos : a.pos - dir * 10.0f; }
    if (a.hp <= 0) {
        a.alive = false; a.deadT = 0; a.fallDir = atan2f(dir.x, dir.z);
        if (a.cover >= 0 && a.cover < (int)G.cover.size()) G.cover[a.cover].occ = -1;
        onActorKilled(i, attacker, zone);
    }
}
static void damageVehicle(Vehicle& v, float dmg) {
    if (!v.alive) return;
    v.hp -= dmg;
    if (v.hp <= 0) {
        v.alive = false; v.burn = 30;
        V3 c = v.pos + V3(0, 1, 0);
        explosionFx(c);
        float vol, pan, d; spatial(c, vol, pan, d);
        playSnd(ST_EXPLO, vol * 1.2f, pan, 0.7f, 2.0f);
        if (v.kind == VK_HELI) { v.fallV = 0; say("Седой", "Вертушка готова!"); }
        else say("Док", "Машина горит!");
    }
}

static void fireBullet(V3 o, V3 d, float range, float dmg, float pen, int shooter, int team, bool tracer, bool isPlayer) {
    V3 start = o; float traveled = 0;
    bool hitPlayer = false;
    V3 endP = o + d * range;
    for (int guard = 0; guard < 6 && traveled < range; guard++) {
        float best = range - traveled; int htype = 0, hidx = -1, zone = 1; float tin = 0, tout = 0;
        int bi; float b0, b1;
        if (worldRay(start, d, best, bi, b0, b1, 0)) { best = b0; htype = 1; hidx = bi; tin = b0; tout = b1; }
        for (int i = 0; i < (int)G.act.size(); i++) {
            const Actor& a = G.act[i];
            if (!a.alive || a.team == team || i == shooter || a.hidden || a.state == AS_CARRIED) continue;
            if (team == 0 && a.team == 0) continue;
            if (a.kind == AK_CONTACT && !isPlayer) continue;
            float t; int z;
            if (actorRay(a, start, d, best, t, z)) { best = t; htype = 2; hidx = i; zone = z; }
        }
        if (team != 0) { float t; if (playerRay(start, d, best, t)) { best = t; htype = 3; } }
        for (int i = 0; i < (int)G.veh.size(); i++) {
            if (G.veh[i].team == team) continue;
            float t; if (vehRay(G.veh[i], start, d, best, t)) { best = t; htype = 4; hidx = i; }
        }
        V3 hp = start + d * best;
        endP = hp;
        if (htype == 0) { endP = start + d * (range - traveled); break; }
        if (htype == 1) {
            const Box& b = G.box[hidx];
            V3 n(0, 1, 0);
            float e = 0.02f;
            if (fabsf(hp.x - b.mn.x) < e) n = V3(-1, 0, 0); else if (fabsf(hp.x - b.mx.x) < e) n = V3(1, 0, 0);
            else if (fabsf(hp.z - b.mn.z) < e) n = V3(0, 0, -1); else if (fabsf(hp.z - b.mx.z) < e) n = V3(0, 0, 1);
            else if (fabsf(hp.y - b.mn.y) < e) n = V3(0, -1, 0);
            if (len(hp - g_lisPos) < 40) impactFx(hp, n, b.mat);
            if (isPlayer || len(hp - g_lisPos) < 12) sndAt(ST_IMPACT, hp, 0.25f, 0.12f, 0, 0, b.mat == MT_METAL ? 1300.0f : 0.0f);
            float th = tout - tin;
            float k = b.mat == MT_WOOD ? 1.0f : b.mat == MT_METAL ? 5.0f : b.mat == MT_GLASS ? 0.2f : b.mat == MT_CLOTH ? 0.05f : 1000.0f;
            float need = th * k;
            if (need < pen) {
                pen -= need; dmg *= std::max(0.3f, 1.0f - need / std::max(0.01f, pen + need) * 0.6f);
                traveled += tout + 0.02f; start = start + d * (tout + 0.02f);
                continue;
            }
            break;
        }
        if (htype == 2) {
            float mul = zone == 0 ? 4.0f : (zone == 1 ? 1.0f : 0.65f);
            Actor& a = G.act[hidx];
            if (a.kind != AK_TARGET && a.kind != AK_CIVTARGET) bloodFx(hp, d);
            else impactFx(hp, -d, MT_WOOD);
            if (isPlayer) { G.hitsN++; G.P.hitMarker = 0.2f; }
            damageActor(hidx, dmg * mul, zone, shooter, d);
            if (isPlayer && !a.alive && zone == 0) G.heads++;
            break;
        }
        if (htype == 3) { hitPlayer = true; damagePlayer(dmg, o); break; }
        if (htype == 4) {
            Vehicle& v = G.veh[hidx];
            impactFx(hp, -d, MT_METAL);
            damageVehicle(v, v.kind == VK_TANK ? 0.0f : dmg * (v.kind == VK_HELI ? 0.5f : 0.35f));
            break;
        }
    }
    if (tracer && G.tracers.size() < 64) { Tracer tr; tr.a = o + d * 1.5f; tr.b = endP; tr.t = 0.06f; tr.col = RGBc(255, 210, 120); G.tracers.push_back(tr); }
    // свист пули рядом с игроком
    if (team != 0 && !hitPlayer) {
        V3 eye = playerEye();
        V3 seg = endP - o; float L = len(seg);
        if (L > 0.1f) {
            float t = clampf(dot(eye - o, seg / L), 0, L);
            float dd = len(o + seg / L * t - eye);
            if (dd < 1.6f && t > 3) {
                float vol, pan, ds; spatial(o + seg / L * t, vol, pan, ds);
                playSnd(ST_WHIZ, 0.35f, pan, 0, 0.14f);
                G.P.suppress = std::min(1.0f, G.P.suppress + 0.25f);
            }
        }
    }
}
// баллистическая пуля (снайперская винтовка)
static float g_wind = 0;
static void stepBullets(float dt) {
    for (size_t i = 0; i < G.bul.size();) {
        Bullet& b = G.bul[i];
        float rem = dt; bool dead = false;
        while (rem > 0 && !dead) {
            float h = std::min(rem, 1.0f / 120.0f); rem -= h;
            V3 np = b.p + b.v * h;
            V3 d = np - b.p; float L = len(d);
            if (L > 1e-4f) {
                d = d / L;
                // проверка попадания в отрезке
                int bi; float t0, t1; float best = L; int ht = 0, hidx = -1, zone = 1;
                if (worldRay(b.p, d, L, bi, t0, t1, 0)) { best = t0; ht = 1; hidx = bi; }
                for (int k = 0; k < (int)G.act.size(); k++) {
                    const Actor& a = G.act[k];
                    if (!a.alive || a.team == b.team || a.hidden) continue;
                    float t; int z;
                    if (actorRay(a, b.p, d, best, t, z)) { best = t; ht = 2; hidx = k; zone = z; }
                }
                for (int k = 0; k < (int)G.veh.size(); k++) { float t; if (G.veh[k].team != b.team && vehRay(G.veh[k], b.p, d, best, t)) { best = t; ht = 3; hidx = k; } }
                if (ht) {
                    V3 hp = b.p + d * best;
                    if (ht == 1) { impactFx(hp, -d, G.box[hidx].mat); spawnPart(hp, V3(0, 1, 0), 1.2f, 0.6f, RGBc(150, 140, 120), RM_BLEND, 0, 1.0f); }
                    else if (ht == 2) {
                        Actor& a = G.act[hidx];
                        bloodFx(hp, d);
                        if (b.player) { G.hitsN++; G.P.hitMarker = 0.25f; }
                        float mul = zone == 0 ? 4.0f : (zone == 1 ? 1.0f : 0.65f);
                        damageActor(hidx, b.dmg * mul, zone, b.shooter, d);
                        if (b.player && !a.alive && zone == 0) G.heads++;
                    } else if (G.veh[hidx].kind != VK_TANK) damageVehicle(G.veh[hidx], b.dmg * 0.5f);
                    dead = true; break;
                }
            }
            if (G.tracers.size() < 64) { Tracer tr; tr.a = b.p; tr.b = np; tr.t = 0.12f; tr.col = RGBc(200, 200, 190); G.tracers.push_back(tr); }
            b.p = np;
            b.v.y -= 9.81f * h;
            b.v.x += g_wind * 0.6f * h;
            b.v *= (1.0f - 0.07f * h);
            b.life -= h;
            if (b.life <= 0 || b.p.y < G.groundY - 5) dead = true;
        }
        if (dead) { G.bul[i] = G.bul.back(); G.bul.pop_back(); } else i++;
    }
}

// ------------------------------------------------------------ взрывы и гранаты
static void alertNoise(V3 p, float radius) {
    for (auto& a : G.act) {
        if (!a.alive || a.team != 1 || a.kind == AK_TARGET || a.kind == AK_CIVTARGET) continue;
        if (len(a.pos - p) < radius) {
            if (!a.alerted) { a.alerted = true; a.lastKnown = p; a.lastSeen = 1; }
        }
    }
}
static bool g_explosive = false;
static float g_blastTankMul = 1.0f;
static void explode(V3 p, int team, float radius, float maxDmg, bool byPlayer) {
    explosionFx(p);
    float vol, pan, d; spatial(p, vol, pan, d);
    playSnd(ST_EXPLO, std::min(1.4f, vol * 1.6f), pan, 0.8f, 2.4f);
    g_env.flash = std::max(g_env.flash, 0.25f * clampf(1 - d / 60, 0, 1));
    for (int i = 0; i < (int)G.act.size(); i++) {
        Actor& a = G.act[i];
        if (!a.alive) continue;
        float da = len(actorChest(a) - p);
        if (da < radius && losClear(p + V3(0, 0.3f, 0), actorChest(a), false)) {
            float k = powf(1 - da / radius, 1.3f);
            damageActor(i, maxDmg * k, 1, byPlayer ? -1 : -2, norm(a.pos - p));
            a.flashed = std::max(a.flashed, 1.5f * k);
        }
    }
    for (auto& v : G.veh) if (v.alive && v.active && len(v.pos - p) < radius) damageVehicle(v, maxDmg * 2 * (1 - len(v.pos - p) / radius) * (v.kind == VK_TANK ? g_blastTankMul : 1.0f));
    V3 pc = playerChest();
    float dp = len(pc - p);
    bool los = losClear(p + V3(0, 0.3f, 0), pc, false);
    g_explosive = true;
    if (G.P.tank) { dp = std::max(0.0f, dp - 1.6f); los = true; }
    if (dp < radius && los && team != 0) damagePlayer(maxDmg * 0.75f * powf(1 - dp / radius, 1.5f), p);
    else if (dp < radius * 0.6f && los && team == 0) damagePlayer(maxDmg * 0.6f * powf(1 - dp / radius, 1.5f), p);
    g_explosive = false;
    if (dp < 14) {
        g_amb.ring = std::max(g_amb.ring, clampf(1.3f - dp / 11, 0, 1));
        G.P.shake = std::max(G.P.shake, clampf(1.2f - dp / 15, 0, 1));
        rumble(0.9f, 0.7f, 450);
    } else if (dp < 40) { G.P.shake = std::max(G.P.shake, 0.3f); rumble(0.4f, 0.2f, 200); }
    alertNoise(p, 70);
}
static void flashBang(V3 p) {
    for (int i = 0; i < 18; i++) spawnPart(p + V3(0, 0.3f, 0), V3(frange(-1, 1), frange(0, 1), frange(-1, 1)) * 4, 0.15f, 0.6f, RGBc(255, 255, 240), RM_ADD, 0, 3);
    float vol, pan, d; spatial(p, vol, pan, d);
    playSnd(ST_EXPLO, std::min(1.2f, vol * 1.4f), pan, 0.4f, 0.9f);
    for (auto& a : G.act) {
        if (!a.alive || a.team != 1) continue;
        float da = len(actorEye(a) - p);
        if (da < 16 && losClear(p + V3(0, 0.3f, 0), actorEye(a), false)) { a.flashed = std::max(a.flashed, 5.0f * (1 - da / 20)); a.alerted = true; a.lastKnown = G.P.pos; }
    }
    V3 eye = playerEye();
    float dp = len(eye - p);
    if (dp < 18 && losClear(p + V3(0, 0.3f, 0), eye, false)) {
        float facing = dot(dirYP(G.P.yaw, G.P.pitch), norm(p - eye)) * 0.5f + 0.5f;
        float k = (1 - dp / 20) * (0.25f + 0.75f * facing);
        G.P.flash = std::max(G.P.flash, 4.0f * k);
        g_amb.ring = std::max(g_amb.ring, k);
    }
    alertNoise(p, 40);
}
static void updateGrenades(float dt) {
    for (size_t i = 0; i < G.gren.size();) {
        Grenade& g = G.gren[i];
        g.fuse -= dt;
        if (g.type == 3) { // реактивная граната РПГ: летит прямо, взрывается при контакте
            bool boom = g.fuse <= 0;
            for (int s = 0; s < 4 && !boom; s++) {
                float h = dt / 4; V3 np = g.p + g.v * h;
                float L = len(g.v) * h; int bi; float t0, t1;
                if (worldRay(g.p, norm(g.v), L, bi, t0, t1, 0)) { np = g.p + norm(g.v) * t0; boom = true; }
                if (G.P.tank && len(np - (G.P.pos + V3(0, 1.2f, 0))) < 2.4f) boom = true;
                if (!G.P.tank && len(np - playerChest()) < 0.8f) boom = true;
                if (G.hasGround && np.y < G.groundY) boom = true;
                g.p = np;
            }
            spawnPart(g.p, V3(frange(-0.2f, 0.2f), 0.3f, frange(-0.2f, 0.2f)), 1.2f, 0.35f, RGBc(170, 165, 155), RM_BLEND, -0.1f, 0.6f);
            if (boom) { explode(g.p - norm(g.v) * 0.3f, 1, 4.5f, 210, false); G.gren[i] = G.gren.back(); G.gren.pop_back(); continue; }
            i++; continue;
        }
        if (!g.rest) {
            int sub = 3; float h = dt / sub;
            for (int s = 0; s < sub; s++) {
                g.v.y -= 9.8f * h;
                V3 np = g.p + g.v * h;
                for (const Box& b : G.box) {
                    if (!b.solid || b.mat == MT_INVIS) continue;
                    if (np.x > b.mn.x && np.x < b.mx.x && np.y > b.mn.y && np.y < b.mx.y && np.z > b.mn.z && np.z < b.mx.z) {
                        if (g.p.y >= b.mx.y) { np.y = b.mx.y + 0.01f; g.v.y = -g.v.y * 0.35f; g.v.x *= 0.6f; g.v.z *= 0.6f; }
                        else if (g.p.y <= b.mn.y) { np.y = b.mn.y - 0.01f; g.v.y = -g.v.y * 0.3f; }
                        else if (g.p.x <= b.mn.x || g.p.x >= b.mx.x) { np.x = g.p.x; g.v.x = -g.v.x * 0.4f; }
                        else { np.z = g.p.z; g.v.z = -g.v.z * 0.4f; }
                        if (len(g.v) > 2) sndAt(ST_IMPACT, np, 0.2f, 0.08f, 0, 0, 900);
                        break;
                    }
                }
                if (G.hasGround && np.y < G.groundY + 0.05f) { np.y = G.groundY + 0.05f; g.v.y = -g.v.y * 0.3f; g.v.x *= 0.6f; g.v.z *= 0.6f; }
                g.p = np;
            }
            g.spin += dt * 12;
            float gy = groundAt(g.p, 0.05f);
            if (fabsf(g.p.y - gy) < 0.1f && len(g.v) < 0.8f) { g.rest = true; g.p.y = gy + 0.05f; }
            if (g.p.y < -60) g.fuse = -1;
        }
        if (g.type == 2 && g.fuse <= 0 && g.fuse > -100) {
            Smoke s; s.p = g.p; s.r = 0.5f; s.life = 26; s.emit = 0;
            G.smoke.push_back(s);
            playSnd(ST_THROW, 0.4f, 0, 0, 0.8f);
            g.fuse = -1000;
        }
        bool remove = false;
        if (g.type == 0 && g.fuse <= 0) { explode(g.p, g.team, 8.0f, 240, g.owner); remove = true; }
        else if (g.type == 1 && g.fuse <= 0) { flashBang(g.p); remove = true; }
        else if (g.type == 2 && g.fuse < -1000 + 0.5f) remove = true;
        if (remove) { G.gren[i] = G.gren.back(); G.gren.pop_back(); } else i++;
    }
}
static void throwGrenade(V3 from, V3 vel, int type, float fuse, int team, bool owner) {
    Grenade g; g.p = from; g.v = vel; g.type = type; g.fuse = fuse; g.team = team; g.rest = false; g.spin = 0; g.owner = owner;
    G.gren.push_back(g);
    sndAt(ST_THROW, from, 0.4f, 0.25f);
}

// =====================================================================
//  ИИ: противники, союзники, собаки, техника
// =====================================================================
static const float kDiffDmg[3] = {0.5f, 0.85f, 1.4f};
static const float kDiffSpread[3] = {1.45f, 1.0f, 0.75f};

static int spawnEnemy(V3 p, int uni, int role, bool alerted, int tag) {
    Actor a;
    a.kind = AK_SOLDIER; a.team = 1; a.uni = uni; a.pos = p; a.home = p; a.role = role; a.alerted = alerted; a.tag = tag;
    a.yaw = alerted ? yawTo(p, G.P.pos) : frange(-PI_F, PI_F); a.lastKnown = G.P.pos; a.lastSeen = alerted ? 5.0f : 99.0f;
    a.relocT = frange(3, 8); a.peek = frange(0.5f, 2.0f); a.grenCD = frange(8, 20); a.grenades = (frand() < 0.5f) ? 1 : 0;
    a.ammo = 30; a.speed = role == 1 ? 4.4f : 3.8f;
    G.act.push_back(a);
    return (int)G.act.size() - 1;
}
static int spawnAlly(V3 p, const char* name, int uni, int form) {
    Actor a;
    a.kind = AK_ALLY; a.team = 0; a.uni = uni; a.name = name; a.pos = p; a.formation = form; a.invuln = true;
    a.speed = 4.6f; a.alerted = true; a.yaw = 0;
    G.act.push_back(a);
    return (int)G.act.size() - 1;
}
static int findAlly(const char* name) {
    for (int i = 0; i < (int)G.act.size(); i++) if (G.act[i].kind == AK_ALLY && strcmp(G.act[i].name, name) == 0) return i;
    return -1;
}

static bool steerTo(Actor& a, V3 tgt, float speed, float dt, bool face = true) {
    V3 d = tgt - a.pos; d.y = 0; float L = len(d);
    if (L < 0.35f) { a.speedNow = 0; moveBody(a.pos, a.vy, V3(), 0.3f, 1.6f, dt); return true; }
    V3 dir = d / L;
    if (a.avoidT > 0) { a.avoidT -= dt; float ang = atan2f(dir.x, dir.z) + a.avoidDir; dir = V3(sinf(ang), 0, cosf(ang)); }
    V3 before = a.pos;
    float sp = std::min(speed, L / dt);
    moveBody(a.pos, a.vy, dir * sp * dt, 0.3f, a.stance == 2 ? 0.5f : 1.6f, dt);
    float moved = dist2D(before, a.pos);
    if (moved < sp * dt * 0.3f) { a.stuckT += dt; if (a.avoidT <= 0) { a.avoidT = frange(0.5f, 0.9f); a.avoidDir = (rnd() & 1) ? 1.4f : -1.4f; } }
    else a.stuckT = std::max(0.0f, a.stuckT - dt * 0.5f);
    if (face) a.yaw += wrapPi(atan2f(dir.x, dir.z) - a.yaw) * std::min(1.0f, dt * 8);
    a.speedNow = moved / std::max(dt, 1e-4f);
    a.walkPh += moved * 2.4f;
    return false;
}
static void idlePhys(Actor& a, float dt) { a.speedNow = 0; moveBody(a.pos, a.vy, V3(), 0.3f, 1.6f, dt); }

static void perceive(int i) {
    Actor& a = G.act[i];
    V3 eye = actorEye(a);
    V3 fwd(sinf(a.yaw), 0, cosf(a.yaw));
    float range = G.rain ? 55.0f : 95.0f;
    int bestId = -2; float bestD = 1e9f; V3 bestP;
    auto consider = [&](V3 head, V3 chest, int id) {
        float d = len(chest - eye);
        if (d > range || d >= bestD) return;
        if (!a.alerted && a.team == 1 && (d > 45 || dot(fwd, norm(chest - eye)) < 0.2f)) return;
        if (losClear(eye, chest)) { bestD = d; bestId = id; bestP = chest; }
        else if (losClear(eye, head)) { bestD = d; bestId = id; bestP = head - V3(0, 0.1f, 0); }
    };
    if (a.team == 1) {
        if (!G.P.dead && !(G.stealth && !G.detected)) consider(playerEye(), playerChest(), -1);
        if (!(G.stealth && !G.detected))
            for (int k = 0; k < (int)G.act.size(); k++) {
                const Actor& o = G.act[k];
                if (o.team != 0 || !o.alive || o.hidden || o.state == AS_CARRIED) continue;
                consider(actorEye(o), actorChest(o), k);
            }
    } else {
        for (int k = 0; k < (int)G.act.size(); k++) {
            const Actor& o = G.act[k];
            if (o.team != 1 || !o.alive || o.hidden || o.kind == AK_TARGET || o.kind == AK_CIVTARGET || o.kind == AK_CONTACT || o.kind == AK_VIP) continue;
            if (G.stealth && !G.detected) continue;
            consider(actorEye(o), actorChest(o), k);
        }
    }
    if (bestId != -2) {
        if (!a.sees) a.react = (a.team == 1 ? frange(0.45f, 0.9f) : frange(0.3f, 0.6f)) * (G.rain ? 1.4f : 1.0f);
        a.sees = true; a.tgt = bestId; a.lastKnown = bestP; a.lastSeen = 0;
        if (a.team == 1 && !a.alerted) { a.alerted = true; }
    } else a.sees = false;
}

static void actorShoot(int i, V3 aim, float spreadDeg) {
    Actor& a = G.act[i];
    V3 fwd(sinf(a.yaw), 0, cosf(a.yaw));
    V3 mz = actorEye(a) + fwd * 0.55f - V3(0, 0.18f, 0) + V3(fwd.z, 0, -fwd.x) * 0.12f;
    V3 dir = norm(aim - mz);
    float s = tanf(spreadDeg * DEG) * sqrtf(frand()), ang = frand() * 2 * PI_F;
    V3 u = norm(cross(dir, V3(0, 1, 0))); V3 v = cross(u, dir);
    dir = norm(dir + u * (cosf(ang) * s) + v * (sinf(ang) * s));
    float dmg = a.team == 1 ? 13.0f * a.dmgMul : 30.0f * a.dmgMul;
    fireBullet(mz, dir, 260, dmg, 0.3f, i, a.team, frand() < 0.3f, false);
    sndShot(a.team == 1 ? (a.weapon == W_PISTOL ? 4 : 5) : (a.weapon == W_PISTOL ? 4 : 0), mz, false);
    a.muzzle = 0.06f;
    a.aimPitch = atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z));
}
static V3 targetPoint(const Actor& a) {
    if (a.tgt == -1) return a.lastKnown;
    if (a.tgt >= 0 && a.tgt < (int)G.act.size()) return actorChest(G.act[a.tgt]);
    return a.lastKnown;
}
static float targetSpread(const Actor& a, V3 aim) {
    float d = len(aim - actorEye(a));
    float sp = 2.1f + d * 0.03f;
    if (a.tgt == -1) {
        if (G.P.speed > 2.0f) sp += 1.0f;
        if (G.P.stance == 1) sp += 0.25f;
        if (G.P.stance == 2) sp += 0.8f;
    }
    sp += a.suppressed * 1.1f;
    if (a.team == 1) sp *= kDiffSpread[clampi(S.difficulty, 0, 2)] * a.accMul;
    else if (a.tag == 77) sp *= 1.7f;   // штурмовая группа бьёт неточно на бегу — основная огневая поддержка за игроком
    else sp = 2.0f + d * 0.03f;
    return sp;
}
static void tryFire(int i, float dt, bool suppress) {
    Actor& a = G.act[i];
    (void)dt;
    if (a.fireCD > 0 || a.reloadT > 0) return;
    if (a.ammo <= 0) { a.reloadT = 2.4f; a.ammo = 30; if (a.team == 0 && G.callReload <= 0 && !G.stealth) { say(a.name, "Перезаряжаюсь!", false, true); G.callReload = 9; } return; }
    if (!suppress && a.react > 0) return;
    if (a.burst <= 0) a.burst = (a.weapon == W_PISTOL || a.weapon == W_SNIPER) ? 1 : (a.team == 1 ? 2 + (int)(rnd() % 4) : 2 + (int)(rnd() % 2));
    V3 aim = suppress ? a.lastKnown + V3(frange(-1.5f, 1.5f), frange(-0.5f, 1), frange(-1.5f, 1.5f)) : targetPoint(a);
    float sp = targetSpread(a, aim) + (suppress ? 2.5f : 0.0f);
    actorShoot(i, aim, sp);
    a.ammo--; a.burst--;
    a.fireCD = a.burst > 0 ? (a.weapon == W_PISTOL ? 0.35f : 0.11f) : (a.team == 1 ? frange(0.7f, 1.6f) : frange(0.8f, 1.6f));
}

static bool coverValid(int ci, V3 threat) {
    if (ci < 0 || ci >= (int)G.cover.size()) return false;
    const CoverPt& c = G.cover[ci];
    V3 td = threat - c.p; td.y = 0; float L = len(td);
    if (L < 2.5f) return false;
    return dot(td / L, c.n) < -0.3f;
}
static bool pickCover(int i, V3 threat, bool advance, V3 anchor, float maxFromAnchor) {
    Actor& a = G.act[i];
    float bestS = -1e9f; int best = -1;
    V3 pf = dirYP(G.P.yaw, 0);
    for (int k = 0; k < (int)G.cover.size(); k++) {
        CoverPt& c = G.cover[k];
        if (c.occ >= 0 && c.occ != i) continue;
        if (fabsf(c.p.y - a.pos.y) > 0.9f) continue;
        float dself = dist2D(c.p, a.pos);
        if (dself > 30) continue;
        float danc = dist2D(c.p, anchor);
        if (danc > maxFromAnchor) continue;
        V3 td = threat - c.p; td.y = 0; float dt = len(td);
        if (dt < 4 || dt > 70) continue;
        td = td / dt;
        if (dot(td, c.n) > -0.35f) continue;
        float s = -dself * 0.6f;
        float ideal = a.team == 0 ? 0 : (a.role == 1 ? 13.0f : (advance ? 11.0f : 19.0f));
        if (a.team == 1) s -= fabsf(dt - ideal) * 0.5f; else s -= danc * 1.5f;
        if (advance && a.team == 1) s += (dist2D(a.pos, threat) - dt) * 0.7f;
        if (a.role == 1 && a.team == 1) { V3 fp = norm(c.p - G.P.pos); s += (1.0f - fabsf(dot(fp, pf))) * 10.0f; }
        if (k == a.cover) s -= 4.0f;
        s += frand() * 2.0f;
        if (s <= bestS) continue;
        if (losClear(c.p + V3(0, 0.95f, 0), threat + V3(0, 0.3f, 0), false)) continue;
        bestS = s; best = k;
    }
    if (best >= 0) {
        if (a.cover >= 0 && a.cover < (int)G.cover.size() && G.cover[a.cover].occ == i) G.cover[a.cover].occ = -1;
        G.cover[best].occ = i; a.cover = best; a.target = G.cover[best].p; a.state = AS_MOVE; a.st = 0; a.stuckT = 0;
        return true;
    }
    return false;
}
static void enemyGrenade(int i) {
    Actor& a = G.act[i];
    V3 tgt = a.lastKnown; tgt.y = G.P.pos.y;
    V3 from = actorEye(a) + V3(0, 0.3f, 0);
    V3 d = tgt - from; float hd = sqrtf(d.x * d.x + d.z * d.z);
    if (hd < 6 || hd > 26) return;
    float ang = 40 * DEG;
    float v = sqrtf(9.8f * hd / sinf(2 * ang)) * frange(0.92f, 1.06f);
    V3 h = V3(d.x, 0, d.z) / hd;
    throwGrenade(from, h * (v * cosf(ang)) + V3(0, v * sinf(ang), 0), 0, 3.6f, 1, false);
    a.grenades--; a.grenCD = 14;
    if (len(tgt - G.P.pos) < 14 && G.callNade <= 0) {
        int al = -1;
        for (int k = 0; k < (int)G.act.size(); k++) if (G.act[k].kind == AK_ALLY && G.act[k].alive && G.act[k].state != AS_DOWN) { al = k; break; }
        if (al >= 0) say(G.act[al].name, "Граната! В укрытие!", false, true);
        G.callNade = 6;
    }
}

static void patrolUpdate(Actor& a, float dt) {
    if (a.path.empty()) { idlePhys(a, dt); a.yaw += sinf(G.time * 0.3f + a.home.x) * dt * 0.3f; return; }
    if (a.waitT > 0) { a.waitT -= dt; idlePhys(a, dt); return; }
    V3 t = a.path[a.pathI];
    if (steerTo(a, t, a.kind == AK_DOG ? 1.6f : 1.4f, dt)) {
        a.pathI++;
        if (a.pathI >= (int)a.path.size()) { if (a.loopPath) a.pathI = 0; else { a.pathI = (int)a.path.size() - 1; a.path.clear(); } }
        a.waitT = frange(0.5f, 2.5f);
    }
}

static void enemyUpdate(int i, float dt) {
    Actor& a = G.act[i];
    a.fireCD -= dt; a.grenCD -= dt; a.react -= dt; a.lastSeen += dt; a.st += dt;
    a.suppressed = std::max(0.0f, a.suppressed - dt * 0.6f);
    if (a.reloadT > 0) a.reloadT -= dt;
    if (((G.frame + i) % 5) == 0) perceive(i);
    if (a.flashed > 0) {
        a.flashed -= dt; a.stance = 0; a.yaw += sinf(G.time * 6 + i) * dt * 2.5f; idlePhys(a, dt);
        return;
    }
    if (G.stealth && !G.detected) { patrolUpdate(a, dt); return; }
    if (!a.alerted) { patrolUpdate(a, dt); return; }
    V3 threat = a.lastSeen < 40 ? a.lastKnown : G.P.pos;
    if (a.sees) { a.yaw += wrapPi(yawTo(a.pos, targetPoint(a)) - a.yaw) * std::min(1.0f, dt * 7); }
    if (a.role == 2 && G.P.tank && a.sees && a.tgt == -1 && a.grenCD <= 0 && a.flashed <= 0) {
        V3 eye = actorEye(a), t = playerChest() + V3(0, -0.4f, 0);
        float d = len(t - eye);
        if (d > 12 && d < 110) {
            V3 dir = norm(t - eye + V3(frange(-1, 1), frange(-0.4f, 0.4f), frange(-1, 1)) * (d * 0.02f * kDiffSpread[clampi(S.difficulty, 0, 2)]));
            throwGrenade(eye + dir * 0.7f, dir * 55.0f, 3, 4.0f, 1, false);
            sndShot(3, eye, false);
            a.grenCD = frange(9, 13);
            if (G.callNade <= 0) { say("Седой", "РПГ! Пехота с гранатомётом!", true, true); G.callNade = 10; }
        }
    }
    // отход по маршруту
    if (a.retreat) {
        a.stance = 0;
        if (a.pathI < (int)a.path.size()) {
            if (steerTo(a, a.path[a.pathI], 5.0f, dt, !a.sees)) a.pathI++;
            if (a.sees && frand() < 0.3f) tryFire(i, dt, false);
            if (a.stuckT > 3) { a.retreat = false; a.state = AS_FIGHT; }
            return;
        }
        a.retreat = false; a.state = AS_FIGHT; a.relocT = 0;
    }
    if (a.stationary) {
        a.peek -= dt;
        if (a.peek <= 0) { a.peeking = !a.peeking; a.peek = a.peeking ? frange(1.4f, 3.0f) : frange(1.0f, 2.2f); }
        a.stance = a.peeking ? 0 : 1;
        idlePhys(a, dt);
        if (a.peeking) { if (a.sees) tryFire(i, dt, false); else if (a.lastSeen < 6) tryFire(i, dt, true); }
        return;
    }
    switch (a.state) {
    case AS_IDLE: case AS_PATROL: case AS_SCRIPT:
        if (!pickCover(i, threat, false, a.pos, 30)) { a.state = AS_FIGHT; a.st = 0; }
        break;
    case AS_MOVE: {
        a.stance = 0;
        bool arrived = steerTo(a, a.target, a.speed, dt, !a.sees);
        if (a.sees && a.react <= 0 && frand() < 0.25f) tryFire(i, dt, false);
        if (arrived) { a.state = AS_COVER; a.st = 0; a.peeking = false; a.peek = frange(0.8f, 2.0f); a.relocT = a.role == 1 ? frange(5, 9) : frange(7, 14); }
        else if (a.stuckT > 2.5f || a.st > 12) { if (a.cover >= 0) G.cover[a.cover].occ = -1; a.cover = -1; a.state = AS_FIGHT; a.st = 0; }
    } break;
    case AS_COVER: {
        idlePhys(a, dt);
        if (((G.frame + i) % 20) == 0 && !coverValid(a.cover, threat)) { if (!pickCover(i, threat, false, a.pos, 25)) a.state = AS_FIGHT; break; }
        if (a.reloadT > 0) { a.stance = 1; break; }
        a.peek -= dt;
        if (a.peek <= 0) { a.peeking = !a.peeking; a.peek = a.peeking ? frange(1.2f, 2.8f) : frange(0.9f, 2.2f); if (a.peeking) a.react = std::max(a.react, 0.25f); }
        a.stance = a.peeking ? 0 : 1;
        if (a.peeking) {
            if (a.sees) tryFire(i, dt, false);
            else if (a.lastSeen < 6) tryFire(i, dt, true);
        }
        if (a.grenades > 0 && a.grenCD <= 0 && !a.peeking && a.lastSeen > 1.5f && a.lastSeen < 10 && frand() < 0.02f) enemyGrenade(i);
        a.relocT -= dt;
        if (a.relocT <= 0) {
            bool adv = (rnd() % 3) != 0;
            if (!pickCover(i, threat, adv, a.pos, 30)) a.relocT = frange(3, 6);
        }
    } break;
    case AS_FIGHT: default: {
        a.stance = (a.st < 0.3f) ? 0 : 1;
        if (a.sees) { tryFire(i, dt, false); a.stance = (fmodf(a.st, 4.0f) < 2.5f) ? 0 : 1; }
        else if (a.lastSeen < 5) tryFire(i, dt, true);
        // наступление, если цель потеряна
        if (!a.sees && a.lastSeen > 4) { a.stance = 0; steerTo(a, threat, 3.0f, dt); }
        else idlePhys(a, dt);
        if (a.st > 3 && ((G.frame + i) % 30) == 0) { if (pickCover(i, threat, true, a.pos, 25)) break; a.st = 0; }
        if (a.grenades > 0 && a.grenCD <= 0 && a.lastSeen > 2 && a.lastSeen < 10 && frand() < 0.02f) enemyGrenade(i);
    } break;
    }
}

static void allyUpdate(int i, float dt) {
    Actor& a = G.act[i];
    a.fireCD -= dt; a.react -= dt; a.lastSeen += dt; a.st += dt;
    a.suppressed = std::max(0.0f, a.suppressed - dt);
    if (a.reloadT > 0) a.reloadT -= dt;
    a.callT += dt;
    a.hp = std::min(a.maxhp, a.hp + dt * (a.tag == 77 ? (a.callT > 8 ? 0.8f : 0.0f) : 10.0f));
    if (a.state == AS_DOWN) { idlePhys(a, dt); a.stance = 2; return; }
    if (a.state == AS_CARRIED) { a.pos = G.P.pos; return; }
    if (((G.frame + i) % 5) == 0) perceive(i);
    bool combat = a.sees || a.lastSeen < 4;
    if (G.stealth && !G.detected) combat = false;
    if (a.sees) a.yaw += wrapPi(yawTo(a.pos, targetPoint(a)) - a.yaw) * std::min(1.0f, dt * 6);
    if (a.scripted) {
        float L = dist2D(a.pos, a.target);
        float sp = L > 4 ? a.speed : 2.2f;
        if (G.stealth && !G.detected) sp = 1.5f;
        steerTo(a, a.target, sp, dt, !a.sees);
        if (G.stealth && !G.detected) a.stance = G.P.stance;
        else if (a.speedNow < 0.1f) a.stance = combat ? 1 : 0;
        else a.stance = 0;
        if (combat && a.sees) { a.stance = 0; tryFire(i, dt, false); }
        return;
    }
    V3 F(sinf(G.P.yaw), 0, cosf(G.P.yaw)), R(cosf(G.P.yaw), 0, -sinf(G.P.yaw));
    static const float off[4][2] = {{-1.8f, -2.2f}, {1.8f, -2.4f}, {0.0f, -4.0f}, {-2.5f, -5.0f}};
    const float* o = off[a.formation & 3];
    V3 fpos = G.P.pos + R * o[0] + F * o[1];
    // догнать при сильном отставании
    if (dist2D(a.pos, G.P.pos) > 28) {
        V3 cpos = toCam(a.pos);
        if (cpos.z < 0 || fabsf(cpos.x) > cpos.z) { a.pos = G.P.pos - F * 3.0f + R * o[0]; if (bodyBlocked(a.pos, 0.3f, 1.6f)) a.pos = G.P.pos; a.pos.y = G.P.pos.y; }
    }
    if (combat) {
        bool needCover = a.cover < 0 || dist2D(G.cover[a.cover].p, fpos) > 8 || !coverValid(a.cover, a.lastKnown);
        a.relocT -= dt;
        if (needCover && a.relocT <= 0) { a.relocT = 1.5f; if (!pickCover(i, a.lastKnown, false, fpos, 7)) { a.cover = -1; a.state = AS_FIGHT; } }
        if (a.state == AS_MOVE && a.cover >= 0) {
            a.stance = 0;
            if (steerTo(a, a.target, a.speed, dt, !a.sees)) { a.state = AS_COVER; a.peek = 1; }
            if (a.sees && frand() < 0.3f) tryFire(i, dt, false);
        } else {
            idlePhys(a, dt);
            a.peek -= dt;
            if (a.peek <= 0) { a.peeking = !a.peeking; a.peek = a.peeking ? frange(1.5f, 3.0f) : frange(0.8f, 1.6f); }
            a.stance = (a.state == AS_COVER && !a.peeking) ? 1 : (a.state == AS_COVER ? 0 : 1);
            if (a.sees && (a.peeking || a.state != AS_COVER)) tryFire(i, dt, false);
        }
    } else {
        if (a.cover >= 0 && a.cover < (int)G.cover.size() && G.cover[a.cover].occ == i) G.cover[a.cover].occ = -1;
        a.cover = -1; a.state = AS_IDLE;
        float L = dist2D(a.pos, fpos);
        if (L > 1.8f) { a.stance = G.P.stance == 2 ? 1 : 0; if (G.stealth) a.stance = G.P.stance; steerTo(a, fpos, L > 5 ? a.speed : 2.4f, dt); }
        else { idlePhys(a, dt); a.stance = G.P.stance == 0 ? 0 : (G.stealth ? G.P.stance : 1); a.yaw += wrapPi(G.P.yaw - a.yaw) * std::min(1.0f, dt * 2); }
    }
}

static int g_enemyUni = U_SYND;

static void dogUpdate(int i, float dt) {
    Actor& a = G.act[i];
    a.barkT -= dt; a.fireCD -= dt; a.lastSeen += dt;
    if (((G.frame + i) % 6) == 0) perceive(i);
    if (a.flashed > 0) { a.flashed -= dt; idlePhys(a, dt); return; }
    bool calm = (G.stealth && !G.detected) || !a.alerted;
    if (calm) {
        if (a.handler >= 0 && a.handler < (int)G.act.size() && G.act[a.handler].alive) {
            const Actor& h = G.act[a.handler];
            V3 t = h.pos + V3(cosf(h.yaw), 0, -sinf(h.yaw)) * 1.1f + V3(sinf(h.yaw), 0, cosf(h.yaw)) * 0.6f;
            if (dist2D(a.pos, t) > 0.6f) steerTo(a, t, std::max(1.6f, h.speedNow + 0.4f), dt); else { idlePhys(a, dt); a.yaw = h.yaw; }
        } else patrolUpdate(a, dt);
        return;
    }
    float d = dist2D(a.pos, G.P.pos);
    if (d > 1.3f) steerTo(a, G.P.pos, 7.0f, dt);
    else {
        idlePhys(a, dt);
        a.yaw = yawTo(a.pos, G.P.pos);
        if (a.fireCD <= 0 && !G.P.dead) { damagePlayer(18, a.pos); a.fireCD = 1.1f; }
    }
    if (a.barkT <= 0) { sndAt(ST_BARK, a.pos, 0.8f, 0.64f); a.barkT = frange(1.5f, 3.0f); }
}
static void contactUpdate(int i, float dt) {
    Actor& a = G.act[i];
    if (a.state == AS_FLEE && a.pathI < (int)a.path.size()) {
        a.stance = 0;
        if (steerTo(a, a.path[a.pathI], 4.2f, dt)) a.pathI++;
    } else { idlePhys(a, dt); if (a.state == AS_FLEE) a.stance = 1; }
}

static void onActorKilled(int i, int attacker, int zone) {
    (void)zone;
    Actor& a = G.act[i];
    if (a.team == 1 && (a.kind == AK_SOLDIER || a.kind == AK_DOG) && attacker == -1) G.kills++;
    if (a.team == 1 && a.kind == AK_SOLDIER && G.callKill <= 0 && frand() < 0.45f && !G.stealth) {
        int al = -1;
        for (int k = 0; k < (int)G.act.size(); k++)
            if (G.act[k].kind == AK_ALLY && G.act[k].alive && G.act[k].state != AS_DOWN && G.act[k].state != AS_CARRIED && !G.act[k].hidden) { al = k; if (frand() < 0.5f) break; }
        if (al >= 0) {
            static const char* lines[] = {"Минус один.", "Готов.", "Цель поражена.", "Чисто сработал, Кот.", "Лёг."};
            say(G.act[al].name, lines[rnd() % 5], false, true);
        }
        G.callKill = 7;
    }
}
static void damagePlayer(float dmg, V3 from) {
    Player& P = G.P;
    if (P.dead || G.screen != SCR_PLAY) return;
    if (P.tank) { // броня: пули только стучат по корпусу
        if (!g_explosive) { if (frand() < 0.1f) playSnd(ST_IMPACT, 0.2f, frange(-0.5f, 0.5f), 0, 0.1f, 0, 0, 1500); return; }
        dmg *= 0.35f;
    }
    P.hp -= dmg * kDiffDmg[clampi(S.difficulty, 0, 2)];
    P.sinceHit = 0;
    HitInd h; h.yaw = yawTo(P.pos, from); h.t = 1.4f;
    if (len(from - P.pos) > 0.5f) G.hits.push_back(h);
    P.shake = std::max(P.shake, 0.25f);
    playSnd(ST_HURT, 0.6f, 0, 0, 0.25f);
    rumble(0.6f, 0.4f, 140);
    if (P.hp <= 0) { P.hp = 0; P.dead = true; P.deadT = 0; }
}

// ------------------------------------------------------------ техника
static void vehUpdate(int vi, float dt) {
    Vehicle& v = G.veh[vi];
    if (!v.active) return;
    v.t += dt; v.rotor += dt * 28;
    if (!v.alive) {
        if (v.kind == VK_HELI && v.mode != 99) {
            v.fallV += 9.8f * dt; v.pos.y -= v.fallV * dt; v.yaw += 3.5f * dt; v.roll = sinf(v.t * 3) * 0.3f;
            if (v.pos.y <= G.groundY + 1) { v.pos.y = G.groundY + 1; v.mode = 99; explode(v.pos, 1, 10, 200, true); }
        }
        if (v.burn > 0) {
            v.burn -= dt;
            if (frand() < 0.5f) spawnPart(v.pos + V3(frange(-1, 1), 1.5f, frange(-1, 1)), V3(0, 2.5f, 0), 0.7f, 0.6f, RGBc(255, 140, 40), RM_ADD, -1, 0.5f);
            if (frand() < 0.3f) spawnPart(v.pos + V3(0, 2.5f, 0), V3(frange(-0.3f, 0.3f), 2.0f, frange(-0.3f, 0.3f)), 3.0f, 1.2f, RGBc(40, 38, 36), RM_BLEND, -0.2f, 0.8f);
        }
        return;
    }
    switch (v.kind) {
    case VK_TRUCK: case VK_TECH: case VK_JEEP: case VK_TANK: {
        if (v.pi < (int)v.path.size()) {
            V3 d = v.path[v.pi] - v.pos; d.y = 0; float L = len(d);
            float sp = std::min(v.speed, std::max(2.0f, L * 1.2f));
            if (L < 1.0f) v.pi++;
            else { v.yaw += wrapPi(atan2f(d.x, d.z) - v.yaw) * std::min(1.0f, dt * 3); v.pos += V3(sinf(v.yaw), 0, cosf(v.yaw)) * sp * dt; }
        } else if (!v.arrived) {
            v.arrived = true;
            if (v.cargo > 0) {
                V3 back(-sinf(v.yaw), 0, -cosf(v.yaw)), side(cosf(v.yaw), 0, -sinf(v.yaw));
                for (int k = 0; k < v.cargo; k++) {
                    V3 p = v.pos + back * (3.4f + (k / 2) * 1.0f) + side * ((k & 1) ? 1.0f : -1.0f);
                    p.y = G.groundY;
                    int e = spawnEnemy(p, g_enemyUni, (k % 3 == 2) ? 1 : 0, true, v.tag);
                    G.act[e].lastKnown = G.P.pos; G.act[e].lastSeen = 2;
                }
                v.unloaded = true;
            }
            if (!v.solidAdded && v.kind != VK_JEEP) {
                float c = cosf(v.yaw), s = sinf(v.yaw);
                float wd = v.kind == VK_TANK ? 1.9f : 1.2f, ln = v.kind == VK_TANK ? 3.4f : 2.8f;
                float hx = fabsf(c) * wd + fabsf(s) * ln, hz = fabsf(s) * wd + fabsf(c) * ln;
                addBox(v.pos.x - hx, v.pos.y, v.pos.z - hz, v.pos.x + hx, v.pos.y + 1.9f, v.pos.z + hz, RGBc(0, 0, 0), MT_INVIS, false);
                G.box.back().mat = MT_METAL; G.box.back().vis = false;
                v.solidAdded = true;
                // выталкиваем из габарита машины тех, кто оказался внутри (десант, игрок)
                V3 back(-sinf(v.yaw), 0, -cosf(v.yaw));
                for (auto& a : G.act) {
                    if (!a.alive || !bodyBlocked(a.pos, 0.3f, 1.6f)) continue;
                    V3 dir = a.pos - v.pos; dir.y = 0; dir = len(dir) > 0.1f ? norm(dir) : back;
                    for (int k = 1; k <= 24 && bodyBlocked(a.pos, 0.3f, 1.6f); k++) a.pos += dir * 0.25f;
                }
                if (bodyBlocked(G.P.pos, 0.3f, 1.6f)) { V3 dir = G.P.pos - v.pos; dir.y = 0; dir = len(dir) > 0.1f ? norm(dir) : back; for (int k = 1; k <= 24 && bodyBlocked(G.P.pos, 0.3f, 1.6f); k++) G.P.pos += dir * 0.25f; }
            }
        }
        if (v.kind == VK_TANK && v.team == 1) {
            // башня наводится на игрока, пушка — фугасный выстрел раз в ~7 с
            float want = wrapPi(yawTo(v.pos, G.P.pos) - v.yaw);
            float dlt = wrapPi(want - v.orbitA);
            v.orbitA = wrapPi(v.orbitA + clampf(dlt, -0.6f * dt, 0.6f * dt));
            v.fireCD -= dt;
            V3 Tf = dirYP(v.yaw + v.orbitA, 0);
            V3 mz = v.pos + V3(0, 1.85f, 0) + Tf * 5.0f;
            V3 tgt = playerChest();
            if (v.fireCD <= 0 && fabsf(dlt) < 0.06f && len(tgt - mz) < 140 && losClear(mz, tgt)) {
                V3 d = norm(tgt - mz);
                float sp = 1.6f * kDiffSpread[clampi(S.difficulty, 0, 2)] * DEG;
                d = norm(d + V3(frange(-sp, sp), frange(-sp * 0.5f, sp * 0.5f), frange(-sp, sp)));
                int bi; float t0, t1; float L = len(tgt - mz) + 3;
                V3 hitP = mz + d * L;
                if (worldRay(mz, d, L, bi, t0, t1, 0)) hitP = mz + d * t0;
                explode(hitP, 1, 6.5f, 150, false);
                float vol, pan, dd; spatial(mz, vol, pan, dd);
                playSnd(ST_SHOT, std::min(1.3f, vol * 1.5f), pan, 0.9f, 1.4f, 7, 0.35f, 45, 1.0f);
                for (int q = 0; q < 10; q++) spawnPart(mz, Tf * frange(2, 6) + V3(frange(-1, 1), frange(0, 1), frange(-1, 1)), 1.5f, 1.0f, RGBc(150, 145, 135), RM_BLEND, -0.2f, 1.0f, 1.0f);
                G.P.shake = std::max(G.P.shake, 0.4f); rumble(0.6f, 0.3f, 250);
                v.fireCD = 7.5f;
            }
            v.burn = 0;
        }
        if (v.kind == VK_TECH && v.team == 1) {
            v.fireCD -= dt;
            V3 gun = v.pos + V3(0, 2.6f, 0) - V3(sinf(v.yaw), 0, cosf(v.yaw)) * 1.5f;
            V3 tgt = playerChest();
            if (((G.frame + vi) % 15) == 0) {   // цель — ближайший видимый: игрок или союзник
                float best = len(tgt - gun); v.aimAt = -1;
                for (int k = 0; k < (int)G.act.size(); k++) {
                    const Actor& o = G.act[k];
                    if (o.team != 0 || !o.alive || o.hidden || o.state == AS_CARRIED) continue;
                    float d = len(actorChest(o) - gun) + (o.invuln ? 25.0f : 0.0f);   // уязвимые цели в приоритете
                    if (d < best && losClear(gun, actorChest(o))) { best = d; v.aimAt = k; }
                }
            }
            if (v.aimAt >= 0) {
                if (v.aimAt < (int)G.act.size() && G.act[v.aimAt].alive) tgt = actorChest(G.act[v.aimAt]); else v.aimAt = -1;
            }
            if (v.fireCD <= 0) {
                if (len(tgt - gun) < 90 && losClear(gun, tgt)) {
                    int burst = (int)(v.t * 10) % 14;
                    if (burst < 6) {
                        V3 dir = norm(tgt - gun);
                        float sp = (3.0f + len(tgt - gun) * 0.02f) * kDiffSpread[clampi(S.difficulty, 0, 2)] * DEG;
                        dir = norm(dir + V3(frange(-sp, sp), frange(-sp, sp), frange(-sp, sp)));
                        fireBullet(gun, dir, 200, 12, 0.6f, -100, 1, true, false);
                        sndShot(5, gun, false);
                    }
                    v.fireCD = 0.1f;
                } else v.fireCD = 0.4f;
            }
        }
    } break;
    case VK_HELI: {
        V3 goal = v.pos;
        if (v.mode == 0 && v.pi < (int)v.path.size()) {
            goal = v.path[v.pi];
            if (len(goal - v.pos) < 2.0f) v.pi++;
        } else if (v.mode == 0) v.mode = 2;
        if (v.mode == 1) {
            v.orbitA += dt * 0.18f;
            goal = v.orbitC + V3(sinf(v.orbitA) * v.orbitR, v.orbitH, cosf(v.orbitA) * v.orbitR);
        }
        if (v.mode == 2) goal = v.path.empty() ? v.pos : v.path.back();
        V3 d = goal - v.pos; float L = len(d);
        float sp = std::min(v.speed, L * 0.8f);
        if (L > 0.05f) v.pos += d / L * sp * dt;
        V3 hd(d.x, 0, d.z);
        if (len(hd) > 1.0f) { float ty = atan2f(hd.x, hd.z); v.yaw += wrapPi(ty - v.yaw) * std::min(1.0f, dt * 1.5f); }
        v.pitch = clampf(sp / 30.0f, 0, 0.25f);
        v.roll = sinf(v.t * 0.7f) * 0.05f;
        if (v.mode == 2 && v.cargo > 0 && L < 1.5f) {
            v.leave += dt;
            if (v.leave > 2.0f) {
                for (int k = 0; k < v.cargo; k++) {
                    V3 p = v.pos + V3(frange(-2, 2), 0, frange(-2, 2)); p.y = groundAt(V3(p.x, v.pos.y, p.z), 0.3f);
                    int e = spawnEnemy(p, g_enemyUni, k & 1, true, v.tag);
                    G.act[e].lastKnown = G.P.pos; G.act[e].lastSeen = 2;
                    for (int q = 0; q < 6; q++) spawnPart(p, V3(frange(-2, 2), 0.5f, frange(-2, 2)), 1.2f, 0.8f, RGBc(140, 130, 110), RM_BLEND, 0, 1.0f);
                }
                v.cargo = 0; v.unloaded = true; v.mode = 1; v.orbitC = V3(v.pos.x, 0, v.pos.z);
            }
        }
        if (v.team == 1 && !v.spot) {
            v.fireCD -= dt;
            V3 gun = v.pos + V3(0, -1.0f, 0);
            V3 tgt = playerChest();
            if (v.fireCD <= 0) {
                if (len(tgt - gun) < 80 && losClear(gun, tgt)) {
                    if (fmodf(v.t, 4.0f) < 1.2f) {
                        V3 dir = norm(tgt - gun);
                        float sp2 = 4.0f * kDiffSpread[clampi(S.difficulty, 0, 2)] * DEG;
                        dir = norm(dir + V3(frange(-sp2, sp2), frange(-sp2, sp2), frange(-sp2, sp2)));
                        fireBullet(gun, dir, 200, 12, 0.6f, -100, 1, true, false);
                        sndShot(5, gun, false);
                    }
                    v.fireCD = 0.1f;
                } else v.fireCD = 0.5f;
            }
        }
        if (v.spot) { v.spotP = V3(v.pos.x + sinf(v.t * 0.8f) * 7, G.groundY, v.pos.z + 8 + cosf(v.t * 0.6f) * 6); }
    } break;
    case VK_JET: {
        if (v.pi < (int)v.path.size()) {
            V3 d = v.path[v.pi] - v.pos; float L = len(d);
            if (L < v.speed * dt * 1.5f) v.pi++;
            else { v.pos += d / L * v.speed * dt; v.yaw = atan2f(d.x, d.z); }
            if (v.mode == 0 && len(v.pos - g_lisPos) < 450) { v.mode = 1; playSnd(ST_JET, 1.0f, 0, 0.3f, 3.5f); }
        } else v.active = false;
    } break;
    default: break;
    }
}

// ------------------------------------------------------------ скрытность (миссия 4)
static float g_stealthMax = 0;
static void stealthUpdate(float dt) {
    g_stealthMax = 0;
    if (!G.stealth || G.detected) return;
    Player& P = G.P;
    bool grass = inGrass(P.pos);
    float standF = P.stance == 0 ? 1.0f : (P.stance == 1 ? 0.5f : 0.2f);
    float moveF = P.speed > 0.4f ? 1.5f : 0.7f;
    float grassF = grass ? (P.stance == 2 ? 0.12f : (P.stance == 1 ? 0.35f : 0.8f)) : 1.0f;
    float gh = P.ghillie ? 0.75f : 1.0f;
    V3 pc = playerChest();
    for (auto& a : G.act) {
        if (!a.alive || a.team != 1 || (a.kind != AK_SOLDIER && a.kind != AK_DOG)) continue;
        V3 eye = actorEye(a);
        float d = len(pc - eye);
        float range = a.kind == AK_DOG ? 15.0f : 42.0f;
        float rate = 0;
        if (d < range) {
            if (a.kind == AK_DOG) rate = 1.5f * moveF * (grass ? 0.5f : 1.0f) * (P.stance == 2 ? 0.5f : 1.0f) * (1 - d / range);
            else {
                V3 fwd(sinf(a.yaw), 0, cosf(a.yaw));
                float face = dot(fwd, norm(pc - eye)) > 0.35f ? 1.0f : 0.12f;
                if (face > 0.2f || d < 6) {
                    if (losClear(eye, pc)) rate = 2.2f * standF * moveF * grassF * gh * face * powf(1 - d / range, 1.2f);
                }
            }
        }
        if (rate > 0.01f) { a.alert += rate * dt; if (a.alert > 0.45f) a.yaw += wrapPi(yawTo(a.pos, P.pos) - a.yaw) * std::min(1.0f, dt * 1.5f); }
        else a.alert = std::max(0.0f, a.alert - 0.18f * dt);
        g_stealthMax = std::max(g_stealthMax, a.alert);
        if (a.alert >= 1) { G.detected = true; a.alerted = true; a.lastKnown = P.pos; a.lastSeen = 0; }
    }
    for (auto& v : G.veh) {
        if (!v.spot || !v.active || !v.alive) continue;
        float d = dist2D(v.spotP, P.pos);
        float r = 0;
        if (d < 7.5f) r = (P.stance == 2 && grass) ? 0.06f : (grass ? 0.7f : 2.0f);
        v.burn = clampf(v.burn + (r > 0 ? r * dt : -0.2f * dt), 0, 1);
        g_stealthMax = std::max(g_stealthMax, v.burn);
        if (v.burn >= 1) G.detected = true;
    }
    if (G.detected) {
        for (auto& a : G.act) if (a.alive && a.team == 1) { a.alerted = true; a.lastKnown = P.pos; a.lastSeen = 0; }
        playSnd(ST_BEEP, 0.4f, 0, 0, 0.3f, 0, 0, 660);
    }
}

static void updateActors(float dt) {
    for (int i = 0; i < (int)G.act.size(); i++) {
        Actor& a = G.act[i];
        if (a.hitFlash > 0) a.hitFlash -= dt;
        if (a.muzzle > 0) a.muzzle -= dt;
        if (!a.alive) { a.deadT += dt; continue; }
        if (a.hidden) continue;
        switch (a.kind) {
        case AK_SOLDIER: enemyUpdate(i, dt); break;
        case AK_ALLY: allyUpdate(i, dt); break;
        case AK_DOG: dogUpdate(i, dt); break;
        case AK_CONTACT: contactUpdate(i, dt); break;
        default: break;
        }
    }
    // сбор боеприпасов с павших
    for (auto& a : G.act) {
        if (a.alive || a.looted || a.team != 1 || a.kind != AK_SOLDIER) continue;
        if (dist2D(a.pos, G.P.pos) < 1.4f && fabsf(a.pos.y - G.P.pos.y) < 1.5f) {
            a.looted = true;
            WState& w = G.P.w[G.P.slot];
            if (w.def >= 0) {
                int add = WD[w.def].mag;
                if (w.def == W_SNIPER) add = 5;
                w.reserve = std::min(WD[w.def].reserveMax, w.reserve + add);
            }
            playSnd(ST_CLICK, 0.3f, 0, 0, 0.1f, 0.05f);
        }
    }
}
static void updateEffects(float dt) {
    for (size_t i = 0; i < G.parts.size();) {
        Part& p = G.parts[i];
        p.life -= dt;
        if (p.life <= 0) { G.parts[i] = G.parts.back(); G.parts.pop_back(); continue; }
        p.v.y -= p.grav * dt;
        if (p.drag > 0) p.v *= std::max(0.0f, 1 - p.drag * dt);
        p.p += p.v * dt;
        p.size += p.grow * dt;
        if (p.grav > 1 && G.hasGround && p.p.y < G.groundY) { p.p.y = G.groundY; p.v = V3(); }
        i++;
    }
    for (size_t i = 0; i < G.tracers.size();) {
        G.tracers[i].t -= dt;
        if (G.tracers[i].t <= 0) { G.tracers[i] = G.tracers.back(); G.tracers.pop_back(); } else i++;
    }
    for (size_t i = 0; i < G.smoke.size();) {
        Smoke& s = G.smoke[i];
        s.life -= dt;
        s.r = s.life > 4 ? std::min(5.5f, s.r + dt * 2.0f) : std::max(0.0f, s.r - dt * 1.3f);
        s.emit -= dt;
        if (s.emit <= 0 && s.life > 3) {
            s.emit = 0.12f;
            V3 o(frange(-1, 1), frange(0, 1), frange(-1, 1));
            spawnPart(s.p + o * s.r * 0.6f + V3(0, 0.6f, 0), V3(frange(-0.3f, 0.3f), 0.25f, frange(-0.3f, 0.3f)), 3.5f, s.r * 0.9f, RGBc(190, 190, 185), RM_BLEND, 0, 0.3f);
        }
        if (s.life <= 0) { G.smoke[i] = G.smoke.back(); G.smoke.pop_back(); } else i++;
    }
    for (size_t i = 0; i < G.hits.size();) {
        G.hits[i].t -= dt;
        if (G.hits[i].t <= 0) { G.hits[i] = G.hits.back(); G.hits.pop_back(); } else i++;
    }
}

// =====================================================================
//  ВВОД (сводный), ИГРОК, ОРУЖИЕ
// =====================================================================
struct Input {
    float mx = 0, my = 0, lookX = 0, lookY = 0;
    bool fire = false, ads = false, reload = false, stance = false, prone = false, nadeHeld = false, tacHeld = false, sw = false;
    bool sprint = false, breath = false, leanL = false, leanR = false, use = false, useHeld = false, pause = false, tacCycle = false;
    int sel = 0;   // 1/2 — прямой выбор оружия или снаряда
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
};
static Input IN;
static SDL_GameController* g_pad = nullptr;
static void rumble(float lo, float hi, int ms) {
#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (g_pad && S.vibration) SDL_GameControllerRumble(g_pad, (Uint16)(clampf(lo, 0, 1) * 65535), (Uint16)(clampf(hi, 0, 1) * 65535), (Uint32)ms);
#else
    (void)lo; (void)hi; (void)ms;
#endif
}
static uint32_t speakerCol(const char* who) {
    if (!strcmp(who, "Седой")) return RGBc(255, 205, 90);
    if (!strcmp(who, "Док")) return RGBc(120, 225, 120);
    if (!strcmp(who, "Сова")) return RGBc(140, 200, 255);
    if (!strcmp(who, "Майор Грин")) return RGBc(190, 190, 255);
    if (!strcmp(who, "Ворон")) return RGBc(255, 90, 80);
    if (!strcmp(who, "Варгас")) return RGBc(235, 150, 60);
    if (!strcmp(who, "Кот")) return RGBc(255, 255, 255);
    return RGBc(215, 215, 215);
}
static void say(const char* who, const char* text, bool radio, bool low) {
    if (low && G.subs.size() >= 2) return;
    Sub s; s.who = who; s.text = text; s.t = -1; s.dur = 1.8f + utf8Len(text) * 0.055f; s.radio = radio; s.col = speakerCol(who);
    G.subs.push_back(s);
}
static void subsUpdate(float dt) {
    if (G.subs.empty()) return;
    Sub& s = G.subs[0];
    if (s.t < 0) { s.t = 0; if (s.radio) playSnd(ST_RADIO, 0.35f, 0, 0, 0.24f); }
    s.t += dt;
    if (s.t > s.dur) G.subs.erase(G.subs.begin());
}

static float g_swayX = 0, g_swayY = 0;
static bool g_scopeHigh = false;
static float baseFov() { return 75.0f; }
static float curFov() {
    const Player& P = G.P;
    if (P.tank) return P.adsT > 0.5f ? 20.0f : 72.0f;
    const WState& W = P.w[P.slot];
    float f = baseFov();
    if (W.def >= 0) {
        float af = WD[W.def].adsFov;
        if (W.def == W_SNIPER && g_scopeHigh) af = 3.2f;
        f = lerpf(f, af, P.adsT);
    }
    if (P.sprint) f += 4;
    return f;
}
static float aimYaw() { return G.P.yaw + G.P.recY + g_swayX; }
static float aimPitch() { return G.P.pitch + G.P.recP + g_swayY; }
static bool scopeView() { const WState& W = G.P.w[G.P.slot]; return W.def == W_SNIPER && G.P.adsT > 0.85f; }

static void giveWeapon(int slot, int def, int reserve) {
    WState w; w.def = def; w.mag = def >= 0 ? WD[def].mag : 0; w.reserve = reserve;
    G.P.w[slot] = w;
}
static void startReload(WState& W) {
    const WeaponDef& D = WD[W.def];
    if (W.reloadT > 0 || W.reserve <= 0) return;
    if (D.shellReload) { if (W.mag >= D.mag) return; W.reloadEmpty = W.mag == 0; W.reloadT = W.reloadEmpty ? 0.9f : 0.5f; W.reloadDur = W.reloadT; }
    else {
        if (W.mag >= D.mag + 1) return;
        W.reloadEmpty = (W.mag == 0);
        W.reloadDur = W.reloadEmpty ? D.rEmpty : D.rTac;
        W.reloadT = W.reloadDur;
    }
    playSnd(ST_CLICK, 0.45f, 0.2f, 0, 0.3f, 0.2f);
}
static void finishReload(WState& W) {
    const WeaponDef& D = WD[W.def];
    if (D.shellReload) {
        if (W.reserve > 0 && W.mag < D.mag) { W.mag++; W.reserve--; playSnd(ST_CLICK, 0.35f, 0.2f, 0, 0.05f, 0.02f); }
        if (W.reserve > 0 && W.mag < D.mag) { W.reloadT = 0.5f; W.reloadDur = 0.5f; }
        return;
    }
    if (W.reloadEmpty) { int take = std::min(W.reserve, D.mag); W.mag = take; W.reserve -= take; playSnd(ST_BOLT, 0.5f, 0.2f, 0, 0.32f); }
    else { int take = std::min(W.reserve, D.mag + 1 - W.mag); W.mag += take; W.reserve -= take; playSnd(ST_CLICK, 0.45f, 0.2f, 0, 0.1f, 0.05f); }
}

static void playerFire(WState& W) {
    Player& P = G.P;
    const WeaponDef& D = WD[W.def];
    V3 eye = playerEye();
    V3 R(cosf(P.yaw), 0, -sinf(P.yaw));
    eye = eye + R * (P.lean * 0.45f);
    V3 dir = dirYP(aimYaw(), aimPitch());
    float stM = P.stance == 0 ? 1.0f : (P.stance == 1 ? 0.75f : 0.55f);
    float spread = (lerpf(D.hip, D.ads, P.adsT) + D.moveAdd * std::min(1.0f, P.speed / 4.0f) * (1 - P.adsT * 0.5f) + P.bloom) * stM;
    if (D.pellets > 1) spread = lerpf(D.hip, D.ads, P.adsT) * 0.3f;
    for (int k = 0; k < D.pellets; k++) {
        float sp = spread;
        if (D.pellets > 1) sp += lerpf(4.5f, 3.2f, P.adsT);
        float s = tanf(sp * DEG) * sqrtf(frand()), a = frand() * 2 * PI_F;
        V3 u = norm(cross(dir, V3(0, 1, 0))); V3 v = cross(u, dir);
        V3 d = norm(dir + u * (cosf(a) * s) + v * (sinf(a) * s));
        if (D.ballistic) {
            Bullet b; b.p = eye; b.v = norm(d + V3(0, 0.00068f, 0)) * 850.0f; b.dmg = D.dmg; b.pen = D.pen; b.life = 3.0f; b.team = 0; b.shooter = -1; b.player = true;
            G.bul.push_back(b);
        } else fireBullet(eye - V3(0, 0.05f, 0), d, D.range, D.dmg, D.pen, -1, 0, D.pellets == 1 && (G.shots % 3 == 0), true);
    }
    W.mag--; G.shots++;
    sndShot(W.silenced ? 6 : D.snd, eye, true);
    alertNoise(P.pos, W.silenced ? 9.0f : 75.0f);
    float kick = D.recV * (1 - 0.35f * P.adsT) * stM;
    P.pitch += kick * 0.55f * DEG;
    P.recP += kick * 0.45f * DEG;
    float hk = frange(-D.recH, D.recH) * (1 - 0.3f * P.adsT);
    P.yaw += hk * 0.5f * DEG; P.recY += hk * 0.5f * DEG;
    P.kick = 1; P.muzzle = 0.05f;
    P.bloom = std::min(4.0f, P.bloom + D.hip * 0.18f);
    if (D.cycle > 0) W.boltT = D.cycle;
    if (D.cycle > 0 && W.mag > 0) playSnd(ST_BOLT, 0.45f, 0.15f, 0, 0.35f);
    rumble(D.recV > 3 ? 0.7f : 0.25f, D.recV > 3 ? 0.6f : 0.45f, D.recV > 3 ? 160 : 60);
    spawnPart(eye + dirYP(P.yaw, P.pitch) * 0.6f + R * 0.18f - V3(0, 0.15f, 0), R * 2.5f + V3(0, 1.5f, 0), 0.6f, 0.03f, RGBc(200, 160, 60), RM_OPAQUE, 9.0f);
}


// ------------------------------------------------------------ танк игрока
static V3 tankCamPos() {
    const Player& P = G.P;
    if (P.adsT > 0.5f) return P.pos + V3(0, 2.35f, 0) + dirYP(P.turret, 0) * 1.3f;
    V3 c = P.pos + V3(0, 4.6f, 0) - dirYP(P.yaw, clampf(P.pitch, -0.3f, 0.5f) * 0.6f) * 10.5f;
    float gy = groundAt(V3(c.x, c.y, c.z), 0.2f);
    if (c.y < gy + 0.6f) c.y = gy + 0.6f;
    return c;
}
static V3 tankAimPoint() {
    V3 o = tankCamPos(), d = dirYP(G.P.yaw, G.P.pitch);
    int bi; float t0, t1; float best = 450;
    if (worldRay(o, d, best, bi, t0, t1, 0)) best = t0;
    for (auto& v : G.veh) { float t; if (v.team == 1 && vehRay(v, o, d, best, t)) best = t; }
    for (auto& a : G.act) { float t; int z; if (a.alive && a.team == 1 && !a.hidden && actorRay(a, o, d, best, t, z)) best = t; }
    if (G.hasGround && d.y < -1e-4f) { float tg = (G.groundY - o.y) / d.y; if (tg > 0 && tg < best) best = tg; }
    return o + d * best;
}
static const char* kShellName[2] = {"БОПС", "ОФС"};
static const char* kShellFull[2] = {"БРОНЕБОЙНЫЙ ПОДКАЛИБЕРНЫЙ", "ОСКОЛОЧНО-ФУГАСНЫЙ"};
static void tankCannon() {
    Player& P = G.P;
    int type = P.loaded;
    V3 mz = P.pos + V3(0, 1.85f, 0) + dirYP(P.turret, 0) * 5.0f;
    V3 d = norm(tankAimPoint() - mz);
    float sp = (0.2f + std::min(1.0f, fabsf(P.tspeed) / 9) * 0.6f) * DEG;
    d = norm(d + V3(frange(-sp, sp), frange(-sp, sp), frange(-sp, sp)));
    float best = 450; int bi; float t0, t1; int vhit = -1, ahit = -1, zone = 1;
    if (worldRay(mz, d, best, bi, t0, t1, 0)) best = t0;
    for (int i = 0; i < (int)G.act.size(); i++) { const Actor& a = G.act[i]; float t; int z; if (a.alive && a.team == 1 && !a.hidden && actorRay(a, mz, d, best, t, z)) { best = t; ahit = i; zone = z; } }
    for (int i = 0; i < (int)G.veh.size(); i++) { float t; if (G.veh[i].team == 1 && vehRay(G.veh[i], mz, d, best, t)) { best = t; vhit = i; ahit = -1; } }
    if (G.hasGround && d.y < -1e-4f) { float tg = (G.groundY - mz.y) / d.y; if (tg > 0 && tg < best) { best = tg; vhit = -1; ahit = -1; } }
    V3 hp = mz + d * best;
    if (type == 0) {
        // БОПС: пробивает броню, почти без осколков
        if (vhit >= 0) { damageVehicle(G.veh[vhit], 400); G.P.hitMarker = 0.3f; for (int k = 0; k < 14; k++) spawnPart(hp, (-d + V3(frange(-1, 1), frange(0, 1), frange(-1, 1))) * frange(3, 9), 0.4f, 0.06f, RGBc(255, 220, 140), RM_ADD, 9.0f); }
        if (ahit >= 0) { damageActor(ahit, 400, zone, -1, d); G.P.hitMarker = 0.3f; }
        g_blastTankMul = 0.0f;
        explode(hp - d * 0.4f, 0, 2.5f, 70, true);
        if (G.tracers.size() < 64) { Tracer tr; tr.a = mz + d * 2.0f; tr.b = hp; tr.t = 0.15f; tr.col = RGBc(255, 235, 170); G.tracers.push_back(tr); }
    } else {
        // ОФС: большой радиус по пехоте и лёгкой технике, слабо по танкам
        if (vhit >= 0) { damageVehicle(G.veh[vhit], G.veh[vhit].kind == VK_TANK ? 60.0f : 350.0f); G.P.hitMarker = 0.3f; }
        g_blastTankMul = 0.12f;
        explode(hp - d * 0.4f, 0, 8.5f, 300, true);
    }
    g_blastTankMul = 1.0f;
    playSnd(ST_SHOT, 1.3f, 0, 0.9f, 1.5f, 6, 0.32f, type == 0 ? 55.0f : 40.0f, 1.0f);
    for (int k = 0; k < 12; k++) spawnPart(mz, d * frange(2, 6) + V3(frange(-1, 1), frange(0, 1), frange(-1, 1)), 1.6f, 1.1f, RGBc(160, 155, 145), RM_BLEND, -0.2f, 1.0f, 1.0f);
    P.shake = std::max(P.shake, 0.5f); P.muzzle = 0.08f; G.shots++;
    rumble(1.0f, 0.6f, 300);
    alertNoise(P.pos, 150);
}
static void tankSelectShell(int s) {
    Player& P = G.P;
    if (s == P.shell || P.shells[s] <= 0) return;
    P.shell = s;
    if (P.loaded != s || P.treload > 0) { P.treload = 3.5f; P.loaded = s; }   // выгрузка и досылка нового снаряда
    playSnd(ST_BOLT, 0.5f, 0, 0, 0.32f);
    say("Кот", s == 0 ? "Заряжаю бронебойный!" : "Заряжаю фугасный!", false, true);
}
static void tankUpdate(float dt) {
    Player& P = G.P;
    P.stance = 0; P.eyeH = 2.3f; P.lean = 0; P.carry = -1; P.recP = P.recY = 0; g_swayX = g_swayY = 0;
    P.adsT = approach(P.adsT, IN.ads ? 1.0f : 0.0f, dt * 5);
    float sens = P.adsT > 0.5f ? 0.28f : 1.0f;
    P.yaw = wrapPi(P.yaw + IN.lookX * DEG * sens);
    P.pitch = clampf(P.pitch - IN.lookY * DEG * sens * (S.invertY ? -1.0f : 1.0f), -12 * DEG, 22 * DEG);
    float dy = wrapPi(P.yaw - P.turret);
    P.turret = wrapPi(P.turret + clampf(dy, -1.3f * dt, 1.3f * dt));
    P.hull = wrapPi(P.hull + IN.mx * 0.85f * dt * (P.tspeed < -0.5f ? -1.0f : 1.0f));
    float target = IN.my > 0 ? IN.my * 9.0f : IN.my * 4.0f;
    P.tspeed = approach(P.tspeed, target, dt * (fabsf(target) > fabsf(P.tspeed) ? 3.5f : 6.0f));
    V3 fwd(sinf(P.hull), 0, cosf(P.hull));
    V3 before = P.pos;
    if (moveBody(P.pos, P.vy, fwd * P.tspeed * dt, 1.7f, 2.2f, dt)) P.tspeed *= 0.5f;
    P.speed = dist2D(before, P.pos) / std::max(dt, 1e-4f);
    P.vel = (P.pos - before) / std::max(dt, 1e-4f);
    if (IN.sw) tankSelectShell(1 - P.shell);
    if (IN.sel == 1) tankSelectShell(0);
    if (IN.sel == 2) tankSelectShell(1);
    P.treload -= dt; P.mgCD -= dt; P.muzzle -= dt;
    if (!IN.fire) P.tankTrig = false;
    bool ready = P.treload <= 0 && P.shells[P.loaded] > 0;
    if (IN.fire && !P.tankTrig && ready) {
        tankCannon(); P.shells[P.loaded]--; P.tankTrig = true;
        if (P.shells[P.shell] <= 0 && P.shells[1 - P.shell] > 0) { P.shell = 1 - P.shell; say("Кот", "Этот тип кончился, перехожу на другой!", false, true); }
        P.loaded = P.shell;
        P.treload = P.shells[P.shell] > 0 ? 3.5f : 0.0f;
    }
    else if (IN.fire && !ready && P.mgCD <= 0) {
        V3 R(cosf(P.turret), 0, -sinf(P.turret));
        V3 mg = P.pos + V3(0, 2.0f, 0) + dirYP(P.turret, 0) * 1.4f + R * 0.35f;
        V3 d = norm(tankAimPoint() - mg);
        float sp = 0.8f * DEG;
        d = norm(d + V3(frange(-sp, sp), frange(-sp, sp), frange(-sp, sp)));
        fireBullet(mg, d, 300, 30, 0.3f, -1, 0, (G.shots % 3) == 0, true);
        sndShot(0, mg, true); G.shots++;
        P.mgCD = 0.09f;
        alertNoise(P.pos, 80);
    }
    P.sinceHit += dt;
    if (P.sinceHit > 5) P.hp = std::min(100.0f, P.hp + 12 * dt);
    g_amb.heli = 0.4f + 0.35f * std::min(1.0f, P.speed / 9); g_amb.heliRate = 8 + P.speed * 1.5f;
    g_amb.breath = 0; g_amb.heart = P.hp < 35 ? (35 - P.hp) / 35.0f : 0.0f;
    P.flash = std::max(0.0f, P.flash - dt); P.shake = std::max(0.0f, P.shake - dt * 1.5f);
    P.suppress = 0; P.hitMarker = std::max(0.0f, P.hitMarker - dt);
    g_amb.ring = std::max(0.0f, g_amb.ring - dt * 0.2f);
}

static void playerUpdate(float dt) {
    Player& P = G.P;
    if (P.dead) { P.deadT += dt; return; }
    if (P.tank) { tankUpdate(dt); return; }
    WState& W = P.w[P.slot];
    const WeaponDef* D = W.def >= 0 ? &WD[W.def] : nullptr;
    bool carrying = P.carry >= 0;
    bool dragging = carrying && G.iv[22] == 2;
    // ---- прицеливание
    bool wantAds = IN.ads && !P.sprint && W.reloadT <= 0 && P.switchT <= 0 && D && P.cookType < 0 && !dragging;
    P.adsT = approach(P.adsT, wantAds ? 1.0f : 0.0f, dt / (D ? D->adsTime : 0.2f));
    // ---- обзор
    float fov = curFov();
    float sensMul = lerpf(1.0f, std::max(0.1f, fov / baseFov() * 0.95f), P.adsT);
    P.yaw = wrapPi(P.yaw + IN.lookX * DEG * sensMul);
    P.pitch -= IN.lookY * DEG * sensMul * (S.invertY ? -1.0f : 1.0f);
    float pmin = P.stance == 2 ? -30 * DEG : -85 * DEG, pmax = P.stance == 2 ? 40 * DEG : 85 * DEG;
    P.pitch = clampf(P.pitch, pmin, pmax);
    P.recP = approach(P.recP, 0, dt * (0.05f + fabsf(P.recP) * 7));
    P.recY = approach(P.recY, 0, dt * (0.05f + fabsf(P.recY) * 7));
    P.kick = approach(P.kick, 0, dt * 9);
    P.bloom = approach(P.bloom, 0, dt * 4);
    // ---- раскачка прицела и задержка дыхания
    P.swayT += dt;
    float amp = 0.05f;
    if (D && W.def == W_SNIPER) amp = P.stance == 0 ? 0.9f : (P.stance == 1 ? 0.55f : 0.28f);
    else if (D) amp = P.stance == 2 ? 0.03f : 0.08f;
    if (g_scopeHigh) amp *= 0.35f;
    P.holding = false;
    if (P.breathLock > 0) { P.breathLock -= dt; amp *= 2.0f; P.breath = std::min(1.0f, P.breath + dt / 6); }
    else if (IN.breath && P.adsT > 0.8f && D && W.def == W_SNIPER && P.breath > 0) {
        P.holding = true; P.breath -= dt / 5.0f; amp *= 0.06f;
        if (P.breath <= 0) { P.breath = 0; P.breathLock = 3; }
    } else P.breath = std::min(1.0f, P.breath + dt / 4);
    amp *= (0.3f + 0.7f * P.adsT);
    if (P.carry >= 0 && !dragging) amp *= 2.0f;
    g_swayX = (sinf(P.swayT * 0.9f) * 0.7f + sinf(P.swayT * 2.3f) * 0.3f) * amp * DEG;
    g_swayY = (sinf(P.swayT * 1.7f) * 0.6f + cosf(P.swayT * 0.6f) * 0.4f) * amp * 0.7f * DEG;
    // ---- стойки
    if (IN.stance) { P.stance = P.stance == 1 ? 0 : 1; playSnd(ST_STEP, 0.25f, 0, 0, 0.15f, 0, 0.3f); }
    if (IN.prone && !carrying) { P.stance = P.stance == 2 ? 1 : 2; playSnd(ST_STEP, 0.35f, 0, 0, 0.2f, 0, 0.2f); }
    if (carrying && P.stance == 2) P.stance = 1;
    if (P.stance != 2 && bodyBlocked(P.pos, 0.25f, playerHeight())) { if (P.stance == 0) P.stance = 1; }
    // ---- наклоны
    if (IN.leanL) P.leanDir = P.leanDir == -1 ? 0 : -1;
    if (IN.leanR) P.leanDir = P.leanDir == 1 ? 0 : 1;
    if (P.sprint || P.stance == 2 || carrying) P.leanDir = 0;
    float leanTarget = (float)P.leanDir;
    if (P.leanDir != 0) {
        V3 R(cosf(P.yaw), 0, -sinf(P.yaw));
        V3 hp = playerEye() + R * (0.5f * P.leanDir);
        for (const Box& b : G.box) if (b.solid && hp.x > b.mn.x && hp.x < b.mx.x && hp.y > b.mn.y && hp.y < b.mx.y && hp.z > b.mn.z && hp.z < b.mx.z) { leanTarget = 0; break; }
    }
    P.lean = approach(P.lean, leanTarget, dt * 4);
    // ---- движение
    V3 F(sinf(P.yaw), 0, cosf(P.yaw)), R(cosf(P.yaw), 0, -sinf(P.yaw));
    V3 wish = F * IN.my + R * IN.mx;
    float mag = len(wish); if (mag > 1) wish = wish / mag;
    bool sprintReq = IN.sprint && IN.my > 0.5f && !carrying && P.adsT < 0.3f && P.cookType < 0;
    if (P.sprintLock > 0) P.sprintLock -= dt;
    if (IN.fire && P.sprint) sprintReq = false;
    P.sprint = sprintReq && P.stamina > 0.02f && P.sprintLock <= 0;
    if (P.sprint) {
        if (P.stance != 0 && !bodyBlocked(P.pos, 0.25f, 1.8f)) P.stance = 0;
        if (W.reloadT > 0 && !WD[W.def].shellReload) { /* CoD: бег прерывает перезарядку */ W.reloadT = 0; }
        P.stamina -= dt / 6.0f;
        if (P.stamina <= 0) { P.stamina = 0; P.sprintLock = 2.0f; }
    } else P.stamina = std::min(1.0f, P.stamina + dt / 5.0f);
    float spd = 4.2f * (P.stance == 0 ? 1.0f : (P.stance == 1 ? 0.55f : 0.22f));
    spd *= lerpf(1.0f, 0.5f, P.adsT);
    if (carrying) spd *= dragging ? 0.38f : 0.5f;
    if (P.sprint) spd *= 1.55f;
    if (G.waterOn && P.pos.y < G.waterY - 0.5f) spd *= 0.6f;
    V3 tv = wish * spd;
    float acc = 14.0f;
    P.vel.x = lerpf(P.vel.x, tv.x, std::min(1.0f, acc * dt));
    P.vel.z = lerpf(P.vel.z, tv.z, std::min(1.0f, acc * dt));
    V3 delta = P.vel * dt;
    if (G.tilt != 0) delta.x += sinf(G.tilt) * 1.6f * dt;
    V3 before = P.pos;
    moveBody(P.pos, P.vy, delta, 0.3f, playerHeight(), dt);
    P.speed = dist2D(before, P.pos) / std::max(dt, 1e-4f);
    if (G.hasGround == false && P.pos.y < -2.5f && !G.sea) P.pos.y = -2.5f;
    float eyeT = P.stance == 0 ? 1.62f : (P.stance == 1 ? 1.05f : 0.35f);
    P.eyeH = approach(P.eyeH, eyeT, dt * 3.5f);
    P.bobPh += P.speed * dt * 1.9f;
    P.bobAmt = approach(P.bobAmt, std::min(1.0f, P.speed / 4.0f), dt * 4);
    P.stepT -= P.speed * dt;
    if (P.stepT <= 0 && P.speed > 0.5f) {
        P.stepT = P.sprint ? 1.9f : 1.6f;
        bool metal = G.mission == 1 || G.mission == 5;
        float v = P.stance == 2 ? 0.1f : (P.sprint ? 0.32f : 0.2f);
        playSnd(ST_STEP, v, frange(-0.2f, 0.2f), 0.05f, 0.18f, 0, 0.35f, metal ? 520.0f : 0.0f);
    }
    // ---- оружие
    W.fireCD -= dt; if (W.fireCD < -0.2f) W.fireCD = -0.2f;
    W.boltT -= dt;
    P.muzzle -= dt;
    P.throwAnim = std::max(0.0f, P.throwAnim - dt);
    if (P.switchT > 0) {
        float prev = P.switchT; P.switchT -= dt;
        if (prev > 0.3f && P.switchT <= 0.3f && P.pendingSlot >= 0) { P.slot = P.pendingSlot; P.pendingSlot = -1; playSnd(ST_CLICK, 0.3f, 0, 0, 0.12f, 0.08f); }
    }
    WState& CW = P.w[P.slot];
    if (carrying && !dragging && P.slot != 1 && P.w[1].def >= 0 && P.switchT <= 0) { P.pendingSlot = 1; P.switchT = 0.6f; CW.reloadT = 0; }
    if (IN.sel > 0 && IN.sel <= 2 && IN.sel - 1 != P.slot) IN.sw = true;
    if (IN.sw && P.switchT <= 0 && P.w[1 - P.slot].def >= 0 && !carrying) { P.pendingSlot = 1 - P.slot; P.switchT = 0.6f; CW.reloadT = 0; }
    if (CW.def >= 0 && P.switchT <= 0) {
        const WeaponDef& CD = WD[CW.def];
        if (CW.reloadT > 0) {
            CW.reloadT -= dt;
            if (CD.shellReload && IN.fire && CW.mag > 0) CW.reloadT = 0;
            else if (CW.reloadT <= 0) finishReload(CW);
        }
        if (IN.reload && !P.sprint && P.cookType < 0) startReload(CW);
        bool canFire = CW.reloadT <= 0 && !P.sprint && CW.boltT <= 0 && P.cookType < 0 && P.throwAnim <= 0 && !dragging;
        if (!IN.fire) CW.semiLock = false;
        if (IN.fire && canFire) {
            if (CW.mag <= 0) {
                if (!CW.semiLock) { playSnd(ST_CLICK, 0.4f, 0, 0, 0.05f, 1); CW.semiLock = true; }
                if (CW.reserve > 0) startReload(CW);
            } else if (!CW.semiLock) {
                int guard = 0;
                while (CW.fireCD <= 0 && CW.mag > 0 && guard++ < 3) {
                    playerFire(CW);
                    CW.fireCD += 60.0f / CD.rpm;
                    if (!CD.autoFire) { CW.semiLock = true; break; }
                }
            }
        }
        if (CW.fireCD < 0 && !IN.fire) CW.fireCD = 0;
    }
    // ---- гранаты
    if (IN.tacCycle) { P.tac = P.tac == 1 ? 2 : 1; playSnd(ST_CLICK, 0.25f, 0, 0, 0.05f, 1); }
    if (P.nade[P.tac] <= 0 && P.nade[3 - P.tac] > 0 && P.cookType < 0) P.tac = 3 - P.tac;
    bool canThrow = P.switchT <= 0 && !dragging && P.throwAnim <= 0 && !P.sprint;
    if (P.cookType < 0 && canThrow) {
        if (IN.nadeHeld && P.nade[0] > 0) { P.cookType = 0; P.cookT = 0; CW.reloadT = 0; playSnd(ST_CLICK, 0.4f, 0, 0, 0.1f, 0.06f); }
        else if (IN.tacHeld && P.nade[P.tac] > 0) { P.cookType = P.tac; P.cookT = 0; CW.reloadT = 0; playSnd(ST_CLICK, 0.4f, 0, 0, 0.1f, 0.06f); }
    }
    if (P.cookType >= 0) {
        P.cookT += dt;
        bool held = P.cookType == 0 ? IN.nadeHeld : IN.tacHeld;
        float fuse = P.cookType == 0 ? 4.0f : (P.cookType == 1 ? 1.8f : 1.4f);
        if (!held || (P.cookType == 0 && P.cookT >= fuse)) {
            V3 from = playerEye() + F * 0.3f + R * 0.15f;
            V3 v = dirYP(P.yaw, P.pitch + 9 * DEG) * 17.0f + P.vel * 0.5f;
            throwGrenade(from, v, P.cookType, std::max(0.05f, fuse - P.cookT), 0, true);
            P.nade[P.cookType]--; P.cookType = -1; P.throwAnim = 0.45f;
        }
    }
    // ---- здоровье
    P.sinceHit += dt;
    if (P.sinceHit > 4.0f) P.hp = std::min(100.0f, P.hp + 32 * dt);
    g_amb.breath = P.hp < 60 ? (60 - P.hp) / 60.0f : 0.0f;
    if (P.sprint && P.stamina < 0.3f) g_amb.breath = std::max(g_amb.breath, 0.5f);
    g_amb.heart = P.hp < 45 ? (45 - P.hp) / 45.0f : 0.0f;
    P.flash = std::max(0.0f, P.flash - dt);
    P.shake = std::max(0.0f, P.shake - dt * 1.5f);
    P.suppress = std::max(0.0f, P.suppress - dt * 0.7f);
    P.hitMarker = std::max(0.0f, P.hitMarker - dt);
    g_amb.ring = std::max(0.0f, g_amb.ring - dt * 0.2f);
    // ---- взаимодействие
    if (!G.useHint.empty() && IN.useHeld) P.useT += dt; else P.useT = 0;
    // ---- вода (миссия 2)
    if (G.waterOn && playerEye().y < G.waterY && P.pos.x > G.waterMn.x && P.pos.x < G.waterMx.x && P.pos.z > G.waterMn.z && P.pos.z < G.waterMx.z) {
        P.hp -= 22 * dt; P.sinceHit = 0;
        if (P.hp <= 0) { P.hp = 0; P.dead = true; }
    }
    if (G.sea && (P.pos.y < -6.0f || fabsf(P.pos.x) > 8.5f)) { P.hp = 0; P.dead = true; G.failMsg = "За бортом."; }
}
// =====================================================================
//  ВСТРОЕННЫЕ МОДЕЛИ (сгенерировано из присланных файлов: LowPolyAK12.fbx,
//  MK2Grenade (MK2.obj + PBR_MK2_Base_Color.png), low_poly_soldier/soldier.blend)
// =====================================================================
static const float kAkScale = 1.0f / 40000.0f;
static const int16_t kAkV[4968] = {
    681,-1565,-5669,681,-1296,-5669,681,-1565,-6799,681,-1296,-6799,681,-1430,-5563,681,-1430,-6904,-681,-1565,-5669,-681,-1296,-5669,
    -681,-1565,-6799,-681,-1296,-6799,-681,-1430,-5563,-681,-1430,-6904,693,-842,-10458,693,-842,-11021,693,-673,-10740,-693,-842,-10458,
    -693,-842,-11021,-693,-673,-10740,-776,-1648,-3498,-776,-388,-3498,-776,-2132,-13318,-776,-388,-13318,776,-1648,-3498,776,-388,-3498,
    776,-2132,-13318,776,-388,-13318,-776,-842,-11021,-776,-2047,-11602,-693,-2047,-11602,-776,-842,-10458,-776,-1964,-9918,-693,-1964,-9918,
    -776,-673,-10740,693,-2047,-11602,693,-1964,-9918,776,-842,-11021,776,-673,-10740,776,-842,-10458,776,-2047,-11602,776,-1964,-9918,
    -477,-2081,-7873,-317,-1844,-7527,-317,-3214,-8382,-317,-1844,-8382,-477,-2713,-7873,317,-1844,-7527,317,-3214,-8382,317,-1844,-8382,
    -317,-3214,-7778,-317,-2963,-7527,317,-2963,-7527,317,-3214,-7778,-477,-2713,-8238,-477,-2081,-8238,477,-2713,-7873,477,-2081,-7873,
    477,-2713,-8238,477,-2081,-8238,-221,-2865,-7788,-221,-3915,-7963,-221,-3891,-8106,-221,-2841,-7931,221,-3915,-7963,221,-2865,-7788,
    221,-3891,-8106,221,-2841,-7931,-238,-3357,-8364,-238,-3433,-8330,-238,-3433,-9650,-238,-3357,-9633,238,-3433,-8330,238,-3357,-8364,
    238,-3433,-9650,238,-3357,-9633,-238,-3151,-10248,-238,-3097,-10176,238,-3151,-10248,238,-3097,-10176,-238,-1948,-10248,-238,-1948,-10176,
    238,-1948,-10248,238,-1948,-10176,-238,-3132,-8221,-238,-3194,-8175,238,-3194,-8175,238,-3132,-8221,-353,-2848,-7953,353,-2848,-7953,
    -353,-2936,-7890,353,-2936,-7890,-353,-2902,-7787,353,-2902,-7787,-353,-2794,-7787,353,-2794,-7787,-353,-2761,-7890,353,-2761,-7890,
    -645,-4499,-11006,645,-4499,-11006,-645,-4922,-11453,645,-4922,-11453,-645,-5468,-11276,645,-5468,-11276,-645,-5891,-11705,645,-5891,-11705,
    -645,-6590,-11533,645,-6590,-11533,-645,-6241,-13048,645,-6241,-13048,-645,-5566,-12858,645,-5566,-12858,-645,-4726,-12582,645,-4726,-12582,
    277,-2256,-12999,-277,-1980,-12999,-277,-2256,-12999,277,-1980,-12999,-277,-6155,-13411,277,-6155,-13411,-277,-5455,-13281,277,-5455,-13281,
    -277,-4615,-13005,277,-4615,-13005,-277,-3502,-12284,277,-3502,-12284,-277,-2964,-12234,277,-2964,-12234,-277,-2656,-12335,277,-2656,-12335,
    -277,-3272,-10261,-277,-1980,-10261,277,-3272,-10261,277,-1980,-10261,-277,-3567,-10666,277,-3567,-10666,-277,-4032,-10951,277,-4032,-10951,
    -277,-4538,-10850,277,-4538,-10850,-277,-4961,-11248,277,-4961,-11248,-277,-5486,-11141,277,-5486,-11141,-277,-5941,-11492,277,-5941,-11492,
    -277,-6615,-11399,277,-6615,-11399,-487,-1980,-12843,-487,-2256,-12843,-487,-2612,-12234,-487,-1980,-10480,-487,-3272,-10480,-487,-2930,-12107,
    -487,-3490,-12142,-487,-3505,-10808,-645,-3968,-12250,-498,-3537,-10867,-631,-3926,-11145,487,-3272,-10480,487,-1980,-10480,487,-2930,-12107,
    487,-3490,-12142,487,-3505,-10808,487,-1980,-12843,487,-2256,-12843,487,-2612,-12234,498,-3537,-10867,631,-3926,-11145,645,-3968,-12250,
    -825,-392,-13317,-825,-189,-13317,825,-392,-13317,825,-189,-13317,-355,853,-1436,-355,853,-12801,355,853,-12801,355,853,-1436,
    -421,697,-6670,421,697,-6670,-421,697,-1436,421,697,-1436,-825,-189,-6935,-825,-392,-6996,825,-392,-6996,825,-189,-6935,
    -776,-1618,-5567,-776,-1618,-6901,-776,-1430,-5432,-776,-1242,-5567,-776,-1430,-7036,-776,-1242,-6901,776,-1242,-6901,776,-1430,-7036,
    776,-1430,-5432,776,-1242,-5567,776,-1618,-6901,776,-1618,-5567,-153,-2292,-9552,-153,-1884,-9347,-153,-2267,-9703,-153,-1891,-10013,
    153,-2292,-9552,153,-1884,-9347,153,-2267,-9703,153,-1891,-10013,-153,-2662,-9560,-153,-2715,-9657,153,-2662,-9560,153,-2715,-9657,
    -153,-3057,-9344,-153,-3110,-9442,153,-3057,-9344,153,-3110,-9442,695,-1217,-10981,839,-1217,-10981,695,-1377,-10752,839,-1377,-10752,
    695,-1258,-10500,839,-1258,-10500,695,-980,-10477,839,-980,-10477,695,-821,-10706,839,-821,-10706,695,-939,-10958,839,-939,-10958,
    695,-719,-10908,839,-719,-10908,839,-1757,-10573,695,-1757,-10573,695,-1638,-10321,839,-1638,-10321,839,-2261,-10230,695,-2261,-10230,
    695,-2224,-10151,839,-2224,-10151,592,-1104,-10886,921,-1104,-10886,592,-1246,-10804,921,-1246,-10804,592,-1246,-10640,921,-1246,-10640,
    592,-1104,-10557,921,-1104,-10557,592,-962,-10640,921,-962,-10640,592,-962,-10804,921,-962,-10804,-695,-1217,-10981,-839,-1217,-10981,
    -695,-1377,-10752,-839,-1377,-10752,-695,-1258,-10500,-839,-1258,-10500,-695,-980,-10477,-839,-980,-10477,-695,-821,-10706,-839,-821,-10706,
    -695,-939,-10958,-839,-939,-10958,-695,-719,-10908,-839,-719,-10908,-839,-1757,-10573,-695,-1757,-10573,-695,-1638,-10321,-839,-1638,-10321,
    -839,-2261,-10230,-695,-2261,-10230,-695,-2224,-10151,-839,-2224,-10151,-592,-1104,-10886,-921,-1104,-10886,-592,-1246,-10804,-921,-1246,-10804,
    -592,-1246,-10640,-921,-1246,-10640,-592,-1104,-10557,-921,-1104,-10557,-592,-962,-10640,-921,-962,-10640,-592,-962,-10804,-921,-962,-10804,
    -669,-2108,-13298,-669,-2108,-18941,669,-2108,-13298,669,-2108,-18941,-404,-315,-13298,-669,-580,-13298,-591,-392,-13298,-669,-580,-19630,
    -404,-315,-19630,-591,-392,-19630,669,-580,-13298,404,-315,-13298,591,-392,-13298,404,-315,-19630,669,-580,-19630,591,-392,-19630,
    -669,-2313,-19227,669,-2313,-19227,-669,-2737,-19385,669,-2737,-19385,-669,-4148,-19315,669,-4148,-19315,-669,-4148,-19630,669,-4148,-19630,
    297,-4690,-19315,-297,-4690,-19315,297,-4690,-19630,-297,-4690,-19630,-186,-2409,-15477,-186,-2054,-14812,-186,-2409,-15868,-186,-2054,-16001,
    186,-2409,-15477,186,-2054,-14812,186,-2409,-15868,186,-2054,-16001,-253,-2055,-16655,-253,-4462,-18721,-253,-2291,-17499,253,-4462,-18721,
    253,-2055,-16655,253,-2659,-17874,253,-2291,-17499,-253,-2659,-17874,-186,-2216,-15275,-253,-4636,-19474,253,-4636,-19474,186,-2216,-15275,
    -253,-2649,-19548,253,-2649,-19548,-253,-1729,-19209,253,-1729,-19209,-253,-1604,-16663,253,-1604,-16663,-253,-2711,-18325,253,-2711,-18325,
    -253,-2650,-18447,253,-2650,-18447,-253,-2856,-18317,253,-2856,-18317,253,-4010,-18970,-253,-4010,-18970,-253,-2732,-18568,253,-2732,-18568,
    -253,-3929,-18850,253,-3929,-18850,253,-3950,-19093,-253,-3950,-19093,253,-3804,-19100,-253,-3804,-19100,0,-2609,-16700,0,-2044,-16743,
    328,-2609,-16462,369,-2044,-16475,203,-2609,-16076,228,-2044,-16042,-203,-2609,-16076,-228,-2044,-16042,-328,-2609,-16462,-369,-2044,-16475,
    -90,-2591,-16569,-90,-2188,-16569,-90,-2296,-17046,90,-2591,-16569,90,-2188,-16569,90,-2296,-17046,-90,-2525,-17352,-90,-2591,-17330,
    90,-2591,-17330,90,-2525,-17352,-785,-1246,-13851,-785,-979,-14117,-785,-1512,-14117,-785,-1246,-14384,785,-1246,-13851,785,-979,-14117,
    785,-1512,-14117,785,-1246,-14384,733,-300,-13668,879,-300,-13562,733,-2234,-13761,733,-300,-13761,968,-2234,-13591,968,-300,-13591,
    878,-2234,-13315,878,-300,-13315,588,-2234,-13315,588,-300,-13315,498,-2234,-13591,498,-300,-13591,823,-300,-13390,642,-300,-13390,
    587,-300,-13562,733,-180,-13668,587,-180,-13562,642,-180,-13390,823,-180,-13390,879,-180,-13562,-600,97,-14734,-600,97,-18914,
    600,97,-18914,600,97,-14734,-460,310,-14734,-460,310,-18914,460,310,-18914,460,310,-14734,-217,423,-14734,-217,423,-18914,
    217,423,-18914,217,423,-14734,405,183,-18914,405,183,-14734,529,-30,-18914,529,-30,-14734,-191,296,-18914,-191,296,-14734,
    -405,183,-18914,-405,183,-14734,191,296,-18914,191,296,-14734,-529,-30,-18914,-529,-30,-14734,778,-1408,-18375,883,-1408,-18375,
    805,-717,-18914,700,-717,-18914,883,-1029,-18670,778,-1029,-18670,-700,-717,-18914,-778,-1029,-18670,-778,-1408,-18375,-883,-1029,-18670,
    -883,-1408,-18375,-805,-717,-18914,778,-1408,-14977,883,-1408,-14977,715,-778,-14734,883,-1029,-14831,778,-1029,-14831,820,-778,-14734,
    -715,-778,-14734,-778,-1029,-14831,-778,-1408,-14977,-883,-1029,-14831,-883,-1408,-14977,-820,-778,-14734,1017,-950,-18308,-1017,-950,-18308,
    -1017,-1118,-15576,1017,-1118,-15576,-1017,-1365,-15397,1017,-1365,-15397,-1017,-1270,-15106,1017,-1270,-15106,-1017,-965,-15106,1017,-965,-15106,
    -1017,-871,-15397,1017,-871,-15397,-1017,-1255,-18331,1017,-1255,-18331,-1017,-1370,-18048,1017,-1370,-18048,-1017,-1137,-17851,1017,-1137,-17851,
    -1017,-877,-18012,1017,-877,-18012,-409,-4024,-19628,-630,-681,-19628,-409,-4024,-20258,-630,-681,-20442,409,-4024,-19628,630,-681,-19628,
    409,-4024,-20258,630,-681,-20442,-630,-1795,-19628,-630,-1795,-20126,-517,-2910,-20126,630,-1795,-20126,517,-2910,-20126,630,-1795,-19628,
    -205,-4472,-20258,-205,-4472,-19628,-243,-122,-19628,-243,-122,-20442,205,-4472,-20258,243,-122,-20442,205,-4472,-19628,243,-122,-19628,
    427,250,3134,427,480,3134,427,250,2248,427,480,2248,427,250,1595,427,480,1595,427,250,710,427,480,710,
    -427,250,3134,-427,480,3134,-427,250,2248,-427,480,2248,-427,250,1595,-427,480,1595,-427,250,710,-427,480,710,
    -559,-1390,4005,-559,478,4005,-559,-1390,2394,559,-1390,4005,559,478,4005,559,-1390,2394,-498,723,4005,498,723,4005,
    -334,865,4005,334,865,4005,-559,478,-215,559,478,-215,-498,723,-215,498,723,-215,-334,865,-215,334,865,-215,
    -559,-223,2394,559,-223,2394,-559,-223,-215,559,-223,-215,-940,-1049,2394,940,-1049,2394,-834,-223,2394,-940,-319,2394,
    940,-319,2394,834,-223,2394,-834,-223,-445,834,-223,-445,-559,325,-215,559,325,-215,559,-1390,-2525,-559,-1390,-2525,
    -559,325,-3504,559,325,-3504,559,-1390,-2750,-559,-1390,-2750,740,-1723,-3121,-740,-1723,-3121,-740,-1723,-3504,740,-1723,-3504,
    558,480,3134,559,250,2248,558,480,1595,559,478,1595,559,250,3134,559,478,3134,559,250,1595,558,480,2248,
    559,478,2248,-558,480,3134,-558,480,2248,-559,250,2248,-559,250,3134,-559,478,3134,-559,250,710,-559,250,1595,
    -558,480,1595,-559,478,1595,-559,478,2248,558,480,710,559,478,710,559,250,710,-558,480,710,-559,478,710,
    940,-1049,-1540,940,-319,-1540,834,-223,-1863,781,-118,-2023,781,-1191,-2023,-834,-223,-1863,-940,-319,-1540,-781,-118,-2023,
    -781,-1191,-2023,-940,-1049,-1540,-781,-1656,-3504,-781,-1656,-3089,-781,-1274,-2750,-781,-1392,-2875,-781,-1191,-2525,-781,-118,-3504,
    781,-118,-3504,781,-1656,-3504,781,-1656,-3089,781,-1392,-2875,781,-1274,-2750,781,-1191,-2525,-901,-878,-2900,-901,-435,-2900,
    -901,-878,-3480,-901,-435,-3480,901,-878,-2900,901,-435,-2900,901,-878,-3480,901,-435,-3480,-855,-632,-1799,-855,-632,2394,
    -855,-520,-1799,-940,-520,-1540,-855,-520,2394,855,-744,-1799,940,-744,-1540,855,-744,2394,940,-744,2394,940,-969,2394,
    855,-969,-1799,855,-969,2394,855,-632,-1799,940,-632,-1540,940,-632,2394,855,-632,2394,-855,-857,-1799,-855,-857,2394,
    -940,-857,-1540,-940,-857,2394,-855,-969,-1799,-855,-969,2394,-940,-632,-1540,-940,-632,2394,940,-520,2394,855,-520,-1799,
    940,-520,-1540,855,-520,2394,855,-407,-1799,855,-407,2394,940,-407,-1540,940,-407,2394,-940,-520,2394,-855,-744,-1799,
    -855,-744,2394,-855,-407,-1799,-940,-407,-1540,-940,-407,2394,-855,-407,2394,855,-857,-1799,855,-857,2394,940,-969,-1540,
    -940,-969,2394,-940,-744,-1540,-940,-969,-1540,-940,-744,2394,940,-857,-1540,940,-857,2394,501,-458,3906,714,-458,3906,
    501,-968,3906,714,-968,3906,826,-339,3906,826,-1087,3906,922,-504,3906,922,-921,3906,501,-458,2169,714,-458,2169,
    501,-968,2169,714,-968,2169,826,-339,3471,826,-339,3037,826,-339,2603,826,-1087,2603,826,-1087,3037,826,-1087,3471,
    922,-504,3471,922,-504,3037,922,-504,2603,922,-921,2603,922,-921,3037,922,-921,3471,780,-389,3471,780,-389,3037,
    780,-389,2603,780,-389,2169,780,-1037,2169,780,-1037,2603,780,-1037,3037,780,-1037,3471,826,-609,3906,826,-609,3471,
    826,-817,3471,826,-817,3906,826,-609,2603,826,-817,2603,826,-609,3037,826,-817,3037,-501,-458,3906,-714,-458,3906,
    -501,-968,3906,-714,-968,3906,-826,-339,3906,-826,-1087,3906,-922,-504,3906,-922,-921,3906,-501,-458,2169,-714,-458,2169,
    -501,-968,2169,-714,-968,2169,-826,-339,3471,-826,-339,3037,-826,-339,2603,-826,-1087,2603,-826,-1087,3037,-826,-1087,3471,
    -922,-504,3471,-922,-504,3037,-922,-504,2603,-922,-921,2603,-922,-921,3037,-922,-921,3471,-780,-389,3471,-780,-389,3037,
    -780,-389,2603,-780,-389,2169,-780,-1037,2169,-780,-1037,2603,-780,-1037,3037,-780,-1037,3471,-826,-609,3906,-826,-609,3471,
    -826,-817,3471,-826,-817,3906,-826,-609,2603,-826,-817,2603,-826,-609,3037,-826,-817,3037,-374,1165,-7506,374,1165,-7506,
    -208,1260,-7506,208,1260,-7506,-324,1118,-7506,324,1118,-7506,-255,840,-1316,-255,1053,-1316,255,840,-1316,255,1053,-1316,
    -374,1165,-1862,-374,1165,-2296,-374,1165,-2730,-374,1165,-3165,-374,1165,-3599,-374,1165,-4033,-374,1165,-4467,-374,1165,-4901,
    -374,1165,-5335,-374,1165,-5769,-374,1165,-6203,-374,1165,-6637,-374,1165,-7072,374,1165,-7072,374,1165,-6637,374,1165,-6203,
    374,1165,-5769,374,1165,-5335,374,1165,-4901,374,1165,-4467,374,1165,-4033,374,1165,-3599,374,1165,-3165,374,1165,-2730,
    374,1165,-2296,374,1165,-1862,-208,1260,-1862,-208,1260,-2296,-208,1260,-2730,-208,1260,-3165,-208,1260,-3599,-208,1260,-4033,
    -208,1260,-4467,-208,1260,-4901,-208,1260,-5335,-208,1260,-5769,-208,1260,-6203,-208,1260,-6637,-208,1260,-7072,208,1260,-7072,
    208,1260,-6637,208,1260,-6203,208,1260,-5769,208,1260,-5335,208,1260,-4901,208,1260,-4467,208,1260,-4033,208,1260,-3599,
    208,1260,-3165,208,1260,-2730,208,1260,-2296,208,1260,-1862,-324,1118,-1316,-324,1118,-1862,-324,1118,-2296,-324,1118,-2730,
    -324,1118,-3165,-324,1118,-3599,-324,1118,-4033,-324,1118,-4467,-324,1118,-4901,-324,1118,-5335,-324,1118,-5769,-324,1118,-6203,
    -324,1118,-6637,-324,1118,-7072,324,1118,-7072,324,1118,-6637,324,1118,-6203,324,1118,-5769,324,1118,-5335,324,1118,-4901,
    324,1118,-4467,324,1118,-4033,324,1118,-3599,324,1118,-3165,324,1118,-2730,324,1118,-2296,324,1118,-1862,324,1118,-1316,
    -104,1165,-7506,-104,1165,-7072,104,1165,-7506,104,1165,-7072,-104,1165,-6637,104,1165,-6637,-104,1165,-6203,104,1165,-6203,
    -104,1165,-5769,104,1165,-5769,-104,1165,-5335,104,1165,-5335,-104,1165,-4901,104,1165,-4901,-104,1165,-4467,104,1165,-4467,
    -104,1165,-4033,104,1165,-4033,-104,1165,-3599,104,1165,-3599,-104,1165,-3165,104,1165,-3165,-104,1165,-2730,104,1165,-2730,
    -104,1165,-2296,104,1165,-2296,-104,1165,-1862,104,1165,-1862,-255,762,-474,255,724,6348,-255,762,3867,-255,975,3867,
    -255,975,-474,255,762,3867,208,1145,7650,-255,724,6348,255,762,-474,255,975,-474,-255,937,6348,255,975,3867,
    -374,1087,3867,-374,1087,3433,-374,1087,2999,-374,1087,2565,-374,1087,2131,-374,1087,1697,-374,1087,1263,-374,1087,829,
    -374,1087,394,-374,1087,-40,374,1087,-40,374,1087,394,374,1087,829,374,1087,1263,374,1087,1697,374,1087,2131,
    374,1087,2565,374,1087,2999,374,1087,3433,374,1087,3867,-208,1183,3867,-208,1183,3433,-208,1183,2999,-208,1183,2565,
    -208,1183,2131,-208,1183,1697,-208,1183,1263,-208,1183,829,-208,1183,394,-208,1183,-40,208,1183,-40,208,1183,394,
    208,1183,829,208,1183,1263,208,1183,1697,208,1183,2131,208,1183,2565,208,1183,2999,208,1183,3433,208,1183,3867,
    -255,724,7650,-324,1041,3433,-324,1041,2999,-324,1041,2565,-324,1041,2131,-324,1041,1697,-324,1041,1263,-324,1041,829,
    -324,1041,394,-324,1041,-40,-324,1041,-474,324,1041,-474,324,1041,-40,324,1041,394,324,1041,829,324,1041,1263,
    324,1041,1697,324,1041,2131,324,1041,2565,324,1041,2999,324,1041,3433,-255,937,7650,255,937,6348,-374,1049,6782,
    255,724,7650,-374,1049,7216,-374,1049,6348,255,937,7650,-104,1087,-40,104,1087,-40,-104,1087,394,104,1087,394,
    -104,1087,829,-374,1049,7650,104,1087,829,-104,1087,1263,-208,1145,7650,104,1087,1263,-104,1087,1697,374,1049,7650,
    104,1087,1697,-104,1087,2131,104,1087,2131,-104,1087,2565,104,1087,2565,-104,1087,2999,104,1087,2999,-104,1087,3433,
    104,1087,3433,-104,1087,3867,104,1087,3867,374,1049,6348,374,1049,6782,374,1049,7216,-208,1145,7216,-208,1145,6782,
    -208,1145,6348,208,1145,6348,208,1145,6782,208,1145,7216,-324,1002,7216,-324,1002,6782,324,1002,6782,324,1002,7216,
    -104,1049,7650,-104,1049,7216,104,1049,7216,104,1049,7650,-104,1049,6348,104,1049,6348,-104,1049,6782,104,1049,6782,
    -255,840,-12759,-255,1053,-12759,255,840,-12759,255,1053,-12759,-374,1165,-7940,-374,1165,-8374,-374,1165,-8808,-374,1165,-9242,
    -374,1165,-9676,-374,1165,-10110,-374,1165,-10545,-374,1165,-10979,-374,1165,-11413,-374,1165,-12618,374,1165,-12618,374,1165,-11413,
    374,1165,-10979,374,1165,-10545,374,1165,-10110,374,1165,-9676,374,1165,-9242,374,1165,-8808,374,1165,-8374,374,1165,-7940,
    -208,1260,-7940,-208,1260,-8374,-208,1260,-8808,-208,1260,-9242,-208,1260,-9676,-208,1260,-10110,-208,1260,-10545,-208,1260,-10979,
    -208,1260,-11413,-208,1260,-12618,208,1260,-12618,208,1260,-11413,208,1260,-10979,208,1260,-10545,208,1260,-10110,208,1260,-9676,
    208,1260,-9242,208,1260,-8808,208,1260,-8374,208,1260,-7940,-324,1118,-7940,-324,1118,-8374,-324,1118,-8808,-324,1118,-9242,
    -324,1118,-9676,-324,1118,-10110,-324,1118,-10545,-324,1118,-10979,-324,1118,-11413,324,1118,-11413,324,1118,-10979,324,1118,-10545,
    324,1118,-10110,324,1118,-9676,324,1118,-9242,324,1118,-8808,324,1118,-8374,324,1118,-7940,-104,1165,-12618,104,1165,-12618,
    -104,1165,-11413,104,1165,-11413,-104,1165,-10979,104,1165,-10979,-104,1165,-10545,104,1165,-10545,-104,1165,-10110,104,1165,-10110,
    -104,1165,-9676,104,1165,-9676,-104,1165,-9242,104,1165,-9242,-104,1165,-8808,104,1165,-8808,-104,1165,-8374,104,1165,-8374,
    -104,1165,-7940,104,1165,-7940,-720,-415,-725,-352,729,-725,-720,-415,-7109,-352,729,-7109,720,-415,-725,352,729,-725,
    720,-415,-7109,352,729,-7109,325,374,-771,325,572,-771,325,374,-1062,325,572,-1062,684,315,-900,684,631,-900,
    684,315,-1051,684,631,-1051,652,374,-1051,652,572,-1051,652,374,-893,652,572,-893,943,315,-917,943,631,-917,
    1258,315,-836,1258,631,-836,1258,315,-905,1258,631,-905,-503,-44,-219,-503,-44,-736,503,-44,-219,503,-44,-736,
    -503,1204,-219,503,1204,-219,-503,718,-736,503,718,-736,-503,881,-1714,503,881,-1714,-503,1204,-1714,503,1204,-1714,
    -503,718,-1502,503,718,-1502,355,1344,-1714,-355,1344,-219,355,1344,-219,-355,1344,-1714,0,70,3983,359,278,3983,
    359,693,3983,0,900,3983,-359,693,3983,-359,278,3983,0,70,7829,359,278,7829,359,693,7829,0,900,7829,
    -359,693,7829,-359,278,7829,0,70,7000,270,226,7829,270,226,7000,-270,226,7829,-270,226,7000,0,4,7962,
    0,-58,7034,178,4,7773,178,-25,7229,-178,4,7773,-178,-25,7229,0,-255,8398,0,-255,6843,282,-255,8210,
    282,-255,7038,-282,-255,8210,-282,-255,7038,255,-1320,3101,255,-1532,3101,-255,-1320,3101,-255,-1532,3101,374,-1645,3101,
    -374,-1645,3101,208,-1740,3101,-208,-1740,3101,255,-1320,-3153,255,-1532,-3153,-255,-1320,-3153,-255,-1532,-3153,374,-1645,2667,
    374,-1645,2233,374,-1645,1799,374,-1645,1365,374,-1645,931,374,-1645,497,374,-1645,63,374,-1645,-372,374,-1645,-806,
    374,-1645,-1240,374,-1645,-1674,374,-1645,-2108,374,-1645,-2542,-374,-1645,-2542,-374,-1645,-2108,-374,-1645,-1674,-374,-1645,-1240,
    -374,-1645,-806,-374,-1645,-372,-374,-1645,63,-374,-1645,497,-374,-1645,931,-374,-1645,1365,-374,-1645,1799,-374,-1645,2233,
    -374,-1645,2667,208,-1740,2667,208,-1740,2233,208,-1740,1799,208,-1740,1365,208,-1740,931,208,-1740,497,208,-1740,63,
    208,-1740,-372,208,-1740,-806,208,-1740,-1240,208,-1740,-1674,208,-1740,-2108,208,-1740,-2542,-208,-1740,-2542,-208,-1740,-2108,
    -208,-1740,-1674,-208,-1740,-1240,-208,-1740,-806,-208,-1740,-372,-208,-1740,63,-208,-1740,497,-208,-1740,931,-208,-1740,1365,
    -208,-1740,1799,-208,-1740,2233,-208,-1740,2667,324,-1598,2667,324,-1598,2233,324,-1598,1799,324,-1598,1365,324,-1598,931,
    324,-1598,497,324,-1598,63,324,-1598,-372,324,-1598,-806,324,-1598,-1240,324,-1598,-1674,324,-1598,-2108,324,-1598,-2542,
    324,-1598,-3153,-324,-1598,-3153,-324,-1598,-2542,-324,-1598,-2108,-324,-1598,-1674,-324,-1598,-1240,-324,-1598,-806,-324,-1598,-372,
    -324,-1598,63,-324,-1598,497,-324,-1598,931,-324,-1598,1365,-324,-1598,1799,-324,-1598,2233,-324,-1598,2667,-332,-339,11854,
    -248,-12,11854,-332,-339,10932,-248,-12,10932,332,-339,11854,248,-12,11854,332,-339,10932,248,-12,10932,-248,1466,11854,
    -248,1466,11263,248,1466,11263,248,1466,11854,-199,1581,11854,-199,1581,11263,199,1581,11263,199,1581,11854,0,1521,11263,
    0,1521,11854,-334,1690,11854,-334,1690,11263,334,1690,11263,334,1690,11854,-300,1708,11854,-300,1708,11263,300,1708,11263,
    300,1708,11854,-378,1865,11854,-378,1865,11263,378,1865,11263,378,1865,11854,-338,1865,11854,-338,1865,11263,338,1865,11263,
    338,1865,11854,-336,2037,11774,-336,2037,11403,336,2037,11403,336,2037,11774,-300,2020,11774,-300,2020,11403,300,2020,11403,
    300,2020,11774,-52,1502,11637,-19,1869,11588,-52,1502,11486,-19,1869,11535,52,1502,11637,19,1869,11588,52,1502,11486,
    19,1869,11535,-248,16,11732,248,16,11732,-248,16,11048,248,16,11048,-248,481,11732,248,481,11732,-248,481,11140,
    248,481,11140,-318,1329,-10272,318,1329,-10272,-180,1329,-10272,180,1329,-10272,-497,1270,-9569,-497,1270,-10312,497,1270,-9569,
    497,1270,-10312,-497,1580,-10654,497,1580,-10654,-497,1580,-10800,497,1580,-10800,-497,1270,-11126,497,1270,-11126,-497,1270,-12449,
    497,1270,-12449,-445,1270,-10312,445,1270,-10312,445,1270,-9569,-445,1270,-9569,-445,1580,-10654,445,1580,-10654,-445,1580,-10800,
    445,1580,-10800,-445,1270,-11126,445,1270,-11126,-445,1270,-12449,445,1270,-12449,-445,1550,-10483,445,1550,-10483,-445,1754,-10243,
    445,1754,-10243,445,1398,-9569,-445,1398,-9569,445,1755,-9916,-445,1755,-9916,-445,1338,-11756,445,1338,-11756,-445,1424,-11901,
    445,1424,-11901,-445,1424,-12081,445,1424,-12081,-445,1718,-12246,445,1718,-12246,-445,1357,-12449,445,1357,-12449,-445,1718,-12363,
    445,1718,-12363,-497,1068,-9569,-415,985,-9569,415,985,-9569,497,1068,-9569,-415,985,-12449,-497,1068,-12449,497,1068,-12449,
    415,985,-12449,318,1729,-10272,180,1729,-10272,-180,1398,-9569,-180,1329,-9569,-180,1729,-10272,-180,1754,-10243,-180,1755,-9916,
    -318,1729,-10272,180,1329,-9569,180,1754,-10243,180,1398,-9569,180,1755,-9916,318,1329,-12449,-318,1329,-12449,318,1580,-10800,
    318,1580,-10654,318,1550,-10483,318,1357,-12449,318,1718,-12363,318,1424,-12081,318,1718,-12246,318,1424,-11901,318,1338,-11756,
    -318,1718,-12363,-318,1718,-12246,-318,1357,-12449,-318,1424,-12081,-318,1424,-11901,-318,1338,-11756,-318,1580,-10800,-318,1580,-10654,
    -318,1550,-10483,-287,1536,-9915,-287,1647,-9915,-287,1536,-12260,-287,1647,-12260,287,1536,-9915,287,1647,-9915,287,1536,-12260,
    287,1647,-12260,-311,1536,-12260,311,1536,-12260,-311,1536,-12395,311,1536,-12395,-120,1706,-12260,120,1706,-12260,-120,1706,-12395,
    120,1706,-12395,120,1842,-12260,120,1842,-12395,311,1842,-12395,311,1842,-12260,-311,1842,-12395,-311,1842,-12260,-120,1842,-12260,
    -120,1842,-12395,-298,1681,-11501,298,1681,-11501,-298,1540,-11420,298,1540,-11420,-298,1540,-11257,298,1540,-11257,-298,1681,-11175,
    298,1681,-11175,-298,1822,-11257,298,1822,-11257,-298,1822,-11420,298,1822,-11420,557,1761,-11384,-557,1761,-11384,557,1761,-11292,
    -557,1761,-11292,557,1681,-11246,-557,1681,-11246,557,1601,-11292,-557,1601,-11292,557,1681,-11430,557,1601,-11384,-557,1601,-11384,
    -557,1681,-11430,-528,1530,-10181,528,1530,-10181,-528,1428,-10122,528,1428,-10122,-528,1428,-10005,528,1428,-10005,-528,1530,-9946,
    528,1530,-9946,-528,1631,-10005,528,1631,-10005,-528,1631,-10122,528,1631,-10122,-545,-1809,-5394,-545,-1009,-5394,-545,-1809,-7261,
    -545,-1009,-7261,545,-1809,-5394,545,-1009,-5394,545,-1809,-7261,545,-1009,-7261,-545,-3588,-6982,-545,-3261,-5150,545,-3588,-6982,
    545,-3261,-5150,-545,-5585,-6402,-545,-5276,-4509,545,-5585,-6402,545,-5276,-4509,-545,-6954,-5797,-545,-6477,-3903,545,-6954,-5797,
    545,-6477,-3903,304,-1809,-7261,-304,-1809,-7261,-304,-1009,-7261,304,-1009,-7261,304,-3588,-6982,-304,-3588,-6982,304,-5585,-6402,
    -304,-5585,-6402,304,-6954,-5797,-304,-6954,-5797,304,-3588,-7165,-304,-3588,-7165,304,-5585,-6585,-304,-5585,-6585,304,-6954,-5980,
    -304,-6954,-5980,-388,-1809,-5263,-388,-1009,-5263,388,-1809,-5263,388,-1009,-5263,-388,-3261,-5019,388,-3261,-5019,-388,-5276,-4378,
    388,-5276,-4378,-388,-6477,-3772,388,-6477,-3772,388,-7811,-2916,-545,-7867,-3010,-388,-7811,-2916,-545,-8832,-4620,545,-8832,-4620,
    545,-7867,-3010,-304,-8902,-4737,-304,-8832,-4620,304,-8902,-4737,304,-8832,-4620,277,-5276,-3816,388,-5318,-3919,-388,-5318,-3919,
    -277,-5276,-3816,277,-3261,-4457,388,-3288,-4565,-388,-3288,-4565,-277,-3261,-4457,277,-1009,-4701,388,-1009,-4812,388,-1819,-4812,
    277,-1809,-4701,-388,-1009,-4812,-277,-1009,-4701,-388,-1819,-4812,-277,-1809,-4701,-388,-6532,-3307,-277,-6477,-3211,277,-6477,-3211,
    388,-6532,-3307,388,-7625,-2605,277,-7568,-2510,-277,-7568,-2510,-388,-7625,-2605,-304,-3117,-7239,304,-3117,-7239,304,-1809,-7650,
    -304,-1809,-7650,-304,-1009,-7650,304,-1009,-7650,-304,-2780,-7492,304,-2780,-7492,0,-942,4258,0,-942,3918,356,-737,4258,
    356,-737,3918,356,-326,4258,356,-326,3918,0,-120,4258,0,-120,3918,-356,-326,4258,-356,-326,3918,-356,-737,4258,
    -356,-737,3918,0,-838,4403,266,-685,4403,266,-378,4403,0,-224,4403,-266,-378,4403,-266,-685,4403,0,-838,10625,
    266,-685,10625,266,-378,10625,0,-224,10625,-266,-378,10625,-266,-685,10625,0,-921,10796,338,-726,10796,338,-336,10796,
    0,-141,10796,-338,-336,10796,-338,-726,10796,0,-921,12264,338,-726,12264,338,-336,12264,0,-141,12264,-338,-336,12264,
    -338,-726,12264,-266,-685,6728,-266,-685,8667,-266,-378,8667,-266,-378,6728,0,-224,6728,0,-224,8667,266,-378,8667,
    266,-378,6728,266,-685,6728,266,-685,8667,0,-838,8667,0,-838,6728,-391,-757,6843,-391,-757,8552,-391,-305,8552,
    -391,-305,6843,0,-79,6843,0,-79,8552,391,-305,8552,391,-305,6843,391,-757,6843,391,-757,8552,0,-983,8552,
    0,-983,6843,293,-814,8552,-293,-814,8552,0,-983,8269,0,-983,7358,293,-814,7358,293,-814,8269,-293,-814,7358,
    -293,-814,8269,0,-983,7033,293,-814,7033,-293,-814,7033,219,-1066,7033,219,-1066,7358,-219,-1066,7033,-219,-1066,7358,
    219,-1281,8552,219,-1281,8269,-219,-1281,8552,-219,-1281,8269,0,-1086,12264,480,-809,12264,480,-254,12264,0,24,12264,
    -480,-254,12264,-480,-809,12264,0,-659,15688,-111,-595,15688,-111,-467,15688,0,-403,15688,111,-467,15688,111,-595,15688,
    262,-380,16975,0,-229,16975,-262,-380,16975,262,-682,16975,0,-833,16975,-262,-682,16975,0,-961,16975,-372,-746,16975,
    -372,-316,16975,0,-101,16975,372,-316,16975,372,-746,16975,372,-316,16160,0,-101,16160,-372,-316,16160,372,-746,16160,
    0,-961,16160,-372,-746,16160,0,-1086,16160,-480,-809,16160,-480,-254,16160,0,24,16160,480,-254,16160,480,-809,16160,
    -480,-254,14928,480,-254,14928,369,-190,14928,-369,-190,14928,-480,-809,14928,480,-809,14928,-367,-874,14928,367,-874,14928,
    -367,-874,15607,367,-874,15607,-367,-874,15360,367,-874,15360,-480,-254,15360,480,-254,15360,369,-190,15360,-369,-190,15360,
    -480,-254,15607,480,-254,15607,369,-190,15607,-369,-190,15607,-480,-809,15360,480,-809,15360,-480,-809,15607,480,-809,15607,
    -480,-809,15175,480,-809,15175,-367,-874,15175,367,-874,15175,-480,-254,15175,480,-254,15175,369,-190,15175,-369,-190,15175,
};
static const uint16_t kAkT[9108] = {
    5,190,3,0,194,2,194,5,2,2,1,0,11,189,188,185,6,8,11,185,8,7,9,8,
    21,24,20,99,167,97,23,18,22,24,33,28,21,23,25,13,12,34,14,35,36,12,39,34,
    16,27,28,15,30,29,17,29,32,15,16,31,35,33,38,16,32,26,12,36,37,45,49,50,
    43,46,42,40,52,44,47,50,46,49,43,42,51,49,48,46,48,42,53,56,52,57,54,56,
    55,44,54,56,44,52,53,55,57,58,60,59,61,64,60,64,63,62,63,59,62,64,59,60,
    71,70,72,72,67,68,69,71,73,68,76,72,73,75,69,77,79,75,74,80,76,66,85,71,
    74,68,75,70,83,67,87,88,86,89,90,88,91,92,90,87,93,89,93,94,92,95,86,94,
    88,92,94,97,135,137,101,139,141,100,138,98,105,143,145,104,107,105,109,121,111,106,117,107,
    108,120,118,115,114,113,116,119,117,118,121,119,120,123,121,122,125,123,124,127,125,127,114,112,
    154,122,120,108,116,106,109,117,119,132,135,133,128,133,130,131,128,130,134,137,135,136,139,137,
    138,141,139,140,143,141,142,145,143,100,142,140,101,143,103,96,138,136,105,144,104,97,139,99,
    104,142,102,153,132,150,99,101,111,158,130,157,152,124,122,160,125,159,96,134,156,164,125,127,
    163,127,112,162,158,163,165,135,166,167,121,123,103,111,101,107,109,103,150,129,149,162,112,115,
    147,126,148,146,114,147,157,130,133,148,124,151,105,107,103,155,134,132,169,170,168,171,182,170,
    170,181,168,173,175,174,173,176,172,169,174,171,180,181,183,181,169,168,179,172,178,180,173,169,
    177,178,176,4,193,192,195,4,192,187,10,186,39,195,22,184,30,18,10,184,186,9,187,189,
    1,190,193,206,202,200,199,202,198,201,196,200,196,206,200,202,205,198,204,196,198,211,208,209,
    204,210,206,207,209,205,213,214,212,216,229,217,216,219,218,219,220,218,225,222,224,223,212,222,
    221,224,220,228,233,229,216,222,214,223,221,217,215,227,214,230,232,231,235,236,234,226,231,227,
    237,238,236,239,240,238,241,242,240,243,244,242,245,234,244,235,243,239,248,247,246,263,250,251,
    250,253,251,254,253,252,256,259,258,246,257,256,258,255,254,267,262,263,248,262,261,251,260,263,
    261,249,248,266,264,265,270,269,268,265,260,261,272,271,270,274,273,272,276,275,274,278,277,276,
    268,279,278,273,277,269,288,291,293,283,290,282,285,281,280,283,280,281,281,297,283,288,286,284,
    289,285,286,291,295,293,292,294,295,291,285,290,296,299,297,298,301,299,302,305,300,294,303,287,
    299,303,294,287,298,296,305,306,304,300,304,301,301,306,303,303,307,302,315,327,312,324,311,308,
    313,324,327,314,308,310,327,308,312,323,319,321,318,321,322,322,316,318,317,326,319,337,343,331,
    334,323,336,311,314,310,345,319,340,343,336,342,348,342,349,341,345,340,344,339,345,338,335,339,
    340,347,341,346,349,347,334,337,335,341,325,317,351,352,350,352,355,354,355,356,354,359,357,355,
    357,358,356,359,350,358,354,356,358,362,369,366,365,364,363,362,366,367,362,364,365,367,369,368,
    368,360,367,371,372,370,373,376,372,376,375,374,375,370,374,376,370,372,373,375,377,390,395,391,
    385,391,387,381,382,380,391,394,392,383,378,379,383,384,382,394,395,396,385,379,390,385,386,384,
    389,378,381,378,397,379,387,388,386,392,393,378,387,392,389,389,380,388,382,386,380,379,396,390,
    411,401,405,400,405,401,439,413,436,398,403,399,407,409,408,404,409,405,402,407,403,418,415,414,
    415,416,414,417,420,416,410,413,411,412,427,413,418,411,419,421,429,420,426,400,401,430,441,442,
    434,423,435,423,427,426,429,431,433,430,444,432,431,398,399,438,422,434,423,437,435,444,441,443,
    438,437,439,443,432,444,446,463,461,449,450,448,461,462,460,451,452,450,464,446,447,459,460,458,
    453,454,452,457,455,453,446,458,447,455,456,454,458,462,447,463,464,462,456,449,448,452,454,456,
    467,475,474,469,477,475,477,471,479,474,476,466,469,485,473,470,484,472,476,472,468,475,478,476,
    478,479,470,484,481,480,483,487,485,473,487,471,472,480,468,467,483,469,474,466,479,468,481,466,
    490,489,488,552,551,491,488,545,490,494,493,492,564,563,495,492,565,494,497,498,496,499,562,498,
    555,496,498,501,502,500,503,567,502,558,500,502,518,510,512,534,572,509,555,559,520,550,545,521,
    519,515,514,520,599,632,534,509,535,605,613,521,513,505,508,513,518,512,569,531,528,523,529,531,
    522,526,520,551,546,511,574,527,530,517,513,511,510,554,553,532,530,522,533,523,531,533,536,532,
    576,535,506,570,571,533,536,583,575,535,580,539,541,538,539,539,580,581,540,542,543,614,631,576,
    566,501,560,493,563,546,500,559,561,489,544,549,578,583,584,623,610,571,585,540,543,639,525,568,
    496,556,557,587,588,538,588,534,538,588,587,584,489,551,544,578,541,579,586,538,540,493,546,547,
    579,539,581,634,527,574,554,497,553,583,581,580,591,592,590,593,596,592,597,594,596,595,590,594,
    596,590,592,593,595,597,603,613,605,627,628,629,632,641,643,614,619,615,601,621,630,641,617,643,
    642,524,640,598,632,599,638,644,645,605,604,603,599,620,598,602,601,630,613,611,612,619,642,640,
    633,602,636,623,627,625,615,616,614,609,639,608,644,606,645,611,622,612,628,528,629,608,638,609,
    636,634,633,625,624,623,649,656,657,684,683,682,664,678,679,650,664,658,649,681,648,659,666,660,
    666,684,682,663,653,651,661,668,662,655,646,647,682,675,672,675,673,672,671,676,685,677,671,670,
    677,679,680,680,653,669,683,668,667,678,680,679,696,689,697,723,724,722,718,704,719,704,690,698,
    718,721,686,706,699,700,724,706,722,693,703,691,708,701,702,686,695,687,715,723,722,713,715,712,
    725,711,724,711,717,710,719,710,717,693,720,709,708,723,707,720,718,719,1031,969,1030,727,775,749,
    969,732,733,1048,1047,1046,748,728,726,746,773,747,744,771,745,742,769,743,740,767,741,738,765,739,
    736,763,737,728,817,816,773,822,820,771,826,824,769,830,828,767,834,832,765,838,836,763,842,840,
    760,787,761,758,785,759,756,783,757,754,781,755,752,779,753,750,777,751,801,803,802,969,981,1001,
    840,843,841,1036,1035,1034,800,821,803,805,799,798,825,805,824,799,804,823,807,797,796,829,807,828,
    797,806,827,809,795,794,833,809,832,795,808,831,811,793,792,837,811,836,793,810,835,791,812,839,
    841,813,790,813,791,790,789,814,843,815,789,788,838,837,836,801,802,819,818,775,729,834,833,832,
    1031,1002,971,969,1001,1030,821,777,776,825,779,778,829,781,780,833,783,782,837,785,784,1044,1043,1042,
    841,787,786,1040,1039,1038,731,730,816,819,816,817,937,936,934,952,966,964,919,952,922,941,940,939,
    864,885,865,862,883,863,860,881,861,858,879,859,856,877,857,885,926,924,883,931,928,881,937,934,
    879,941,939,877,945,943,874,895,875,872,893,873,870,891,871,868,889,869,866,887,867,906,908,907,
    961,959,956,855,852,853,924,927,925,848,846,847,905,925,908,910,904,903,930,910,928,904,909,927,
    912,902,901,936,912,934,902,911,933,914,900,899,940,914,939,900,913,938,916,898,897,944,916,943,
    898,915,942,946,945,849,929,950,921,950,960,961,947,954,948,945,944,943,949,850,935,850,923,935,
    925,887,886,931,930,928,930,889,888,936,891,890,940,893,892,944,895,894,923,845,918,957,958,967,
    959,957,956,851,917,854,962,850,955,966,965,964,965,954,953,917,932,929,960,962,961,1032,1031,1030,
    980,1001,981,978,999,979,976,997,977,974,995,975,972,993,973,1001,1032,1030,999,1036,1034,997,1040,1038,
    995,1044,1042,993,1048,1046,990,1011,991,988,1009,989,986,1007,987,984,1005,985,982,1003,983,822,821,820,
    830,829,828,1012,731,1029,1002,982,971,1022,1020,1019,1035,1022,1019,1020,1021,1033,1024,1018,1017,1039,1024,1038,
    1018,1023,1037,1026,1016,1015,1043,1026,1042,1016,1025,1041,1014,1027,1045,1047,1028,1046,1028,1014,1013,1012,1029,1049,
    970,735,734,1031,1003,1002,1035,1005,1004,1039,1007,1006,1043,1009,1008,826,825,824,1047,1011,1010,970,969,971,
    1051,1052,1050,1053,1056,1052,1057,1054,1056,1055,1050,1054,1056,1050,1052,1053,1055,1057,1059,1060,1058,1074,1072,1070,
    1069,1058,1068,1066,1062,1068,1061,1069,1067,1067,1063,1065,1060,1068,1058,1063,1068,1062,1070,1073,1071,1067,1064,1066,
    1062,1071,1063,1075,1064,1065,1075,1072,1074,1071,1075,1065,1086,1088,1082,1079,1076,1077,1080,1093,1086,1092,1080,1081,
    1087,1092,1081,1084,1093,1085,1082,1079,1077,1084,1089,1088,1081,1083,1087,1082,1089,1083,1092,1093,1091,1108,1094,1095,
    1103,1104,1109,1099,1096,1095,1097,1102,1096,1096,1101,1095,1110,1099,1094,1099,1104,1098,1098,1103,1097,1106,1116,1110,
    1100,1113,1107,1100,1115,1111,1106,1114,1112,1109,1116,1115,1107,1114,1108,1111,1121,1117,1113,1120,1114,1111,1119,1113,
    1112,1122,1116,1112,1120,1118,1115,1122,1121,1100,1111,1113,1100,1109,1115,1106,1112,1116,1106,1108,1114,1193,1132,1124,
    1161,1130,1186,1127,1161,1135,1130,1124,1126,1126,1209,1210,1146,1173,1147,1144,1171,1145,1142,1169,1143,1140,1167,1141,
    1138,1165,1139,1136,1163,1137,1173,1175,1174,1171,1177,1176,1169,1179,1178,1167,1181,1180,1165,1183,1182,1163,1185,1184,
    1160,1130,1128,1158,1185,1159,1156,1183,1157,1154,1181,1155,1152,1179,1153,1150,1177,1151,1148,1175,1149,1200,1202,1201,
    1126,1133,1134,1132,1123,1124,1199,1173,1202,1204,1198,1197,1176,1197,1171,1198,1203,1175,1206,1196,1195,1178,1195,1169,
    1196,1205,1177,1208,1194,1193,1180,1193,1167,1194,1207,1179,1210,1192,1191,1182,1191,1165,1192,1209,1181,1190,1211,1183,
    1184,1189,1163,1212,1190,1189,1188,1213,1185,1186,1187,1161,1214,1188,1187,348,326,329,329,331,343,331,333,322,
    333,320,322,322,321,337,321,339,335,321,335,337,331,322,337,343,348,329,318,316,332,332,330,318,
    330,328,342,318,330,336,328,325,349,328,349,342,336,330,342,338,323,334,323,318,336,166,167,160,
    160,165,166,161,159,157,157,164,158,164,163,158,104,102,106,106,102,108,110,102,100,100,98,110,
    154,98,96,154,156,152,152,155,151,153,150,151,150,149,148,148,149,147,147,149,146,27,21,20,
    30,188,29,29,189,32,26,32,21,19,186,18,192,23,22,25,38,24,35,25,36,37,191,39,
    36,190,37,190,23,193,19,189,187,19,21,32,25,23,36,419,405,409,445,421,398,398,417,402,
    402,415,406,406,419,409,424,412,400,433,420,428,416,399,403,414,403,407,418,407,408,404,418,408,
    400,410,404,178,172,176,174,183,171,175,177,174,177,175,179,935,959,949,947,958,918,918,959,923,
    922,957,919,929,956,917,917,957,854,963,917,923,920,917,896,953,918,965,952,854,922,965,854,964,
    851,918,845,856,897,847,859,898,858,861,900,860,863,902,862,865,904,864,847,901,848,875,916,874,
    873,914,872,871,912,870,869,910,868,867,908,866,853,911,912,761,813,760,759,811,758,757,809,756,
    755,807,754,753,805,752,751,803,750,749,731,727,991,1028,990,989,1026,988,987,1024,986,985,1022,984,
    982,1021,971,804,735,803,737,789,736,739,791,738,741,793,740,743,795,742,745,797,744,747,799,746,
    726,801,748,973,1012,972,975,1014,974,977,1016,976,979,1018,978,981,1020,980,801,969,733,5,191,190,
    0,195,194,194,191,5,4,0,1,2,5,3,3,1,2,11,9,189,185,184,6,11,188,185,
    8,6,7,10,7,6,9,11,8,21,25,24,99,111,167,23,19,18,27,20,28,24,38,33,
    34,39,22,34,22,31,18,30,31,28,20,24,22,18,31,31,28,33,33,34,31,21,19,23,
    34,33,13,13,14,12,14,13,35,12,37,39,16,26,27,15,31,30,17,15,29,28,31,16,
    15,17,16,35,13,33,16,17,32,12,14,36,45,41,49,43,47,46,40,53,52,51,46,50,
    47,45,50,42,48,49,49,41,43,51,50,49,46,51,48,53,57,56,57,55,54,55,40,44,
    56,54,44,53,40,55,58,61,60,61,65,64,64,65,63,63,58,59,64,62,59,76,80,77,
    80,81,77,85,84,71,71,84,70,72,76,77,73,71,72,72,77,73,72,70,67,69,66,71,
    68,74,76,73,77,75,77,81,79,74,78,80,66,82,85,75,79,78,67,83,66,83,82,66,
    75,78,74,67,66,68,69,75,68,68,66,69,70,84,83,87,89,88,89,91,90,91,93,92,
    91,89,93,87,95,93,93,95,94,95,87,86,94,86,88,88,90,92,97,166,135,101,99,139,
    100,140,138,105,103,143,104,106,107,109,119,121,106,116,117,108,110,120,115,112,114,116,118,119,
    118,120,121,120,122,123,122,124,125,124,126,127,127,126,114,120,110,154,154,152,122,108,118,116,
    109,107,117,132,134,135,128,132,133,131,129,128,134,136,137,136,138,139,138,140,141,140,142,143,
    142,144,145,100,102,142,101,141,143,96,98,138,105,145,144,97,137,139,104,144,142,128,150,132,
    153,155,132,158,131,130,152,151,124,160,123,125,96,136,134,164,159,125,163,164,127,165,133,135,
    123,160,167,167,111,121,103,109,111,150,128,129,162,163,112,147,114,126,146,113,114,133,165,161,
    161,157,133,148,126,124,155,156,134,169,171,170,171,183,182,170,182,181,173,172,175,169,173,174,
    183,177,176,176,180,183,181,182,183,181,180,169,179,175,172,180,176,173,177,179,178,4,1,193,
    195,0,4,187,7,10,39,191,194,195,192,22,39,194,195,185,188,30,18,186,184,184,185,30,
    10,6,184,9,7,187,1,3,190,206,210,211,211,207,206,202,203,201,202,201,200,206,207,202,
    199,203,202,201,197,196,196,204,206,202,207,205,198,205,204,209,208,204,196,197,198,205,209,204,
    197,199,198,211,210,208,204,208,210,207,211,209,213,215,214,216,228,229,216,217,219,219,221,220,
    225,223,222,223,213,212,221,225,224,228,232,233,214,227,216,231,232,228,216,218,220,231,228,227,
    222,212,214,227,228,216,216,220,222,224,222,220,217,229,215,233,230,226,215,213,223,233,226,229,
    221,219,217,229,226,215,215,223,217,225,221,223,215,226,227,230,233,232,235,237,236,226,230,231,
    237,239,238,239,241,240,241,243,242,243,245,244,245,235,234,239,237,235,235,245,243,243,241,239,
    248,249,247,263,262,250,250,252,253,254,255,253,256,257,259,246,247,257,258,259,255,267,266,262,
    265,261,266,248,246,254,256,258,254,254,252,250,250,262,248,266,261,262,246,256,254,254,250,248,
    267,263,264,251,253,255,255,259,257,257,247,249,249,260,251,264,263,260,251,255,257,257,249,251,
    261,260,249,266,267,264,270,271,269,265,264,260,272,273,271,274,275,273,276,277,275,278,279,277,
    268,269,279,269,271,273,273,275,277,277,279,269,288,284,291,283,294,290,285,287,281,283,282,280,
    281,296,297,288,289,286,289,287,285,291,292,295,292,290,294,280,282,290,290,292,291,291,284,285,
    286,285,284,280,290,285,296,298,299,298,300,301,302,307,305,287,289,288,288,293,294,295,294,293,
    303,302,287,287,288,294,294,283,297,299,301,303,294,297,299,287,302,298,302,300,298,296,281,287,
    305,307,306,300,305,304,301,304,306,303,306,307,312,314,315,315,313,327,310,308,311,324,309,311,
    313,309,324,314,312,308,327,324,308,323,317,319,318,323,321,322,320,316,317,325,326,311,315,314,
    326,348,346,326,346,340,339,321,345,326,340,319,345,321,319,343,337,336,348,343,342,341,344,345,
    344,338,339,338,334,335,340,346,347,346,348,349,334,336,337,317,323,344,323,338,344,347,349,325,
    317,344,341,341,347,325,351,353,352,352,353,355,355,357,356,355,353,351,351,359,355,357,359,358,
    359,351,350,358,350,352,352,354,358,362,365,369,363,368,365,369,365,368,367,360,362,361,362,360,
    362,361,364,367,366,369,368,363,360,371,373,372,373,377,376,376,377,375,375,371,370,376,374,370,
    373,371,375,390,396,395,385,390,391,381,383,382,391,395,394,383,381,378,383,385,384,396,397,393,
    393,394,396,385,383,379,385,387,386,389,392,378,378,393,397,387,389,388,392,394,393,387,391,392,
    389,381,380,388,380,386,382,384,386,379,397,396,411,413,401,400,404,405,439,401,413,398,402,403,
    407,406,409,404,408,409,402,406,407,418,419,415,415,417,416,417,421,420,410,412,413,436,413,438,
    412,425,427,427,438,413,418,410,411,428,420,429,421,440,441,441,429,421,401,439,437,437,426,401,
    424,400,426,430,429,441,434,422,423,424,426,427,423,422,427,427,425,424,433,428,429,429,430,432,
    432,431,429,430,442,444,399,433,431,431,443,398,445,398,443,438,427,422,423,426,437,445,443,441,
    444,442,441,441,440,445,439,436,438,438,434,435,435,437,438,443,431,432,461,459,446,446,465,463,
    449,451,450,461,463,462,451,453,452,464,465,446,459,461,460,453,455,454,453,451,449,449,457,453,
    446,459,458,455,457,456,464,447,462,458,460,462,463,465,464,456,457,449,456,448,450,450,452,456,
    467,469,475,469,473,477,477,473,471,468,466,476,474,475,476,469,483,485,470,486,484,476,478,472,
    475,477,478,470,472,478,478,477,479,484,486,481,483,482,487,473,485,487,472,484,480,467,482,483,
    486,470,466,479,471,467,487,482,467,467,474,479,466,481,486,470,479,466,487,467,471,468,480,481,
    490,491,489,491,490,552,545,552,490,488,548,545,494,495,493,495,494,564,565,564,494,492,550,565,
    497,499,498,555,498,562,499,554,562,555,556,496,501,503,502,558,502,567,503,566,567,558,559,500,
    518,516,510,525,509,568,534,589,572,572,568,509,504,505,556,505,557,556,520,506,504,532,522,558,
    567,514,532,558,567,532,520,504,556,558,522,520,562,561,559,559,558,520,520,556,555,555,562,559,
    533,515,564,533,564,565,521,523,565,507,509,521,549,508,548,548,508,507,523,533,565,548,507,521,
    547,552,545,545,548,521,521,565,550,550,547,545,532,514,515,516,518,514,519,517,515,515,533,532,
    514,518,519,506,520,615,526,527,636,527,635,636,640,524,619,619,524,506,520,526,636,602,630,621,
    520,636,602,615,619,506,643,617,615,632,643,615,602,621,599,632,615,520,602,599,520,506,535,509,
    539,538,534,509,507,504,539,534,535,509,504,506,529,521,627,509,525,609,525,607,609,629,528,627,
    627,528,529,521,509,638,638,645,606,509,609,638,625,627,521,612,622,613,613,622,625,638,606,605,
    613,625,521,521,638,605,504,507,508,508,511,513,513,512,505,510,505,512,504,508,505,513,519,518,
    529,528,531,569,570,531,523,521,529,522,530,526,511,508,544,508,549,544,515,517,563,563,564,515,
    552,547,546,563,517,546,551,552,546,546,517,511,511,544,551,526,530,527,573,574,530,517,519,513,
    557,505,553,510,516,560,514,567,566,560,561,562,516,514,566,560,562,554,516,566,560,553,505,510,
    510,560,554,533,537,536,506,524,577,576,582,535,506,577,576,537,533,571,531,570,533,571,584,537,
    573,530,532,532,536,575,575,573,532,535,582,580,541,540,538,540,541,542,573,575,633,576,577,618,
    577,642,618,634,574,633,633,574,573,575,598,600,614,616,631,576,618,614,600,633,575,620,601,600,
    598,620,600,575,576,598,616,641,631,598,576,631,566,503,501,493,495,563,561,560,501,501,500,561,
    549,548,488,488,489,549,537,584,583,585,543,542,542,578,585,583,536,537,584,585,578,572,571,610,
    570,569,626,569,628,626,639,568,608,608,568,572,571,570,626,623,624,610,571,626,623,637,608,572,
    604,644,637,603,604,637,637,572,603,624,611,610,603,572,610,585,586,540,639,607,525,557,553,497,
    497,496,557,588,589,534,584,571,589,571,572,589,586,585,587,587,585,584,584,589,588,489,491,551,
    578,542,541,586,587,538,547,550,492,492,493,547,579,541,539,634,635,527,554,499,497,582,576,575,
    575,583,582,578,579,581,580,582,583,578,581,583,591,593,592,593,597,596,597,595,594,595,591,590,
    596,594,590,593,591,595,603,610,613,627,626,628,632,631,641,614,618,619,601,620,621,641,616,617,
    642,577,524,598,631,632,638,637,644,605,606,604,599,621,620,602,600,601,613,610,611,619,618,642,
    633,600,602,623,626,627,615,617,616,609,607,639,644,604,606,611,624,622,628,569,528,608,637,638,
    636,635,634,625,622,624,649,648,656,684,685,683,664,652,678,650,652,664,647,646,678,649,651,653,
    646,648,681,652,650,647,678,652,647,649,653,681,681,678,646,659,665,666,666,665,684,663,669,653,
    661,667,668,655,654,646,660,666,672,683,667,675,672,666,682,682,683,675,661,675,667,675,674,673,
    684,665,671,659,671,665,676,662,668,685,684,671,676,668,685,677,676,671,669,663,677,677,670,679,
    658,664,670,680,669,677,670,664,679,680,681,653,683,685,668,678,681,680,696,688,689,723,725,724,
    718,692,704,704,692,690,688,686,721,687,690,692,687,692,718,689,688,721,693,691,689,687,718,686,
    721,693,689,706,705,699,724,705,706,693,709,703,708,707,701,686,694,695,722,706,712,700,712,706,
    715,701,707,715,707,723,722,712,715,713,714,715,699,705,711,725,708,716,705,724,711,716,711,725,
    708,702,716,711,716,717,717,703,709,717,709,720,698,710,704,719,704,710,717,720,719,693,721,720,
    708,725,723,720,721,718,1031,971,969,727,729,775,969,968,732,1048,1049,1047,748,774,728,746,772,773,
    744,770,771,742,768,769,740,766,767,738,764,765,736,762,763,728,774,817,773,772,822,771,770,826,
    769,768,830,767,766,834,765,764,838,763,762,842,760,786,787,758,784,785,756,782,783,754,780,781,
    752,778,779,750,776,777,801,800,803,840,842,843,1036,1037,1035,750,803,776,800,747,773,800,773,820,
    821,776,803,800,820,821,805,804,799,745,771,798,825,778,805,798,771,824,778,752,805,798,824,805,
    822,772,799,746,799,772,804,751,777,823,822,799,804,777,823,807,806,797,743,769,796,829,780,807,
    796,769,828,780,754,807,796,828,807,826,770,797,744,797,770,806,753,779,827,826,797,806,779,827,
    809,808,795,741,767,794,833,782,809,794,767,832,782,756,809,794,832,809,830,768,795,742,795,768,
    808,755,781,831,830,795,808,781,831,811,810,793,739,765,792,837,784,811,792,765,836,784,758,811,
    792,836,811,834,766,793,740,793,766,810,757,783,835,834,793,810,783,835,838,764,791,738,791,764,
    812,759,785,839,838,791,812,785,839,737,763,790,841,786,813,790,763,840,786,760,813,790,840,841,
    813,812,791,842,762,789,736,789,762,814,761,787,843,842,789,814,787,843,815,814,789,838,839,837,
    817,774,801,748,801,774,802,749,775,819,817,801,802,775,819,818,819,775,834,835,833,821,823,777,
    825,827,779,829,831,781,833,835,783,837,839,785,1044,1045,1043,841,843,787,1040,1041,1039,818,729,731,
    727,731,729,730,726,728,816,818,731,730,728,816,819,818,816,937,938,936,952,951,966,919,951,952,
    941,942,940,864,884,885,862,882,883,860,880,881,858,878,879,856,876,877,885,884,926,883,882,931,
    881,880,937,879,878,941,877,876,945,874,894,895,872,892,893,870,890,891,868,888,889,866,886,887,
    906,905,908,962,955,959,949,959,955,956,921,950,961,962,959,956,950,961,855,849,852,924,926,927,
    848,844,846,866,908,886,905,865,885,905,885,924,925,886,908,905,924,925,910,909,904,863,883,903,
    930,888,910,903,883,928,888,868,910,903,928,910,926,884,904,864,904,884,909,867,887,927,926,904,
    909,887,927,912,911,902,861,881,901,936,890,912,901,881,934,890,870,912,901,934,912,931,882,902,
    862,902,882,911,869,889,933,931,902,911,889,933,914,913,900,859,879,899,940,892,914,899,879,939,
    892,872,914,899,939,914,937,880,900,860,900,880,913,871,891,938,937,900,913,891,938,916,915,898,
    857,877,897,944,894,916,897,877,943,894,874,916,897,943,916,941,878,898,858,898,878,915,873,893,
    942,941,898,915,893,942,855,875,895,855,895,946,846,849,945,876,856,847,847,846,945,855,946,849,
    945,876,847,929,932,950,950,932,960,947,953,954,945,946,944,949,955,850,850,963,923,925,927,887,
    931,933,930,930,933,889,936,938,891,940,942,893,944,946,895,923,920,845,966,951,957,919,957,951,
    958,948,954,967,966,957,958,954,967,959,958,957,851,896,917,962,963,850,966,967,965,965,967,954,
    917,960,932,960,963,962,1032,1033,1031,980,1000,1001,978,998,999,976,996,997,974,994,995,972,992,993,
    1001,1000,1032,999,998,1036,997,996,1040,995,994,1044,993,992,1048,990,1010,1011,988,1008,1009,986,1006,1007,
    984,1004,1005,982,1002,1003,822,823,821,830,831,829,1012,730,731,1022,1021,1020,979,999,1019,1035,1004,1022,
    1019,999,1034,1004,984,1022,1019,1034,1035,1032,1000,1020,980,1020,1000,1021,983,1003,1033,1032,1020,1021,1003,1033,
    1024,1023,1018,977,997,1017,1039,1006,1024,1017,997,1038,1006,986,1024,1017,1038,1024,1036,998,1018,978,1018,998,
    1023,985,1005,1037,1036,1018,1023,1005,1037,1026,1025,1016,975,995,1015,1043,1008,1026,1015,995,1042,1008,988,1026,
    1015,1042,1026,1040,996,1016,976,1016,996,1025,987,1007,1041,1040,1016,1025,1007,1041,1044,994,1014,974,1014,994,
    1027,989,1009,1045,1044,1014,1027,1009,1045,973,993,1013,1047,1010,1028,1013,993,1046,1010,990,1028,1013,1046,1028,
    1028,1027,1014,1048,992,1012,972,1012,992,1029,991,1011,1049,1048,1012,1029,1011,1049,970,971,735,1031,1033,1003,
    1035,1037,1005,1039,1041,1007,1043,1045,1009,826,827,825,1047,1049,1011,970,968,969,1051,1053,1052,1053,1057,1056,
    1057,1055,1054,1055,1051,1050,1056,1054,1050,1053,1051,1055,1059,1061,1060,1064,1074,1070,1070,1062,1064,1069,1059,1058,
    1066,1064,1062,1061,1059,1069,1067,1069,1063,1060,1066,1068,1063,1069,1068,1070,1072,1073,1066,1060,1061,1067,1065,1064,
    1066,1061,1067,1062,1070,1071,1075,1074,1064,1075,1073,1072,1071,1073,1075,1065,1063,1071,1077,1076,1082,1080,1086,1082,
    1084,1088,1086,1082,1076,1080,1079,1078,1076,1080,1091,1093,1078,1081,1080,1092,1091,1080,1080,1076,1078,1087,1090,1092,
    1087,1085,1090,1084,1086,1093,1093,1090,1085,1082,1083,1079,1084,1085,1089,1085,1087,1089,1081,1078,1083,1083,1089,1087,
    1078,1079,1083,1082,1088,1089,1092,1090,1093,1095,1101,1108,1108,1106,1094,1101,1107,1108,1109,1100,1107,1107,1101,1102,
    1102,1103,1107,1104,1105,1109,1109,1107,1103,1095,1094,1099,1099,1098,1097,1097,1096,1099,1097,1103,1102,1096,1102,1101,
    1110,1109,1105,1105,1099,1110,1094,1106,1110,1099,1105,1104,1098,1104,1103,1109,1110,1116,1107,1113,1114,1111,1115,1121,
    1113,1119,1120,1111,1117,1119,1112,1118,1122,1112,1114,1120,1115,1116,1122,1188,1136,1137,1188,1137,1189,1127,1135,1187,
    1188,1189,1124,1124,1127,1187,1124,1187,1188,1200,1132,1199,1146,1147,1199,1199,1132,1198,1124,1189,1190,1190,1138,1139,
    1190,1139,1191,1198,1146,1199,1144,1145,1197,1197,1198,1132,1124,1190,1191,1192,1140,1141,1124,1191,1192,1197,1132,1196,
    1196,1144,1197,1142,1143,1195,1196,1132,1195,1194,1142,1195,1195,1132,1194,1192,1141,1193,1194,1132,1193,1124,1192,1193,
    1161,1129,1130,1127,1129,1161,1126,1128,1130,1130,1129,1124,1124,1123,1125,1129,1127,1124,1125,1126,1124,1214,1160,1128,
    1126,1134,1208,1202,1148,1149,1134,1201,1202,1214,1128,1126,1158,1159,1213,1213,1214,1126,1202,1149,1203,1204,1150,1151,
    1134,1202,1203,1204,1151,1205,1134,1203,1204,1212,1158,1213,1134,1204,1205,1206,1152,1153,1134,1205,1206,1212,1213,1126,
    1156,1157,1211,1211,1212,1126,1206,1153,1207,1208,1154,1155,1134,1206,1207,1208,1155,1209,1134,1207,1208,1210,1156,1211,
    1126,1208,1209,1210,1211,1126,1146,1172,1173,1144,1170,1171,1142,1168,1169,1140,1166,1167,1138,1164,1165,1136,1162,1163,
    1173,1172,1175,1171,1170,1177,1169,1168,1179,1167,1166,1181,1165,1164,1183,1163,1162,1185,1160,1186,1130,1158,1184,1185,
    1156,1182,1183,1154,1180,1181,1152,1178,1179,1150,1176,1177,1148,1174,1175,1200,1199,1202,1126,1125,1133,1132,1131,1123,
    1148,1202,1174,1199,1147,1173,1173,1174,1202,1204,1203,1198,1145,1171,1197,1176,1150,1204,1204,1197,1176,1175,1172,1198,
    1146,1198,1172,1203,1149,1175,1206,1205,1196,1143,1169,1195,1178,1152,1206,1206,1195,1178,1177,1170,1196,1144,1196,1170,
    1205,1151,1177,1208,1207,1194,1141,1167,1193,1180,1154,1208,1208,1193,1180,1179,1168,1194,1142,1194,1168,1207,1153,1179,
    1210,1209,1192,1139,1165,1191,1182,1156,1210,1210,1191,1182,1181,1166,1192,1140,1192,1166,1209,1155,1181,1183,1164,1190,
    1138,1190,1164,1211,1157,1183,1137,1163,1189,1184,1158,1212,1212,1189,1184,1212,1211,1190,1185,1162,1188,1136,1188,1162,
    1213,1159,1185,1135,1161,1187,1186,1160,1214,1214,1187,1186,1214,1213,1188,166,97,167,160,159,165,164,157,159,
    161,165,159,110,108,102,154,110,98,154,96,156,152,156,155,148,151,150,155,153,151,27,26,21,
    29,188,189,19,187,186,192,193,23,25,35,38,37,190,191,190,36,23,19,32,189,419,411,405,
    445,440,421,398,421,417,402,417,415,406,415,419,424,425,412,433,399,420,416,420,399,414,416,403,
    418,414,407,404,410,418,400,412,410,174,177,183,935,923,959,947,948,958,918,958,959,922,854,957,
    929,921,956,917,956,957,963,960,917,920,923,917,953,947,918,952,964,854,965,918,854,851,854,918,
    856,857,897,859,899,898,861,901,900,863,903,902,865,905,904,906,848,905,847,897,898,899,900,847,
    847,898,899,905,848,904,902,903,848,904,848,903,847,900,901,902,848,901,875,855,916,873,915,914,
    871,913,912,869,911,910,867,909,908,916,855,915,853,907,908,909,910,853,853,908,909,915,855,914,
    912,913,855,914,855,913,853,910,911,912,855,853,761,814,813,759,812,811,757,810,809,755,808,807,
    753,806,805,751,804,803,749,802,731,991,1029,1028,989,1027,1026,987,1025,1024,985,1023,1022,982,983,1021,
    731,802,971,804,805,735,1029,731,971,805,806,735,1028,1029,971,806,807,735,1027,1028,971,807,808,735,
    1026,1027,971,808,809,735,1025,1026,971,809,810,735,1024,1025,971,810,811,735,1023,1024,971,811,812,735,
    1022,1023,971,812,813,735,1021,1022,971,813,814,735,971,802,735,814,815,735,802,803,735,737,790,789,
    739,792,791,741,794,793,743,796,795,745,798,797,747,800,799,726,730,801,973,1013,1012,975,1015,1014,
    977,1017,1016,979,1019,1018,981,969,1020,788,789,733,791,792,733,733,789,790,792,793,733,733,790,791,
    1020,969,1019,1017,1018,969,1019,969,1018,733,793,794,795,796,733,733,794,795,1017,969,1016,1014,1015,969,
    1016,969,1015,733,796,797,798,799,733,733,797,798,1014,969,1013,730,1012,969,1013,969,1012,733,799,800,
    801,730,969,733,800,801,1216,1217,1215,1222,1219,1221,1228,1232,1231,1218,1221,1217,1231,1230,1229,1238,1242,1246,
    1239,1243,1235,1230,1239,1229,1228,1237,1227,1225,1236,1226,1223,1234,1224,1245,1254,1253,1248,1252,1256,1240,1247,1239,
    1235,1244,1236,1238,1245,1237,1233,1242,1234,1251,1256,1252,1249,1254,1250,1241,1250,1242,1247,1251,1243,1245,1249,1241,
    1248,1255,1247,1244,1251,1252,1246,1250,1254,1218,1225,1222,1257,1260,1259,1260,1263,1259,1263,1262,1261,1262,1257,1261,
    1260,1262,1264,1268,1271,1267,1222,1266,1220,1266,1267,1265,1269,1272,1270,1224,1269,1223,1266,1269,1270,1273,1342,1357,
    1273,1333,1275,1276,1329,1274,1339,1340,1276,1280,1294,1290,1342,1341,1328,1278,1292,1289,1359,1295,1293,1322,1328,1323,
    1327,1286,1324,1284,1298,1296,1286,1300,1298,1297,1287,1285,1329,1338,1304,1295,1285,1283,1344,1302,1294,1290,1279,1280,
    1331,1308,1306,1285,1321,1278,1289,1281,1278,1308,1334,1303,1358,1309,1295,1357,1311,1309,1356,1313,1311,1355,1315,1313,
    1346,1320,1318,1320,1349,1316,1314,1310,1300,1309,1313,1299,1289,1308,1303,1304,1307,1290,1326,1322,1321,1324,1328,1327,
    1337,1332,1323,1275,1335,1331,1351,1312,1350,1348,1316,1349,1275,1276,1341,1343,1310,1351,1350,1314,1348,1336,1301,1303,
    1352,1315,1353,1359,1301,1360,1340,1304,1338,1352,1317,1319,1294,1284,1296,1341,1351,1348,1339,1307,1340,1362,1363,1361,
    1368,1365,1367,1364,1366,1368,1383,1381,1382,1376,1372,1371,1368,1380,1374,1379,1370,1372,1375,1383,1373,1374,1375,1373,
    1382,1371,1369,1380,1378,1377,1366,1361,1365,1367,1363,1371,1374,1378,1376,1386,1387,1385,1388,1389,1387,1390,1391,1389,
    1394,1386,1392,1393,1398,1400,1385,1393,1391,1391,1386,1385,1395,1408,1398,1392,1399,1394,1392,1403,1401,1405,1401,1403,
    1391,1404,1389,1388,1405,1406,1394,1397,1396,1387,1408,1385,1396,1405,1386,1389,1407,1387,1407,1402,1398,1390,1406,1403,
    1391,1400,1402,1410,1411,1409,1412,1413,1411,1414,1415,1413,1416,1417,1415,1410,1418,1412,1418,1419,1417,1420,1409,1419,
    1411,1415,1409,1234,1228,1224,1235,1229,1239,1229,1225,1231,1228,1231,1224,1224,1231,1225,1241,1237,1245,1227,1233,1223,
    1244,1240,1236,1236,1230,1226,1230,1232,1226,1227,1223,1232,1226,1232,1223,1223,1220,1226,1219,1216,1215,1225,1270,1272,
    1222,1272,1268,1220,1270,1226,1270,1220,1266,1223,1269,1216,1218,1271,1224,1271,1218,1267,1269,1265,1216,1218,1265,1267,
    1342,1354,1355,1354,1352,1353,1355,1354,1353,1355,1356,1357,1357,1358,1273,1358,1359,1273,1360,1336,1273,1359,1360,1273,
    1342,1355,1357,1274,1329,1345,1345,1344,1274,1344,1343,1274,1351,1350,1348,1343,1351,1274,1347,1346,1349,1346,1341,1348,
    1348,1349,1346,1341,1274,1351,1216,1218,1217,1222,1220,1219,1228,1227,1232,1218,1222,1221,1231,1232,1230,1238,1234,1242,
    1239,1247,1243,1230,1240,1239,1228,1238,1237,1225,1235,1236,1223,1233,1234,1245,1246,1254,1248,1244,1252,1240,1248,1247,
    1235,1243,1244,1238,1246,1245,1233,1241,1242,1251,1255,1256,1249,1253,1254,1241,1249,1250,1247,1255,1251,1245,1253,1249,
    1248,1256,1255,1244,1243,1251,1246,1242,1250,1218,1224,1225,1257,1258,1260,1260,1264,1263,1263,1264,1262,1262,1258,1257,
    1260,1258,1262,1268,1272,1271,1222,1268,1266,1266,1268,1267,1269,1271,1272,1224,1271,1269,1266,1265,1269,1273,1336,1333,
    1276,1330,1329,1330,1276,1340,1337,1339,1276,1340,1338,1330,1280,1282,1294,1299,1317,1342,1317,1354,1342,1326,1287,1299,
    1328,1325,1342,1288,1327,1300,1346,1318,1341,1300,1327,1328,1325,1326,1299,1299,1342,1325,1341,1318,1300,1300,1328,1341,
    1278,1277,1292,1281,1293,1295,1359,1358,1295,1295,1283,1281,1322,1325,1328,1279,1324,1280,1327,1288,1286,1286,1284,1282,
    1280,1324,1286,1286,1282,1280,1284,1286,1298,1286,1288,1300,1297,1299,1287,1304,1302,1329,1329,1330,1338,1302,1345,1329,
    1295,1297,1285,1344,1345,1302,1290,1291,1279,1331,1335,1308,1285,1287,1326,1326,1321,1285,1278,1281,1285,1321,1277,1278,
    1281,1283,1285,1289,1293,1281,1308,1335,1334,1358,1357,1309,1357,1356,1311,1356,1355,1313,1355,1353,1315,1346,1347,1320,
    1320,1347,1349,1298,1300,1310,1318,1320,1316,1314,1312,1310,1318,1316,1314,1310,1296,1298,1318,1314,1300,1297,1295,1309,
    1309,1311,1313,1313,1315,1317,1319,1317,1315,1299,1297,1309,1313,1317,1299,1301,1293,1289,1289,1292,1306,1306,1308,1289,
    1303,1301,1289,1291,1290,1305,1302,1304,1290,1290,1294,1302,1307,1305,1290,1326,1325,1322,1324,1323,1328,1291,1305,1337,
    1305,1339,1337,1324,1279,1291,1322,1323,1332,1277,1321,1292,1331,1306,1332,1292,1321,1322,1323,1324,1291,1291,1337,1323,
    1332,1306,1292,1292,1322,1332,1331,1332,1275,1275,1333,1335,1334,1335,1333,1351,1310,1312,1348,1314,1316,1341,1342,1275,
    1275,1332,1337,1342,1273,1275,1276,1274,1341,1275,1337,1276,1343,1296,1310,1350,1312,1314,1303,1334,1336,1336,1360,1301,
    1334,1333,1336,1352,1319,1315,1359,1293,1301,1340,1307,1304,1352,1354,1317,1296,1343,1344,1294,1282,1284,1296,1344,1294,
    1339,1305,1307,1362,1364,1363,1368,1366,1365,1364,1362,1366,1383,1384,1381,1376,1378,1379,1371,1381,1375,1381,1384,1375,
    1376,1379,1372,1371,1375,1376,1373,1383,1382,1382,1369,1364,1369,1363,1364,1373,1382,1364,1368,1367,1370,1373,1364,1368,
    1380,1377,1374,1374,1373,1368,1368,1370,1380,1379,1380,1370,1375,1384,1383,1374,1376,1375,1382,1381,1371,1380,1379,1378,
    1366,1362,1361,1371,1372,1367,1367,1365,1361,1372,1370,1367,1363,1369,1371,1367,1361,1363,1374,1377,1378,1386,1388,1387,
    1388,1390,1389,1390,1392,1391,1394,1396,1386,1393,1395,1398,1385,1395,1393,1391,1392,1386,1395,1385,1408,1392,1401,1399,
    1392,1390,1403,1403,1406,1405,1405,1397,1399,1399,1401,1405,1391,1402,1404,1388,1386,1405,1394,1399,1397,1387,1407,1408,
    1396,1397,1405,1389,1404,1407,1398,1408,1407,1407,1404,1402,1402,1400,1398,1390,1388,1406,1391,1393,1400,1410,1412,1411,
    1412,1414,1413,1414,1416,1415,1416,1418,1417,1414,1412,1418,1410,1420,1418,1418,1416,1414,1418,1420,1419,1420,1410,1409,
    1419,1409,1415,1411,1413,1415,1415,1417,1419,1234,1238,1228,1235,1225,1229,1241,1233,1237,1227,1237,1233,1244,1248,1240,
    1236,1240,1230,1223,1216,1220,1219,1220,1216,1225,1226,1270,1222,1225,1272,1218,1216,1265,1480,1495,1477,1444,1427,1441,
    1421,1458,1422,1427,1445,1441,1431,1447,1445,1435,1449,1447,1439,1476,1449,1450,1470,1437,1448,1437,1433,1434,1461,1430,
    1440,1464,1466,1446,1433,1429,1438,1463,1434,1430,1457,1421,1442,1429,1423,1468,1465,1438,1494,1498,1495,1436,1462,1464,
    1426,1459,1425,1424,1442,1423,1455,1473,1456,1453,1456,1454,1451,1454,1452,1502,1452,1501,1501,1508,1502,1432,1459,1462,
    1490,1488,1485,1492,1481,1488,1472,1466,1467,1488,1482,1487,1488,1486,1485,1492,1489,1491,1483,1492,1491,1477,1482,1481,
    1480,1483,1479,1480,1493,1494,1496,1477,1495,1497,1495,1498,1500,1494,1493,1484,1477,1481,1505,1503,1504,1503,1507,1504,
    1443,1504,1442,1507,1442,1504,1442,1501,1446,1452,1446,1501,1454,1446,1452,1456,1448,1454,1473,1450,1456,1503,1444,1441,
    1503,1441,1508,1502,1508,1441,1441,1445,1502,1502,1445,1451,1442,1507,1501,1453,1445,1447,1455,1447,1449,1475,1449,1476,
    1457,1489,1458,1461,1491,1457,1463,1483,1461,1465,1479,1463,1469,1493,1465,1496,1467,1466,1478,1466,1464,1482,1464,1462,
    1487,1462,1459,1486,1459,1460,1428,1425,1427,1425,1431,1427,1432,1435,1431,1439,1436,1440,1471,1440,1472,1470,1438,1437,
    1437,1434,1433,1433,1430,1429,1421,1429,1430,1422,1423,1421,1505,1444,1506,1489,1485,1486,1486,1458,1489,1443,1460,1444,
    1443,1422,1458,1444,1426,1428,1499,1497,1498,1476,1473,1475,1500,1467,1497,1472,1469,1468,1476,1468,1474,1470,1474,1468,
    1471,1472,1476,1480,1494,1495,1444,1428,1427,1421,1457,1458,1427,1431,1445,1431,1435,1447,1435,1439,1449,1439,1471,1476,
    1450,1474,1470,1448,1450,1437,1434,1463,1461,1440,1436,1464,1446,1448,1433,1438,1465,1463,1430,1461,1457,1442,1446,1429,
    1468,1469,1465,1494,1499,1498,1436,1432,1462,1426,1460,1459,1424,1443,1442,1455,1475,1473,1453,1455,1456,1451,1453,1454,
    1502,1451,1452,1501,1507,1508,1432,1425,1459,1490,1492,1488,1492,1484,1481,1472,1440,1466,1488,1481,1482,1488,1487,1486,
    1492,1490,1489,1483,1484,1492,1477,1478,1482,1480,1484,1483,1480,1479,1493,1496,1478,1477,1497,1496,1495,1500,1499,1494,
    1484,1480,1477,1505,1506,1503,1503,1508,1507,1443,1505,1504,1454,1448,1446,1456,1450,1448,1473,1474,1450,1503,1506,1444,
    1453,1451,1445,1455,1453,1447,1475,1455,1449,1457,1491,1489,1461,1483,1491,1463,1479,1483,1465,1493,1479,1469,1500,1493,
    1496,1497,1467,1478,1496,1466,1482,1478,1464,1487,1482,1462,1486,1487,1459,1428,1426,1425,1425,1432,1431,1432,1436,1435,
    1439,1435,1436,1471,1439,1440,1470,1468,1438,1437,1438,1434,1433,1434,1430,1421,1423,1429,1422,1424,1423,1505,1443,1444,
    1489,1490,1485,1486,1460,1458,1443,1458,1460,1443,1424,1422,1444,1460,1426,1499,1500,1497,1476,1474,1473,1500,1469,1467,
    1472,1467,1469,1476,1472,1468,1511,1514,1513,1513,1516,1515,1516,1517,1515,1546,1531,1547,1518,1519,1517,1561,1563,1564,
    1513,1522,1511,1519,1525,1517,1515,1523,1513,1517,1524,1515,1550,1529,1551,1550,1531,1530,1554,1529,1528,1538,1543,1537,
    1530,1537,1536,1528,1535,1534,1532,1537,1531,1529,1536,1535,1541,1542,1544,1536,1541,1535,1546,1570,1555,1537,1542,1536,
    1535,1540,1534,1522,1552,1553,1524,1548,1549,1567,1584,1569,1553,1564,1565,1545,1560,1548,1524,1552,1523,1547,1562,1559,
    1526,1548,1525,1553,1568,1556,1585,1586,1587,1527,1538,1532,1547,1558,1546,1551,1566,1563,1549,1564,1552,1549,1560,1561,
    1551,1562,1550,1546,1527,1532,1565,1563,1566,1555,1569,1554,1534,1539,1533,1572,1583,1581,1554,1527,1555,1512,1509,1510,
    1558,1557,1575,1559,1557,1558,1526,1556,1545,1568,1545,1556,1533,1544,1538,1580,1583,1582,1573,1580,1578,1577,1580,1582,
    1527,1534,1533,1522,1556,1521,1520,1509,1519,1566,1574,1573,1509,1526,1519,1579,1583,1575,1571,1587,1576,1576,1586,1570,
    1569,1585,1574,1511,1521,1509,1559,1561,1560,1511,1512,1514,1513,1514,1516,1516,1518,1517,1546,1532,1531,1518,1520,1519,
    1561,1562,1563,1513,1523,1522,1519,1526,1525,1515,1524,1523,1517,1525,1524,1550,1530,1529,1550,1547,1531,1554,1551,1529,
    1538,1544,1543,1530,1531,1537,1528,1529,1535,1532,1538,1537,1529,1530,1536,1544,1539,1540,1540,1541,1544,1542,1543,1544,
    1536,1542,1541,1567,1555,1570,1546,1558,1570,1537,1543,1542,1535,1541,1540,1522,1523,1552,1524,1525,1548,1567,1570,1586,
    1586,1584,1567,1553,1552,1564,1545,1557,1560,1524,1549,1552,1547,1550,1562,1526,1545,1548,1553,1565,1568,1585,1584,1586,
    1527,1533,1538,1547,1559,1558,1551,1554,1566,1549,1561,1564,1549,1548,1560,1551,1563,1562,1546,1555,1527,1565,1564,1563,
    1566,1554,1569,1555,1567,1569,1534,1540,1539,1572,1575,1583,1581,1573,1572,1554,1528,1527,1512,1511,1509,1575,1572,1576,
    1576,1570,1558,1572,1571,1576,1568,1577,1579,1579,1575,1557,1576,1558,1575,1557,1568,1579,1559,1560,1557,1526,1521,1556,
    1568,1557,1545,1533,1539,1544,1580,1581,1583,1573,1581,1580,1577,1578,1580,1582,1579,1577,1527,1528,1534,1522,1553,1556,
    1520,1510,1509,1577,1568,1578,1565,1566,1573,1574,1571,1572,1566,1569,1574,1578,1568,1565,1574,1572,1573,1573,1578,1565,
    1509,1521,1526,1579,1582,1583,1571,1574,1585,1585,1587,1571,1576,1587,1586,1569,1584,1585,1511,1522,1521,1559,1562,1561,
    1592,1591,1589,1603,1598,1599,1601,1596,1597,1603,1594,1604,1600,1597,1598,1605,1596,1602,1599,1597,1595,1606,1603,1604,
    1608,1601,1609,1610,1603,1611,1607,1604,1605,1607,1602,1608,1605,1594,1595,1612,1611,1615,1614,1609,1613,1616,1611,1606,
    1612,1609,1610,1614,1607,1608,1610,1601,1600,1618,1615,1616,1620,1613,1621,1623,1612,1615,1618,1617,1619,1619,1614,1620,
    1616,1607,1617,1621,1638,1654,1593,1624,1592,1590,1629,1589,1639,1643,1621,1635,1633,1618,1618,1634,1650,1622,1613,1612,
    1642,1639,1638,1626,1627,1631,1643,1642,1632,1653,1645,1649,1641,1623,1647,1651,1630,1650,1655,1654,1650,1654,1627,1626,
    1648,1636,1652,1646,1620,1640,1638,1639,1635,1633,1634,1632,1625,1590,1626,1590,1591,1626,1621,1622,1642,1622,1641,1642,
    1637,1653,1638,1653,1654,1638,1654,1626,1621,1626,1591,1621,1621,1642,1638,1621,1591,1627,1591,1592,1627,1592,1624,1627,
    1655,1652,1636,1655,1636,1639,1621,1627,1655,1640,1620,1643,1620,1621,1643,1655,1639,1621,1618,1588,1631,1588,1589,1631,
    1589,1629,1631,1651,1649,1645,1651,1645,1635,1618,1631,1651,1647,1623,1633,1623,1618,1633,1651,1635,1618,1628,1593,1630,
    1593,1588,1630,1618,1619,1632,1619,1646,1632,1644,1648,1634,1648,1650,1634,1650,1630,1618,1630,1588,1618,1618,1632,1634,
    1589,1588,1593,1593,1592,1589,1591,1590,1589,1603,1600,1598,1601,1602,1596,1603,1599,1594,1600,1601,1597,1605,1595,1596,
    1595,1594,1599,1599,1598,1597,1597,1596,1595,1606,1611,1603,1608,1602,1601,1610,1600,1603,1607,1606,1604,1607,1605,1602,
    1605,1604,1594,1612,1610,1611,1614,1608,1609,1616,1615,1611,1612,1613,1609,1614,1617,1607,1610,1609,1601,1618,1623,1615,
    1620,1614,1613,1623,1622,1612,1618,1616,1617,1619,1617,1614,1616,1606,1607,1593,1628,1624,1590,1625,1629,1622,1621,1613,
    1642,1643,1639,1628,1630,1627,1631,1629,1626,1625,1626,1629,1627,1624,1628,1630,1631,1627,1647,1633,1642,1632,1646,1643,
    1640,1643,1646,1642,1641,1647,1633,1632,1642,1653,1637,1645,1641,1622,1623,1651,1631,1630,1649,1651,1654,1650,1648,1655,
    1652,1655,1648,1654,1653,1649,1651,1650,1654,1654,1655,1627,1648,1644,1636,1646,1619,1620,1644,1634,1639,1635,1645,1638,
    1637,1638,1645,1639,1636,1644,1634,1635,1639,1633,1635,1634,
};
static const uint8_t kAkM[3036] = {
    0,0,0,0,0,0,0,0,0,2,0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0,0,0,
    0,0,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,3,2,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,3,2,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,1,
    0,0,1,0,0,1,0,0,0,1,0,1,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,4,4,4,4,2,2,
    2,4,4,4,4,4,4,4,4,4,4,4,2,2,2,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,3,2,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,
    0,1,0,0,1,0,0,1,1,0,0,0,1,0,1,0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,0,0,0,0,0,0,4,4,4,4,2,2,2,4,4,4,4,4,4,4,4,4,4,4,2,2,2,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,0,0,1,0,1,0,1,0,0,1,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,2,
    2,2,2,2,2,2,2,2,2,2,2,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,7,5,5,5,8,5,8,8,5,8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,8,8,8,5,8,8,8,8,8,8,8,8,8,8,8,8,
    8,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,7,5,5,5,8,5,8,8,5,8,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,8,8,8,5,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,12,12,12,12,12,12,11,11,11,11,11,12,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    13,13,13,11,11,13,13,13,11,11,13,13,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,12,12,12,12,12,12,12,12,11,11,11,11,11,12,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,13,13,13,13,13,13,13,13,13,13,13,11,11,13,13,13,
    13,13,13,13,11,11,13,13,13,13,13,13,
};
static const uint8_t kAkPal[14] = {
    84,162,67,112,53,79,107,230,48,56,65,60,0,43,
};
static const uint16_t kAkPart[6] = {
    0,2186,2574,2746,2896,3036,
};
static const int16_t kAkLV[1032] = {
    703,-1370,-5551,729,-1430,-6890,-703,-1370,-5551,-729,-1430,-6890,734,-842,-10458,751,-764,-10844,-734,-842,-10458,-751,-764,-10844,
    -616,-1537,-3141,-779,-253,-3501,-722,-2120,-13308,-682,-376,-13308,616,-1537,-3141,779,-253,-3501,690,-2196,-13433,706,-293,-13449,
    -652,-2235,-11813,-507,-2076,-10123,652,-2235,-11813,507,-2076,-10123,-389,-1871,-7514,-290,-3092,-8199,-397,-1963,-8310,-335,-2905,-7705,
    389,-1871,-7514,290,-3092,-8199,397,-1963,-8310,335,-2905,-7705,-221,-3903,-8035,221,-3903,-8035,-181,-3056,-9548,181,-3056,-9548,
    -310,-3198,-10291,310,-3198,-10291,-400,-4666,-11035,400,-4666,-11035,-645,-4922,-11453,645,-4922,-11453,-461,-5477,-11208,461,-5477,-11208,
    -461,-5916,-11599,461,-5916,-11599,-645,-6590,-11533,645,-6590,-11533,-645,-6241,-13048,645,-6241,-13048,-645,-5566,-12858,645,-5566,-12858,
    -461,-4670,-12793,461,-4670,-12793,382,-2118,-12921,-382,-2118,-12921,-277,-5805,-13346,277,-5805,-13346,-382,-3221,-12192,382,-3221,-12192,
    -277,-2656,-12335,277,-2656,-12335,-421,-3766,-10907,421,-3766,-10907,-277,-6615,-11399,277,-6615,-11399,-487,-3505,-10808,-645,-3968,-12250,
    487,-3505,-10808,645,-3968,-12250,-383,777,-1422,-305,846,-12780,305,846,-12780,538,560,-1114,-421,697,-6670,421,697,-6670,
    -825,-290,-6966,825,-290,-6966,-153,-2088,-9449,153,-2088,-9449,700,-1238,-10793,742,-1265,-10450,921,-1104,-10754,921,-1104,-10557,
    -700,-1238,-10793,-742,-1265,-10450,-921,-1104,-10754,-921,-1104,-10557,-669,-2210,-19084,669,-2210,-19084,-507,-418,-19629,507,-418,-19629,
    -479,-2765,-19686,479,-2765,-19686,-336,-3968,-19066,336,-3968,-19066,-539,-4086,-19629,539,-4086,-19629,275,-4576,-19018,-275,-4576,-19018,
    252,-4599,-19577,-252,-4599,-19577,-186,-2313,-15376,-186,-2054,-14812,-193,-2321,-16342,186,-2313,-15376,186,-2054,-14812,215,-2289,-16302,
    -104,-2349,-17194,253,-2719,-18241,131,-2426,-17307,-253,-2719,-18241,-515,-1379,-18940,515,-1379,-18940,-253,-1604,-16663,253,-1604,-16663,
    -253,-2732,-18568,253,-2732,-18568,-785,-1246,-13851,-783,-1192,-14362,785,-1246,-13851,783,-1192,-14362,968,-2234,-13591,968,-300,-13591,
    -375,262,-14734,-375,262,-18914,375,262,-18914,375,262,-14734,678,-488,-18914,688,-529,-14734,-678,-488,-18914,-688,-529,-14734,
    778,-1408,-18375,990,-1224,-18182,883,-1029,-18670,-778,-1408,-18375,-883,-1029,-18670,-990,-1224,-18182,778,-1408,-14977,990,-1225,-15233,
    883,-1029,-14831,-778,-1408,-14977,-883,-1029,-14831,-990,-1225,-15233,-1017,-871,-15397,1017,-871,-15397,-1017,-877,-18012,1017,-877,-18012,
    -409,-4024,-20258,-437,-401,-20442,409,-4024,-20258,437,-401,-20442,-630,-1795,-19877,630,-1795,-19877,-205,-4472,-20258,205,-4472,-20258,
    506,387,3134,506,387,2248,506,387,1595,506,387,710,-506,387,3134,-506,387,2248,-506,387,1595,-506,387,710,
    -433,-1049,3983,-338,553,3976,-525,-1340,2200,650,-1103,3931,394,633,3975,525,-1340,2200,-437,657,-399,412,616,-482,
    -780,-505,2406,780,-505,2406,-624,-190,-468,624,-190,-468,-934,-980,2464,934,-980,2464,-938,-569,2424,938,-569,2424,
    479,-1471,-2282,-479,-1471,-2282,-559,325,-3504,559,325,-3504,940,-1009,-1540,940,-580,-1540,841,-500,-1840,-841,-500,-1840,
    -940,-580,-1540,-940,-1009,-1540,569,-534,3987,922,-504,3906,922,-921,3906,815,-538,3254,474,-1406,3039,922,-504,3254,
    922,-921,3254,-465,-459,4005,-922,-504,3906,-922,-921,3906,-815,-538,3254,-474,-1406,3039,-922,-504,3254,-922,-921,3254,
    -253,1177,-7289,253,1177,-7289,-388,1120,-1555,388,1120,-1555,-253,1177,-2079,-253,1177,-2947,-253,1177,-3816,-253,1177,-4684,
    -253,1177,-5552,-253,1177,-6420,253,1177,-6420,253,1177,-5552,253,1177,-4684,253,1177,-3816,253,1177,-2947,253,1177,-2079,
    262,475,6674,-188,1047,3890,-306,1120,-193,244,1053,7433,-175,340,6783,306,1120,-193,-244,1053,6565,235,1083,3867,
    -253,1100,3216,-253,1100,2348,-253,1100,1480,-253,1100,611,253,1100,611,253,1100,1480,253,1100,2348,253,1100,3216,
    -237,333,7790,-217,1036,7477,244,1053,6565,284,385,7782,-338,1330,-12485,338,1330,-12485,-253,1177,-8157,-300,1202,-9363,
    -347,1448,-10208,-374,1424,-11104,374,1424,-11104,347,1448,-10208,300,1202,-9363,253,1177,-8157,-720,-415,-7109,-352,729,-7109,
    720,-415,-7109,352,729,-7109,0,4,7962,-172,-423,6854,236,-420,7293,-236,-420,7293,-198,-491,8523,312,-572,8496,
    315,-532,6869,302,-1661,1148,302,-1661,280,302,-1661,-589,302,-1661,-1457,-302,-1661,-1457,-302,-1661,-589,-302,-1661,280,
    -302,-1661,1148,-317,-374,12147,-199,-409,10750,369,-413,12128,298,-413,10784,-198,1575,11757,-270,1611,11263,216,1593,11263,
    169,1567,11771,-231,1921,11730,-338,1947,11333,338,1947,11333,231,1921,11730,-165,174,11910,248,249,11732,-248,249,11094,
    248,249,11094,-335,1532,-12039,335,1532,-12039,216,1842,-12260,216,1842,-12395,-216,1842,-12395,-216,1842,-12260,-427,1791,-11338,
    427,1791,-11338,-545,-1809,-5394,-384,-1009,-7391,545,-1809,-5394,384,-1009,-7391,-424,-3588,-6982,-400,-3268,-4798,424,-3588,-6982,
    400,-3268,-4798,-384,-5585,-6463,-545,-5276,-4509,384,-5585,-6463,545,-5276,-4509,-384,-6954,-5858,-466,-6477,-3838,384,-6954,-5858,
    466,-6477,-3838,304,-3588,-7165,-304,-3588,-7165,-351,-1813,-4925,-351,-1009,-4925,351,-1813,-4925,351,-1009,-4925,-333,-5276,-4097,
    333,-5276,-4097,466,-7839,-2963,-466,-7839,-2963,-384,-8855,-4659,384,-8855,-4659,388,-5318,-3919,-388,-5318,-3919,-333,-6504,-3259,
    333,-6504,-3259,333,-7596,-2558,-333,-7596,-2558,-133,-531,4403,266,-531,4403,0,-921,10796,0,-1004,12264,-109,-1132,8411,
    -73,-1011,6970,-109,-1025,7358,219,-1066,7033,219,-1066,7358,219,-1281,8411,-332,-532,15457,379,-532,15425,317,-531,16975,
    -181,-470,16975,0,-961,16975,426,-531,16160,-341,-445,16160,0,-1024,16160,0,24,16160,-424,-532,14928,424,-532,14928,
};
static const uint16_t kAkLT[2370] = {
    11,14,10,37,65,35,13,8,12,14,18,16,11,13,15,5,4,19,6,7,17,24,23,27,
    22,25,21,20,21,23,26,27,25,23,22,21,25,23,21,22,24,26,21,29,28,27,28,29,
    31,21,30,30,25,31,30,33,31,31,32,30,33,17,32,32,19,33,27,21,23,38,34,36,
    43,41,61,42,45,43,44,53,45,46,48,52,52,49,53,48,55,49,54,57,55,57,51,50,
    63,54,48,46,52,44,32,59,33,58,35,59,34,39,35,38,41,39,40,61,41,43,60,42,
    62,58,32,37,39,49,18,55,57,65,49,55,41,49,39,45,47,41,51,56,16,43,45,41,
    15,72,11,67,69,68,67,70,66,11,68,15,72,67,11,71,66,70,19,0,12,2,17,8,
    31,19,75,74,31,75,19,30,17,30,74,17,76,5,77,78,77,76,79,76,77,81,82,80,
    80,83,81,86,15,87,85,15,14,11,84,10,85,10,84,84,89,85,88,91,89,92,95,90,
    87,93,86,89,93,87,86,88,84,95,96,94,90,94,91,91,96,93,93,97,92,102,98,101,
    103,98,100,107,94,105,104,105,106,106,100,104,105,113,109,113,107,112,91,112,90,90,105,91,
    90,97,95,104,103,100,104,103,106,117,114,116,117,114,115,15,118,14,119,14,118,121,123,122,
    122,120,121,120,126,121,122,125,123,124,109,125,127,108,126,130,122,123,131,115,137,134,129,135,
    129,109,130,108,132,126,131,139,133,132,120,121,117,128,134,129,136,135,139,115,138,117,136,125,
    138,133,139,142,129,133,135,140,139,145,149,148,148,88,92,93,151,146,88,146,144,148,89,88,
    89,149,93,151,97,150,145,87,147,146,150,144,148,92,149,144,97,92,157,158,168,154,153,169,
    176,165,177,164,166,161,181,171,175,153,154,164,184,174,170,161,157,156,167,178,166,178,9,183,
    8,9,13,169,181,175,168,184,174,183,162,168,169,181,182,168,184,183,162,185,172,165,180,176,
    176,169,165,191,186,189,189,175,169,190,188,163,165,192,190,189,188,192,169,192,173,193,198,196,
    174,196,168,195,197,160,199,162,197,195,196,199,199,168,172,236,66,202,200,210,201,240,242,241,
    211,209,208,212,208,207,213,207,206,214,206,205,215,205,204,203,204,202,239,243,240,225,229,226,
    224,230,225,223,167,221,218,228,221,218,161,217,223,217,164,217,231,224,226,228,227,219,216,234,
    219,222,233,220,233,222,241,237,236,238,201,245,245,239,238,68,203,69,68,236,237,166,246,170,
    247,248,246,249,171,248,167,170,171,248,170,246,247,167,249,202,66,166,166,171,170,202,69,66,
    221,167,203,166,69,167,221,202,218,216,161,164,217,235,164,161,233,217,220,252,251,235,252,216,
    235,256,252,250,255,235,232,251,254,232,250,235,220,251,253,220,216,252,258,12,190,197,263,264,
    12,177,8,261,176,260,262,260,259,263,259,258,264,258,257,162,257,165,197,165,190,91,96,89,
    89,109,113,109,111,106,111,103,106,109,106,105,113,91,89,104,100,110,110,108,104,108,88,112,
    104,108,107,88,97,90,88,90,112,107,108,112,59,65,55,64,55,33,33,18,19,18,50,19,
    42,40,44,44,40,46,48,40,38,38,36,48,63,36,34,63,58,54,62,32,54,32,17,16,
    16,17,51,16,11,10,17,3,6,6,3,7,9,2,8,0,13,12,15,18,14,4,1,19,
    5,1,4,1,13,0,9,3,2,9,11,7,15,13,5,68,73,15,69,71,68,235,233,232,
    220,234,216,217,226,218,200,236,202,11,15,14,37,49,65,19,12,17,16,10,14,12,8,17,
    17,16,18,18,19,17,11,9,13,19,18,5,16,17,7,24,20,23,22,26,25,20,22,21,
    26,24,27,23,20,22,25,27,23,22,20,24,23,21,28,21,25,29,29,25,27,27,23,28,
    31,25,21,30,21,25,30,32,33,31,33,32,33,19,17,32,17,19,27,25,21,39,37,35,
    42,44,45,47,53,49,44,52,53,52,48,49,48,54,55,54,56,57,57,56,51,47,45,53,
    32,58,59,58,34,35,34,38,39,38,40,41,40,60,61,43,61,60,42,60,40,50,18,57,
    41,47,49,64,33,59,16,56,54,15,73,72,67,66,69,11,67,68,73,71,70,70,72,73,
    72,70,67,71,69,66,19,1,0,2,3,17,74,30,31,19,31,30,19,77,76,78,79,77,
    79,78,76,17,80,81,7,81,80,81,83,82,80,82,83,86,11,15,85,87,15,11,86,84,
    85,14,10,10,14,15,10,15,11,84,88,89,88,90,91,92,97,95,93,92,86,89,91,93,
    87,85,89,86,92,88,92,90,88,95,97,96,90,95,94,91,94,96,93,96,97,103,102,101,
    98,99,100,102,99,98,103,101,98,107,95,94,104,107,105,106,103,100,91,105,94,113,105,107,
    91,113,112,90,107,105,95,107,90,15,119,118,119,15,14,121,120,123,122,123,120,120,127,126,
    122,124,125,109,117,125,115,108,127,123,125,136,136,130,123,124,122,130,131,108,115,134,128,129,
    124,130,109,129,128,109,108,131,133,133,132,108,131,137,139,121,126,132,132,138,120,127,120,138,
    117,109,128,129,130,136,127,138,115,139,137,115,117,134,135,135,136,117,138,132,133,142,143,129,
    135,141,140,86,145,148,145,147,149,149,147,87,144,92,88,93,96,151,88,89,146,148,149,89,
    93,146,89,151,96,97,145,86,87,146,151,150,96,93,92,149,87,86,86,148,149,92,97,96,
    93,149,92,144,150,97,173,165,180,160,161,156,168,162,160,166,170,159,168,160,156,159,170,168,
    158,159,168,168,156,157,169,171,155,163,165,169,152,164,163,171,167,155,152,163,169,153,152,169,
    169,155,154,162,177,165,8,12,176,165,163,160,8,176,177,165,160,162,160,163,164,160,164,161,
    164,167,166,169,175,171,181,182,171,155,167,154,154,167,164,164,152,153,168,170,174,183,184,170,
    161,166,158,166,159,158,161,158,157,167,179,178,162,185,177,179,167,182,171,182,167,182,13,179,
    183,170,166,166,178,183,179,13,9,9,178,179,13,182,176,13,176,12,183,9,177,8,177,9,
    183,177,162,176,182,169,191,187,186,163,188,186,189,191,175,190,192,188,165,173,192,169,173,165,
    190,192,189,189,186,188,169,189,192,193,194,198,193,195,160,174,198,196,195,199,197,199,172,162,
    162,172,168,196,199,197,195,193,196,199,196,168,236,67,66,200,209,210,240,243,242,211,210,209,
    212,211,208,213,212,207,214,213,206,215,214,205,203,215,204,239,244,243,225,230,229,224,231,230,
    223,164,167,218,227,228,218,166,161,161,164,217,217,223,231,226,229,228,219,235,216,219,234,222,
    220,232,233,241,242,237,238,200,201,245,244,239,68,237,203,68,67,236,166,247,246,247,249,248,
    249,167,171,167,166,170,248,171,170,247,166,167,218,202,166,166,170,218,171,221,218,218,170,171,
    166,167,171,202,203,69,221,171,167,167,69,203,166,66,69,221,203,202,164,235,216,216,220,161,
    217,233,235,232,161,220,161,232,233,232,220,253,250,232,254,235,255,256,250,254,255,251,252,256,
    232,253,251,190,165,257,260,176,12,190,257,258,260,12,259,259,12,258,197,8,263,8,177,261,
    8,261,262,264,162,197,8,262,263,12,176,177,261,177,176,262,261,260,263,262,259,264,263,258,
    162,264,257,197,162,165,59,35,65,18,33,55,64,59,55,48,46,40,63,48,36,63,34,58,
    16,54,32,58,62,54,16,7,11,15,5,18,1,5,13,9,7,3,68,71,73,235,219,233,
    220,222,234,217,224,225,226,227,218,217,225,226,231,223,230,229,230,223,221,228,229,229,223,221,
    210,211,203,245,201,237,211,212,203,244,245,237,212,213,203,243,244,237,213,214,203,242,243,237,
    214,215,203,237,201,203,201,210,203,205,206,202,202,204,205,241,236,240,202,206,207,240,236,239,
    202,207,208,208,209,202,239,236,238,200,238,236,202,209,200,270,272,271,272,275,271,271,276,272,
    270,273,269,269,274,270,266,271,268,273,272,269,276,269,272,268,278,267,278,279,277,277,280,278,
    270,277,269,240,236,281,239,237,244,237,242,244,241,239,240,240,243,237,281,243,282,284,282,237,
    236,286,281,282,236,281,282,241,281,288,282,242,281,287,241,269,267,272,271,278,280,267,278,272,
    269,277,265,266,279,270,266,277,279,281,241,240,242,282,243,237,243,282,270,269,272,272,276,275,
    271,275,276,270,274,273,269,273,274,266,270,271,273,276,272,276,273,269,268,280,278,278,280,279,
    277,279,280,270,279,277,239,236,237,243,244,242,236,239,241,242,237,282,236,241,281,237,236,240,
    240,239,244,240,244,243,281,240,243,284,283,282,236,285,286,282,237,236,282,243,240,282,240,281,
    282,242,241,269,265,267,271,272,278,268,271,280,266,265,277,311,320,312,289,308,2,302,311,298,
    294,307,289,319,321,320,300,296,312,0,309,291,303,315,301,299,301,297,305,297,306,27,306,23,
    308,309,310,307,296,309,311,294,318,294,312,296,290,24,20,20,23,293,306,293,23,297,293,306,
    24,295,27,27,295,305,302,318,311,314,319,302,320,313,304,317,304,312,292,291,24,291,295,24,
    296,299,295,303,300,304,316,304,313,315,302,301,301,298,297,297,294,293,289,293,294,2,20,289,
    290,310,292,290,2,308,322,313,321,316,314,315,311,319,320,289,307,308,298,311,294,304,300,312,
    319,322,321,0,310,309,303,316,315,299,303,301,305,299,297,27,305,306,296,291,309,308,307,309,
    307,294,296,312,317,296,311,318,319,320,317,312,294,311,312,290,292,24,299,305,295,302,319,318,
    314,322,319,320,321,313,317,320,304,292,0,291,291,296,295,296,300,299,303,299,300,316,303,304,
    315,314,302,301,302,298,297,298,294,289,20,293,2,290,20,290,308,310,292,310,0,322,314,313,
    316,313,314,251,255,256,193,324,186,254,268,255,266,267,268,327,332,255,323,256,324,256,328,251,
    268,326,325,255,266,254,254,251,253,325,265,266,330,329,328,252,330,256,266,268,325,324,251,323,
    160,323,193,251,329,253,186,323,160,251,254,255,193,323,324,254,266,268,265,326,267,266,265,267,
    323,251,256,254,327,255,268,267,326,331,252,329,255,268,266,253,329,254,329,327,254,325,326,265,
    330,331,329,252,331,330,328,256,330,324,256,251,256,255,252,255,327,329,255,329,252,251,328,329,
    186,324,323,265,277,267,335,333,336,335,333,334,337,335,336,340,335,337,338,336,335,340,336,339,
    334,342,333,334,342,343,267,277,343,341,338,334,334,343,341,343,277,341,341,277,342,277,265,342,
    341,342,333,339,341,333,340,326,343,326,267,343,340,343,334,338,340,334,265,326,342,340,339,333,
    333,342,340,342,326,340,340,338,335,338,339,336,340,337,336,338,341,339,
};
static const uint8_t kAkLM[790] = {
    0,2,0,0,0,3,3,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    0,0,0,0,0,0,0,0,2,2,2,2,2,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,0,0,0,0,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,4,4,4,4,4,4,4,
    4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,
    2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,0,2,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,3,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,2,2,2,3,3,2,2,3,3,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,
    3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,5,5,5,5,5,5,7,5,5,8,8,5,5,5,5,5,5,5,5,5,5,5,8,8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,7,5,5,8,
    8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,12,12,11,11,11,11,
    13,13,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
};
static const int16_t kGrV[1005] = {
    -683,-8771,1832,-825,-6616,3403,575,-8789,1837,642,-6627,3429,-1088,-1810,5137,-987,-3814,5394,816,-1811,5173,813,-3794,5376,
    -1109,1192,5213,-1036,-873,5012,832,1191,5250,740,-890,5034,-845,3795,4607,-1005,2258,4875,718,2235,4903,564,3824,4623,
    -794,7979,1917,-1040,4237,3792,880,4248,3811,667,7982,1947,-1190,-4684,4449,-798,-5348,5718,549,-5374,5692,-1329,-6811,2947,
    562,-4100,4327,-843,-4100,4296,-2090,-3915,4018,-2117,-1219,4285,-2263,-1823,4695,-2255,1977,4302,-2467,-631,4696,-2467,1181,4696,
    642,-6627,-3429,576,-8789,-1837,-683,-8771,-1833,-825,-6616,-3404,-1088,-1810,-5137,816,-1811,-5174,840,-3496,-4861,-1080,-3495,-4827,
    -1109,1192,-5214,832,1191,-5251,740,-890,-5034,-1036,-873,-5013,564,3824,-4624,718,2235,-4904,-1005,2258,-4876,-845,3795,-4608,
    625,8318,-1866,880,4246,-3813,-1040,4234,-3794,-750,8314,-1837,-538,-4683,-4598,269,-4683,-4616,-1190,-4684,-4449,-1329,-6811,-2947,
    -843,-4100,-4297,562,-4100,-4328,-2090,-3915,-4018,-2264,-1823,-4696,-2117,-1219,-4285,-2467,1181,-4696,-2467,-631,-4696,-2255,1977,-4302,
    -2314,-6912,-2313,-1831,-8783,-624,-3716,-1813,-3716,-3404,-3649,-3404,-4477,-1811,-2773,-4018,-3915,-2090,-4695,-1823,-2264,-2773,-1811,-4477,
    -3772,1189,-3772,-3680,-784,-3680,-4696,1181,-2467,-4696,-631,-2467,-3292,3884,-3293,-3562,2378,-3562,-3583,4060,-2261,-4302,1977,-2256,
    -2261,4061,-3583,-1571,7747,-1571,-2769,4312,-2769,-1876,8334,-596,-3324,-6131,-2177,-3885,-4683,-2467,-3276,-4683,-3276,-2177,-6131,-3325,
    -2466,-4683,-3886,-1128,8406,-1066,-2948,-6813,-1329,-3419,-6649,-653,-4285,-1219,-2117,-3813,4245,-881,-1826,-8765,733,-3391,-6645,830,
    -5137,-1810,1088,-5173,-1811,-816,-4861,-3496,-840,-4827,-3495,1080,-5213,1192,1109,-5250,1191,-832,-5034,-890,-740,-5013,-873,1036,
    -4624,3824,-564,-4904,2235,-718,-4875,2258,1005,-4607,3795,845,-3794,4234,1040,-2206,8313,747,-4598,-4683,538,-4615,-4683,-269,
    -4449,-4684,1189,-4449,-4684,-1190,-2948,-6813,1328,-4297,-4100,843,-4327,-4100,-562,-4018,-3915,2090,-4695,-1823,2263,-4285,-1219,2117,
    -4696,1181,2467,-4696,-631,2467,-4302,1977,2255,-2314,-6912,2313,-3716,-1813,3716,-3404,-3649,3404,-2773,-1811,4477,-4477,-1811,2773,
    -3772,1189,3772,-3680,-784,3680,-3695,4226,3799,-3562,2378,3562,-2235,4072,3575,-3868,4254,2323,-1731,8231,2236,-2769,4312,2769,
    -2177,-6131,3324,-2466,-4683,3885,-3276,-4683,3276,-3324,-6131,2176,-3885,-4683,2466,-968,8288,967,-678,-9002,817,-714,-9002,-749,
    659,-9002,-785,622,-9002,850,1190,-4684,4449,1329,-6811,2947,2090,-3918,4019,2117,-1219,4285,2263,-1823,4695,2255,1977,4302,
    2467,-631,4696,2467,1181,4696,1190,-4684,-4449,1329,-6811,-2947,2090,-3915,-4018,2264,-1823,-4696,2117,-1219,-4285,2467,1181,-4696,
    2255,1977,-4302,2467,-631,-4696,2314,-6912,-2313,1831,-8783,-624,3716,-1813,-3716,3404,-3649,-3404,4477,-1811,-2773,4018,-3915,-2090,
    4695,-1823,-2264,2773,-1811,-4477,3772,1189,-3772,3680,-784,-3680,4696,1181,-2467,4696,-631,-2467,3292,3884,-3293,3583,4060,-2261,
    3562,2378,-3562,4302,1977,-2256,2261,4061,-3583,1571,7747,-1571,1876,8334,-596,2769,4312,-2769,3276,-4683,-3276,3885,-4683,-2467,
    3324,-6131,-2177,2177,-6131,-3325,2466,-4683,-3886,1079,8401,-1079,2948,-6813,-1329,3419,-6649,-653,4285,-1219,-2117,3813,4245,-881,
    3391,-6645,830,1826,-8765,733,5137,-1810,1088,4827,-3495,1080,4861,-3496,-840,5173,-1811,-816,5213,1192,1109,5013,-873,1036,
    5034,-890,-740,5250,1191,-832,4624,3824,-564,4607,3795,845,4875,2258,1005,4903,2235,-718,1944,8311,880,3794,4234,1040,
    4615,-4683,-269,4598,-4683,538,4449,-4684,1189,4449,-4684,-1190,2948,-6813,1328,4327,-4100,-562,4297,-4100,843,4018,-3915,2090,
    4695,-1823,2263,4285,-1219,2117,4696,1181,2467,4302,1977,2255,4696,-631,2467,2314,-6912,2313,3723,-1806,3724,3410,-3662,3410,
    2776,-1807,4483,4477,-1811,2773,3772,1189,3772,3681,-786,3681,3292,3884,3292,2260,4063,3582,3562,2378,3562,3583,4062,2260,
    1751,8218,2239,2769,4312,2769,3276,-4683,3276,2466,-4683,3885,2177,-6131,3324,3324,-6131,2176,3885,-4683,2466,899,8289,996,
    -1180,-8979,1235,-1180,-8979,-1236,1180,-8979,-1236,1180,-8979,1235,548,11612,-1843,-665,11614,-1817,-1352,11570,-1352,-1752,11561,-553,
    -1878,12622,-693,-1180,12428,-2369,885,12439,-2417,-1448,12133,-1335,-2024,12612,879,-1933,10111,-29,-1938,12511,2446,-2506,11044,3645,
    -1739,8401,3692,-1451,8444,4820,-1910,12522,3451,-2587,9770,915,1352,11570,-1352,1752,11559,-553,1907,10073,-64,1878,12622,-693,
    362,12854,-767,2024,12612,879,1843,11535,1074,1928,12617,2440,1859,11476,2481,2009,10037,622,1911,12522,3451,1455,8442,4820,
    1795,8373,3698,1018,12581,-3199,-1418,12581,-3194,-1313,9153,4146,-463,10800,4256,2248,11028,3651,1406,9074,4134,-1842,11386,1240,
    -2080,11376,2440,0,12773,3452,0,13149,2451,0,13104,1240,775,7332,5713,457,9983,4810,638,5879,6103,745,3793,6341,
    720,1959,6485,705,226,6465,688,-1329,6339,1728,9971,4707,1704,12875,-2305,2499,10249,3738,-13,12283,3722,-525,9905,4820,
    -782,7317,5691,1342,5809,5839,-890,5806,5854,-748,3791,6318,-726,1962,6460,-705,233,6442,-690,-1320,6322,0,-6000,5659,
    372,10580,4570,252,-4208,6038,204,10965,4372,2103,10352,2899,-723,5880,6097,-2213,10181,5384,-1704,12875,-2305,-2043,10178,3855,
    -372,10580,4570,-335,-4201,6032,-2481,10289,2235,-2382,10775,5082,-2525,9623,6499,-2633,8364,7012,-3331,8153,7078,-3484,6665,6920,
    -2784,6879,7016,-3599,5524,6377,-2935,5652,6495,-3723,4897,5407,-3723,4347,4863,-3758,4626,3996,-3581,4823,2123,-3301,6626,561,
    -2924,7030,440,-3197,8138,400,-3078,9494,896,-3019,10170,1520,-3053,10700,5349,-3087,10258,5717,-3195,9612,6538,
};
static const uint16_t kGrT[2217] = {
    1,0,2,1,2,3,6,4,5,6,5,7,10,8,9,10,9,11,12,13,14,12,14,15,
    16,17,18,16,18,19,1,21,20,1,3,22,1,22,21,23,0,1,21,22,24,21,24,25,
    20,21,25,20,25,26,1,20,26,4,6,11,4,11,9,4,9,27,5,28,27,5,27,26,
    5,26,25,7,5,25,7,25,24,8,10,14,8,14,13,8,13,29,30,31,29,30,29,27,
    12,15,18,12,18,17,32,33,34,32,34,35,36,37,38,36,38,39,40,41,42,40,42,43,
    44,45,46,44,46,47,48,49,50,48,50,51,32,35,52,32,52,53,35,54,52,34,55,35,
    53,52,56,53,56,57,54,35,58,54,58,56,52,54,56,39,38,57,39,57,56,39,56,58,
    59,39,58,59,58,60,36,60,43,37,36,43,37,43,42,61,62,60,61,60,63,40,63,46,
    41,40,46,41,46,45,44,47,50,44,50,49,64,34,65,68,66,67,68,67,69,70,68,69,
    71,59,58,66,71,58,66,58,67,74,72,73,74,73,75,72,61,62,72,62,73,76,77,78,
    78,77,79,80,63,77,80,77,76,81,82,78,81,78,83,51,80,82,51,82,81,64,84,85,
    64,85,86,87,64,86,87,86,88,89,81,83,51,81,89,86,85,69,86,69,67,88,86,67,
    88,67,58,87,88,58,87,58,35,87,55,64,84,64,90,85,84,91,85,91,69,70,69,92,
    68,70,92,66,68,92,66,92,73,66,73,60,71,66,60,59,71,60,75,73,92,74,75,92,
    74,92,79,72,74,79,72,79,77,72,77,63,61,72,63,62,61,63,62,63,60,73,62,60,
    83,78,93,51,50,80,76,78,82,80,76,82,91,65,94,91,94,95,96,97,98,96,98,99,
    100,101,102,100,102,103,104,105,106,104,106,107,83,93,108,83,108,109,91,95,110,91,110,111,
    95,112,110,91,111,113,94,114,95,90,65,91,111,110,115,111,115,116,113,111,116,113,116,69,
    91,113,69,112,95,117,112,117,115,110,112,115,99,98,116,99,116,115,99,115,117,118,99,117,
    118,117,119,96,119,103,97,96,103,97,103,102,97,102,92,98,70,92,98,92,69,98,69,116,
    120,121,119,120,119,122,100,122,106,101,100,106,101,106,105,101,105,79,75,74,79,75,79,92,
    104,107,108,104,108,93,123,94,0,126,124,125,126,125,26,28,126,26,127,118,117,124,127,117,
    124,117,125,31,128,129,31,129,30,128,120,121,128,121,129,130,131,132,132,131,29,133,122,131,
    133,131,130,134,135,132,134,132,16,109,133,135,109,135,134,123,136,137,123,137,138,139,123,138,
    139,138,140,141,134,16,109,134,141,138,137,26,138,26,125,140,138,125,140,125,117,139,140,117,
    139,117,95,139,114,123,136,123,23,137,136,1,137,1,26,28,26,27,126,28,27,124,126,27,
    124,27,129,124,129,119,127,124,119,118,127,119,30,129,27,31,30,27,31,27,29,128,31,29,
    128,29,131,128,131,122,120,128,122,121,120,122,121,122,119,129,121,119,16,132,17,109,108,133,
    130,132,135,133,130,135,107,106,122,107,122,133,107,133,108,79,105,104,79,104,78,93,78,104,
    47,46,63,47,63,80,47,80,50,29,13,12,29,12,132,17,132,12,142,143,144,142,144,145,
    3,146,22,147,3,2,146,24,22,146,148,24,3,148,146,6,149,11,7,148,149,7,149,150,
    7,24,148,10,151,14,152,149,151,152,151,153,32,53,154,33,32,155,154,156,32,154,57,156,
    53,57,154,38,156,57,157,158,156,157,156,38,37,42,158,159,160,158,159,158,161,41,45,160,
    162,163,33,166,167,165,166,165,164,168,167,166,169,156,157,164,165,156,164,156,169,172,173,171,
    172,171,170,170,171,161,170,161,159,174,175,176,175,177,176,178,176,160,178,174,176,179,180,175,
    179,175,181,48,179,181,48,181,178,162,182,183,162,183,184,185,186,182,185,182,162,180,179,187,
    179,48,187,182,165,167,182,167,183,186,156,165,186,165,182,185,32,156,185,156,186,185,162,155,
    184,188,162,183,167,189,183,189,184,168,190,167,166,190,168,164,190,166,164,171,190,164,158,171,
    169,158,164,157,158,169,173,190,171,172,177,190,172,190,173,170,177,172,170,176,177,170,160,176,
    159,160,170,161,158,160,161,160,159,171,158,161,180,191,175,48,178,49,174,181,175,178,181,174,
    189,192,193,189,193,163,194,195,196,194,196,197,198,199,200,198,200,201,202,203,204,202,204,205,
    180,206,207,180,207,191,189,208,209,189,209,192,192,209,210,189,211,208,193,192,212,188,189,163,
    208,213,214,208,214,209,211,213,208,211,167,213,189,167,211,210,215,192,210,214,215,209,214,210,
    195,214,213,195,213,196,195,215,214,216,217,215,216,215,195,194,199,217,197,200,199,197,199,194,
    197,190,200,196,167,190,196,190,168,196,213,167,218,219,217,218,217,220,198,204,219,201,205,204,
    201,204,198,201,177,205,173,190,177,173,177,172,202,191,207,202,207,203,221,2,193,224,148,223,
    224,223,222,150,148,224,225,215,216,222,223,215,222,215,225,153,152,227,153,227,226,226,227,220,
    226,220,218,228,229,230,229,151,230,231,230,219,231,228,230,232,19,229,232,229,233,206,232,233,
    206,233,231,221,234,235,221,235,236,237,238,234,237,234,221,19,232,239,232,206,239,234,223,148,
    234,148,235,238,215,223,238,223,234,237,192,215,237,215,238,237,221,212,236,147,221,235,148,3,
    235,3,236,150,149,148,224,149,150,222,149,224,222,227,149,222,217,227,225,217,222,216,217,225,
    152,149,227,153,151,149,153,149,152,226,151,153,226,230,151,226,219,230,218,219,226,220,217,219,
    220,219,218,227,217,220,19,18,229,206,231,207,228,233,229,231,233,228,203,231,219,203,219,204,
    203,207,231,177,175,202,177,202,205,191,202,175,44,178,160,44,160,45,44,49,178,151,229,15,
    151,15,14,18,15,229,0,94,240,65,34,241,33,163,242,193,2,243,141,239,89,239,187,89,
    240,142,0,94,142,240,65,143,142,65,142,94,241,143,65,34,143,241,33,144,143,33,143,34,
    242,144,33,163,144,242,193,145,144,193,144,163,243,145,193,2,145,243,0,142,145,0,145,2,
    109,141,89,109,89,83,19,239,141,19,141,16,180,187,239,180,239,206,51,89,187,51,187,48,
    4,28,5,8,31,30,8,30,9,27,28,4,29,31,8,27,9,30,39,59,36,43,62,61,
    43,61,40,60,36,59,60,62,43,63,40,61,65,90,64,64,55,34,35,55,87,90,91,84,
    99,118,96,97,70,98,103,121,120,103,120,100,101,74,75,101,75,102,119,96,118,92,70,97,
    119,121,103,122,100,120,79,74,101,92,102,75,0,23,123,123,114,94,95,114,139,23,1,136,
    7,150,6,11,152,153,11,153,10,6,150,149,10,153,151,152,11,149,37,157,38,41,159,161,
    41,161,42,157,37,158,42,161,158,159,41,160,162,188,163,33,155,162,185,155,32,184,189,188,
    194,216,195,196,168,197,198,218,220,198,220,199,200,173,172,200,172,201,216,194,217,197,168,190,
    199,220,217,218,198,219,201,172,177,173,200,190,221,147,2,193,212,221,237,212,192,236,3,147,
    249,250,244,249,244,245,244,48,51,244,51,245,51,89,246,51,246,245,249,245,246,251,249,246,
    248,251,246,248,246,247,89,83,247,89,247,246,253,247,83,255,256,257,255,257,258,262,180,261,
    260,187,48,260,48,244,244,250,260,260,250,264,260,264,263,260,263,261,261,180,187,261,187,260,
    266,267,268,266,268,269,270,271,272,272,271,257,272,257,256,254,258,270,254,270,267,252,254,267,
    252,267,265,263,248,252,263,252,265,263,250,249,263,249,248,249,250,273,249,273,274,274,273,250,
    274,250,249,134,109,206,134,206,232,257,275,276,257,276,258,277,278,271,277,271,270,259,109,279,
    259,279,252,252,248,247,252,247,253,252,253,259,266,261,263,266,263,265,270,258,276,270,276,277,
    275,257,271,275,271,278,109,259,253,109,253,83,269,206,180,269,180,262,278,277,276,278,276,275,
    267,270,268,272,268,270,267,268,280,267,280,254,248,249,251,250,263,264,109,89,51,109,51,48,
    109,48,187,109,187,180,109,180,206,254,280,258,279,109,280,134,256,280,252,279,254,267,266,265,
    256,134,232,256,232,272,109,134,280,279,280,254,255,280,256,262,261,269,261,266,269,272,232,268,
    268,206,269,268,232,206,258,280,255,282,281,270,282,270,267,283,282,267,283,267,265,264,283,265,
    264,265,263,293,277,291,277,276,294,277,294,270,281,270,294,295,285,284,295,284,296,296,284,297,
    296,297,298,298,297,287,298,287,299,299,287,288,299,288,300,300,288,289,300,289,301,301,289,290,
    301,290,302,302,290,7,291,295,293,285,295,291,284,286,297,286,287,297,295,276,277,295,277,293,
    306,285,304,304,277,306,277,270,281,277,281,306,293,307,277,277,307,268,277,268,270,263,292,250,
    291,277,304,291,304,285,7,290,305,22,7,305,282,254,258,282,258,281,283,252,254,283,254,282,
    264,248,252,264,252,283,311,309,255,248,249,250,248,250,264,255,258,294,255,294,276,281,294,258,
    302,7,5,309,311,295,296,298,308,308,298,299,21,22,303,295,311,255,295,255,276,21,5,7,
    21,7,22,296,308,286,296,286,284,306,276,312,306,312,285,312,276,255,255,306,281,255,281,258,
    311,255,314,255,280,314,287,286,308,287,308,299,305,290,302,305,302,313,22,305,313,22,313,21,
    248,249,310,276,306,255,309,295,312,309,312,255,285,312,295,5,313,302,21,313,5,254,280,268,
    254,268,267,316,315,309,319,318,320,319,320,322,322,319,321,325,323,324,329,327,328,329,328,109,
    328,327,109,109,327,329,314,280,259,314,259,331,333,332,315,309,333,315,334,315,332,333,334,332,
    334,316,315,133,130,325,133,325,326,326,325,130,326,130,133,320,317,318,321,322,323,130,324,323,
    130,323,325,325,324,130,330,329,109,330,109,259,259,109,329,259,329,330,259,330,331,334,333,309,
    334,309,316,334,316,318,316,317,318,
};
static const uint8_t kGrC[1005] = {
    88,85,64,90,86,67,90,87,65,88,85,65,89,85,69,118,110,99,87,84,68,112,106,96,
    91,86,68,91,89,73,91,85,69,82,79,65,88,85,66,86,83,66,84,81,65,87,84,65,
    82,79,65,92,89,69,87,84,64,85,81,67,101,97,79,142,134,131,140,131,127,92,86,68,
    83,80,66,89,85,71,95,89,73,86,82,67,104,101,81,89,85,67,93,90,69,114,109,89,
    91,88,68,90,86,65,98,95,73,89,86,66,98,94,74,101,98,77,88,85,64,92,89,68,
    99,95,74,95,92,71,86,83,65,96,94,76,94,87,66,89,84,66,91,86,67,94,87,65,
    118,113,104,88,84,64,93,87,69,116,103,91,93,90,69,91,87,66,97,94,74,87,84,65,
    88,84,69,84,80,65,94,91,73,102,98,79,85,81,66,106,102,82,95,92,72,90,86,68,
    87,84,64,90,87,65,96,92,72,86,84,64,92,88,67,93,87,70,100,96,76,94,91,72,
    90,87,65,86,83,64,117,113,93,100,94,75,94,87,66,93,87,68,93,88,68,91,86,69,
    92,86,65,91,88,65,87,84,64,116,108,99,89,86,64,112,108,88,88,86,64,89,87,65,
    89,87,66,99,90,79,93,86,69,90,87,67,83,81,66,90,86,66,90,86,65,92,89,69,
    98,94,74,92,89,68,96,93,72,90,88,67,97,93,71,96,93,72,86,84,66,92,89,71,
    89,86,64,86,83,65,87,84,65,88,85,63,87,85,67,119,111,104,86,83,62,94,91,70,
    94,91,70,88,85,64,88,85,66,84,82,67,84,81,66,95,90,73,93,89,69,86,82,66,
    95,92,71,99,94,76,88,85,66,87,85,65,90,87,66,88,84,64,87,85,63,88,84,63,
    98,95,74,90,87,71,131,121,118,92,87,67,91,85,66,119,112,107,134,132,127,88,82,68,
    90,87,66,88,85,65,90,87,66,91,88,66,100,97,76,77,76,63,89,86,63,91,88,66,
    92,89,66,91,88,65,88,85,65,90,86,68,86,83,65,84,80,65,93,90,70,86,84,65,
    92,89,69,95,92,71,93,88,68,91,87,69,90,84,67,100,96,76,83,80,65,93,91,70,
    86,83,65,88,85,65,87,84,64,93,90,68,95,92,71,86,83,63,107,103,82,90,87,69,
    110,105,86,89,86,66,89,86,64,86,83,64,123,119,99,109,105,86,89,86,64,88,85,65,
    89,87,67,86,84,66,88,85,65,91,88,66,114,106,96,98,89,73,88,85,64,110,107,87,
    89,86,65,88,86,64,86,84,63,98,95,85,88,86,66,88,86,66,83,81,65,88,84,65,
    90,87,67,89,87,65,106,102,82,94,90,69,99,91,72,93,89,67,94,90,69,91,88,70,
    88,85,67,96,93,72,91,86,64,93,86,65,90,86,67,87,83,64,113,106,97,89,84,65,
    95,88,68,93,89,68,97,94,73,91,88,67,89,85,66,84,81,66,85,82,67,90,88,69,
    107,104,84,84,81,66,100,96,75,90,86,68,95,90,70,87,84,65,92,90,69,86,84,64,
    89,87,66,87,84,63,104,100,79,85,83,64,90,86,64,89,85,65,91,86,65,89,85,65,
    131,124,118,88,84,65,92,90,68,89,86,65,92,89,67,88,86,64,97,94,73,78,76,63,
    95,92,70,108,104,83,89,86,64,90,88,66,139,136,135,140,135,133,148,143,142,160,155,154,
    136,126,124,132,122,119,128,120,117,124,118,114,135,123,120,137,129,127,126,118,115,125,117,114,
    145,138,137,131,120,118,126,116,113,139,131,129,149,146,145,149,145,145,128,122,121,130,124,122,
    136,133,132,129,122,120,147,146,145,131,122,120,127,115,112,140,133,131,124,118,116,138,131,129,
    152,148,146,134,126,125,138,127,125,109,108,106,128,126,124,126,115,112,116,115,114,152,151,151,
    122,114,111,140,137,136,139,138,138,136,135,135,147,139,137,149,144,143,143,136,133,142,132,128,
    146,132,128,143,134,131,148,139,136,145,136,134,140,132,130,141,132,130,112,111,111,140,133,131,
    144,136,134,147,138,136,150,146,144,144,138,135,140,134,131,140,129,126,146,137,135,138,116,110,
    147,138,136,148,139,137,150,149,149,129,116,112,136,129,126,140,132,130,139,122,118,132,117,113,
    145,139,138,140,133,131,132,119,116,135,129,127,143,135,133,146,137,134,142,135,134,138,128,126,
    149,139,137,141,132,129,154,144,141,144,135,133,144,133,131,153,143,140,138,133,131,139,129,126,
    120,107,103,149,141,139,152,144,142,162,154,152,150,146,145,152,145,144,154,146,145,
};
static const int16_t kGrHV[2457] = {
    -648,-8729,1953,-592,-7287,3203,-1420,-7263,2903,-937,-8249,2196,463,-8757,1950,395,-7286,3225,-1386,-1864,5065,-1283,-3494,4780,
    -605,-1877,5253,-574,-3499,4943,303,-1877,5273,287,-3499,4962,-1366,1277,5060,-1393,-636,5142,-619,1369,5139,-618,-639,5321,
    310,1369,5159,309,-639,5341,-1163,3706,4593,-1279,2425,4873,-600,2330,4987,-528,3884,4621,300,2330,5007,264,3884,4638,
    -513,7747,2148,-1119,4146,3928,-468,4312,3885,234,4312,3901,385,7747,2166,-1006,-6122,3839,-666,-6137,3927,-1190,-4684,4449,
    -538,-4683,4598,444,-6136,3951,269,-4683,4615,-466,8234,1937,333,8237,1953,-621,-6810,3214,414,-6810,3237,-1346,-6750,2977,
    283,-4100,4370,-565,-4100,4351,-1536,-4081,4092,-617,-1396,4925,308,-1396,4945,-1283,-1481,4859,-1747,-1269,4469,-1726,1723,4484,
    -2011,1671,4356,-2041,-1196,4340,-1403,4267,3388,-1712,1845,4477,395,-7286,-3225,463,-8757,-1951,-648,-8729,-1954,-592,-7287,-3204,
    -937,-8248,-2197,-1419,-7263,-2904,-605,-1877,-5254,303,-1877,-5274,287,-3499,-4962,-574,-3499,-4943,-1386,-1864,-5066,-1283,-3494,-4781,
    -619,1369,-5139,310,1369,-5159,309,-639,-5342,-618,-639,-5322,-1366,1277,-5060,-1393,-636,-5142,264,3884,-4639,300,2330,-5007,
    -601,2330,-4988,-528,3884,-4622,-1279,2425,-4873,-1163,3706,-4594,385,7747,-2167,234,4312,-3901,-468,4312,-3886,-513,7747,-2148,
    -1120,4142,-3931,444,-6136,-3952,-666,-6137,-3928,-538,-4683,-4598,269,-4683,-4616,-1190,-4684,-4449,-1006,-6122,-3840,341,8474,-1916,
    -472,8471,-1899,-1346,-6750,-2977,-621,-6810,-3215,414,-6810,-3237,-565,-4100,-4352,283,-4100,-4371,-1536,-4081,-4093,-1747,-1269,-4470,
    -1283,-1481,-4860,-617,-1396,-4925,308,-1396,-4945,-2041,-1196,-4341,-2011,1671,-4357,-1726,1723,-4484,-1712,1845,-4477,-1405,4258,-3393,
    -2299,-7287,-2298,-1298,-8873,-1328,-2036,-8480,-997,-2949,-7260,-1326,-2053,-8648,-579,-4313,-1877,-3036,-3743,-1877,-3743,-3522,-3498,-3522,
    -3902,-3499,-3090,-4287,-3494,-2474,-4601,-1857,-2510,-3036,-1877,-4313,-2510,-1857,-4601,-2473,-3494,-4288,-3089,-3499,-3902,-4071,1369,-3196,
    -3791,1189,-3792,-3791,-639,-3792,-4200,-639,-3326,-4546,1192,-2812,-4546,-642,-2812,-4671,1182,-2543,-4671,-632,-2543,-2812,1192,-4547,
    -2543,1182,-4671,-2543,-632,-4671,-2812,-642,-4547,-3196,1369,-4072,-3326,-639,-4200,-3081,4077,-3081,-3635,2575,-3635,-4053,2409,-3003,
    -3734,3732,-2973,-3972,3858,-2360,-4361,2198,-2297,-2360,3858,-3972,-2296,2198,-4361,-3003,2409,-4053,-2972,3732,-3735,-1563,7748,-1563,
    -3279,4297,-2103,-1929,7717,-1118,-1118,7717,-1929,-2102,4298,-3279,-2828,-6135,-2828,-3324,-6131,-2177,-3632,-4683,-2871,-3276,-4683,-3276,
    -3987,-4684,-2305,-2177,-6131,-3325,-2305,-4684,-3987,-2871,-4683,-3632,-1352,8479,-1352,-2166,7747,-385,-1934,8477,-314,-2977,-6750,-1346,
    -2308,-6810,-2308,-3477,-4100,-2678,-3103,-4100,-3104,-2677,-4100,-3477,-2075,-4119,-3856,-4092,-4081,-1536,-3856,-4119,-2075,-4341,-1196,-2042,
    -4057,-1484,-2975,-4585,-1738,-2519,-3727,-1748,-3727,-3403,-1220,-3403,-2975,-1484,-4057,-2519,-1738,-4585,-4357,1671,-2011,-3404,1730,-3405,
    -3394,1841,-3394,-3396,4254,-1407,-3226,-7289,-395,-2042,-8641,675,-3205,-7290,592,-2949,-7260,1325,-5254,-1877,605,-5273,-1877,-303,
    -4962,-3499,-287,-4943,-3499,574,-5065,-1864,1386,-4781,-3494,1282,-5065,-1864,-1386,-4781,-3494,-1283,-5139,1369,619,-5159,1369,-310,
    -5342,-639,-309,-5321,-639,618,-5060,1277,1366,-5142,-636,1393,-5060,1277,-1366,-5142,-636,-1393,-4639,3884,-264,-5007,2330,-300,
    -4987,2330,600,-4621,3884,527,-4873,2425,1278,-4594,3706,1163,-4594,3706,-1164,-4873,2425,-1279,-3901,4312,-234,-3886,4312,468,
    -2148,7747,513,-3930,4143,1119,-3932,4140,-1120,-3952,-6136,-444,-3928,-6137,666,-4598,-4683,538,-4615,-4683,-269,-4449,-4684,1189,
    -3839,-6122,1006,-3839,-6122,-1006,-4449,-4684,-1190,-2072,8486,577,-2977,-6750,1346,-3215,-6810,621,-3237,-6810,-414,-4352,-4100,565,
    -4370,-4100,-283,-4092,-4081,1536,-4470,-1269,1747,-4859,-1481,1283,-4925,-1396,617,-4945,-1396,-309,-4860,-1481,-1283,-4470,-1269,-1747,
    -4341,-1196,2041,-4357,1671,2011,-4484,1723,1726,-4484,1723,-1726,-4477,1845,1712,-3393,4258,1405,-4477,1845,-1713,-2299,-7287,2298,
    -1298,-8873,1328,-2036,-8480,996,-3036,-1877,4313,-3743,-1877,3743,-3522,-3498,3522,-3089,-3499,3901,-2473,-3494,4287,-2510,-1857,4601,
    -4313,-1877,3036,-4601,-1857,2510,-4287,-3494,2473,-3902,-3499,3089,-3196,1369,4071,-3791,1189,3791,-3791,-639,3791,-3326,-639,4200,
    -2812,1192,4546,-2812,-642,4546,-2543,1182,4670,-2543,-632,4670,-4546,1192,2812,-4671,1182,2543,-4671,-632,2543,-4546,-642,2812,
    -4071,1369,3196,-4200,-639,3326,-3278,4160,3160,-3635,2575,3635,-3003,2409,4052,-3315,4117,4025,-2359,3858,3972,-2296,2198,4361,
    -4141,4266,2243,-4361,2198,2296,-4053,2409,3003,-3769,4232,3225,-1563,7748,1563,-2074,4281,3309,-1118,7717,1929,-1929,7717,1118,
    -3350,4319,2222,-2828,-6135,2828,-2177,-6131,3324,-2871,-4683,3631,-3276,-4683,3276,-2305,-4684,3987,-3324,-6131,2176,-3987,-4684,2304,
    -3632,-4683,2870,-1385,8254,1338,-2308,-6810,2308,-2677,-4100,3477,-3103,-4100,3103,-3477,-4100,2677,-3856,-4119,2074,-2075,-4119,3855,
    -2975,-1484,4056,-2519,-1738,4584,-3727,-1748,3727,-3403,-1220,3403,-4057,-1484,2974,-4585,-1738,2518,-3404,1730,3404,-3394,1841,3394,
    -428,-9002,1087,-831,-9002,789,-1016,-9002,420,-428,-9002,-1087,-1033,-9002,-336,-831,-9002,-790,367,-9002,-1105,367,-9002,1105,
    937,-8249,2196,1419,-7263,2903,1386,-1864,5065,1283,-3494,4780,1366,1277,5060,1393,-636,5142,1163,3706,4593,1279,2425,4873,
    1119,4146,3928,1006,-6122,3839,1190,-4684,4449,1346,-6750,2977,1536,-4081,4092,1283,-1481,4859,1747,-1269,4469,1726,1723,4484,
    2041,-1196,4340,2011,1671,4356,1403,4267,3388,1712,1845,4477,1420,-7263,-2904,937,-8248,-2197,1386,-1864,-5066,1283,-3494,-4781,
    1366,1277,-5060,1393,-636,-5142,1163,3706,-4594,1279,2425,-4873,1120,4142,-3931,1190,-4684,-4449,1006,-6122,-3840,1346,-6750,-2977,
    1536,-4081,-4093,1747,-1269,-4470,1283,-1481,-4860,2011,1671,-4357,2041,-1196,-4341,1726,1723,-4484,1712,1845,-4477,1405,4258,-3393,
    2299,-7287,-2298,1298,-8873,-1328,2949,-7260,-1326,2036,-8480,-997,2053,-8648,-579,4313,-1877,-3036,3902,-3499,-3090,3522,-3498,-3522,
    3743,-1877,-3743,4287,-3494,-2474,4601,-1857,-2510,3036,-1877,-4313,2473,-3494,-4288,2510,-1857,-4601,3089,-3499,-3902,4071,1369,-3196,
    4200,-639,-3326,3791,-639,-3792,3791,1189,-3792,4546,1192,-2812,4546,-642,-2812,4671,1182,-2543,4671,-632,-2543,2812,1192,-4547,
    2812,-642,-4547,2543,-632,-4671,2543,1182,-4671,3196,1369,-4072,3326,-639,-4200,3081,4077,-3081,3734,3732,-2973,4053,2409,-3003,
    3635,2575,-3635,3972,3858,-2360,4361,2198,-2297,2360,3858,-3972,3003,2409,-4053,2296,2198,-4361,2972,3732,-3735,1563,7748,-1563,
    3279,4297,-2103,1929,7717,-1118,1118,7717,-1929,2102,4298,-3279,2828,-6135,-2828,3276,-4683,-3276,3632,-4683,-2871,3324,-6131,-2177,
    3987,-4684,-2305,2177,-6131,-3325,2305,-4684,-3987,2871,-4683,-3632,1352,8479,-1352,2166,7747,-385,1934,8477,-314,2977,-6750,-1346,
    2308,-6810,-2308,3103,-4100,-3104,3477,-4100,-2678,2677,-4100,-3477,2075,-4119,-3856,4092,-4081,-1536,3856,-4119,-2075,4341,-1196,-2042,
    4057,-1484,-2975,4585,-1738,-2519,3727,-1748,-3727,3403,-1220,-3403,2975,-1484,-4057,2519,-1738,-4585,4357,1671,-2011,3404,1730,-3405,
    3394,1841,-3394,3396,4254,-1407,3226,-7289,-395,3205,-7290,592,2042,-8641,675,2949,-7260,1325,5254,-1877,605,4943,-3499,574,
    4962,-3499,-287,5273,-1877,-303,5065,-1864,1386,4781,-3494,1282,5065,-1864,-1386,4781,-3494,-1283,5139,1369,619,5321,-639,618,
    5342,-639,-309,5159,1369,-310,5060,1277,1366,5142,-636,1393,5060,1277,-1366,5142,-636,-1393,4639,3884,-264,4621,3884,527,
    4987,2330,600,5007,2330,-300,4594,3706,1163,4873,2425,1278,4594,3706,-1164,4873,2425,-1279,2148,7747,513,3886,4312,468,
    3901,4312,-234,3930,4143,1119,3932,4140,-1120,3952,-6136,-444,4615,-4683,-269,4598,-4683,538,3928,-6137,666,4449,-4684,1189,
    3839,-6122,1006,3839,-6122,-1006,4449,-4684,-1190,2013,8450,580,2977,-6750,1346,3215,-6810,621,3237,-6810,-414,4370,-4100,-283,
    4352,-4100,565,4092,-4081,1536,4470,-1269,1747,4860,-1481,1283,4925,-1396,617,4945,-1396,-309,4859,-1481,-1283,4470,-1269,-1747,
    4357,1671,2011,4341,-1196,2041,4484,1723,1726,4484,1723,-1726,4477,1845,1712,3393,4258,1405,4477,1845,-1713,2299,-7287,2298,
    1298,-8873,1328,2036,-8480,996,3045,-1876,4326,3098,-3519,3912,3531,-3518,3531,3756,-1876,3756,2475,-3502,4293,2510,-1857,4601,
    4313,-1877,3036,4287,-3494,2473,4601,-1857,2510,3908,-3511,3094,3196,1369,4071,3330,-646,4205,3791,-639,3791,3791,1189,3791,
    2812,1192,4546,2812,-642,4546,2543,1182,4670,2543,-632,4670,4546,1192,2812,4546,-642,2812,4671,-632,2543,4671,1182,2543,
    4071,1369,3196,4200,-639,3326,3081,4077,3081,2972,3732,3734,3003,2409,4052,3635,2575,3635,2359,3858,3972,2296,2198,4361,
    3972,3858,2359,4053,2409,3003,4361,2198,2296,3734,3732,2972,1563,7748,1563,2102,4297,3279,1118,7717,1929,1929,7717,1118,
    3279,4298,2102,2828,-6135,2828,3276,-4683,3276,2871,-4683,3631,2177,-6131,3324,2305,-4684,3987,3324,-6131,2176,3987,-4684,2304,
    3632,-4683,2870,1347,8248,1347,2308,-6810,2308,3103,-4100,3103,2677,-4100,3477,3477,-4100,2677,3856,-4119,2074,2075,-4119,3855,
    2977,-1477,4060,2519,-1738,4584,3734,-1733,3734,3403,-1220,3403,4057,-1484,2974,4585,-1738,2518,3404,1730,3404,3394,1841,3394,
    831,-9002,-790,1033,-9002,-336,1016,-9002,420,831,-9002,789,-534,-8972,1650,-1632,-8978,523,-1655,-8977,-407,-534,-8972,-1651,
    475,-8975,-1656,1655,-8977,-407,1632,-8978,523,475,-8975,1656,-1466,8425,-524,-1336,8295,553,-553,8295,1336,474,8295,1341,
    1336,8295,553,1466,8425,-524,529,8388,-1429,-595,8400,-1437,307,11531,-1898,-430,11537,-1885,-714,11394,-1724,-1289,11513,-1364,
    -1750,11488,-632,-1830,11396,-213,-1761,12992,-539,-1744,11995,-723,-1955,12203,-415,-1940,12912,-451,-535,12036,-1952,327,12038,-2098,
    -1373,12022,-1378,-2009,12871,1416,-1841,12042,1239,-1933,10111,-29,-1877,12129,2308,-2039,12943,2463,-1795,11073,3675,-1739,8401,3692,
    -1451,8444,4820,-1929,12199,3676,-1898,8566,1238,-2061,10076,536,714,11394,-1724,1289,11513,-1364,1750,11486,-632,1907,10073,-64,
    1830,11391,-213,1744,11995,-723,1761,12992,-539,1940,12912,-451,1955,12203,-415,1373,12022,-1378,2009,12871,1416,1841,12052,1209,
    1834,12133,2292,1843,9694,1133,1845,11133,970,2039,12943,2463,1965,12842,3023,1900,12260,3165,1926,12199,3676,1455,8442,4820,
    1807,8399,3697,1892,8549,1240,2024,10068,575,1770,8319,3700,1698,13002,-900,-1698,13002,-900,1725,11976,2878,-1794,12436,3528,
    -1724,12112,3712,1746,12114,3712,1792,12767,3174,-1792,12767,3174,1791,12952,2670,-1791,12952,2670,-1795,12970,1239,1795,12970,1239,
    1428,12904,-3085,-1428,12904,-3085,-1420,12339,-3178,1420,12339,-3178,-1768,8325,2039,1791,8304,2044,-1313,9153,4146,-1476,10653,3726,
    1580,10623,3688,1406,9074,4134,-1844,10857,1240,-1895,12113,2987,-1965,12842,3023,-1770,8395,2858,1819,8406,2866,1781,8311,2854,
    2096,11149,3311,1931,11328,2440,-1961,11396,2364,-2200,11275,3225,1830,8412,2070,-1814,8421,2068,0,12923,-3096,0,12339,-3223,
    1461,12063,-2534,0,13095,-416,0,13092,-901,0,12957,3181,0,13118,1446,0,12406,3745,1362,7317,5672,1493,9550,4881,
    300,9616,4990,292,7368,5769,1334,5852,5976,293,5879,6121,1281,3806,6352,297,3795,6403,1231,1971,6444,293,1950,6546,
    1189,244,6470,296,209,6523,1150,-1308,6338,297,-1352,6391,1093,-4127,5952,257,-4175,6004,1073,-4942,5823,213,-5076,5861,
    0,13177,2456,349,10456,4623,1887,10160,4624,1891,13036,-900,2102,11124,3858,1649,12963,-2468,1877,12616,-1821,1957,12428,-1253,
    2087,12393,463,297,-5870,5671,1000,-5748,5689,2178,10529,4027,-13,11383,4104,-13,12283,3722,-303,9579,4962,1321,7174,5535,
    -13,7169,5571,-295,5867,6075,1287,3768,6137,-13,3765,6185,1241,1963,6284,-295,1956,6504,1205,251,6283,-298,221,6484,
    1167,-1292,6184,-13,-1288,6231,-389,10458,4617,1844,12596,-1803,-276,10655,4484,2093,10792,2628,2179,10485,3164,0,11249,4250,
    407,10681,4495,-1461,12063,-2534,-1367,7295,5642,-351,7369,5764,-1503,9551,4880,-1336,5852,5977,-1279,3806,6352,-357,3798,6396,
    -1243,1973,6489,-1189,244,6470,-1148,-1308,6339,-356,-1344,6384,-1093,-4127,5952,-360,-4178,6013,-1073,-4942,5823,-355,-5029,5877,
    -2163,10531,4971,-1891,13036,-900,-2236,11173,3878,-1649,12963,-2468,-1877,12616,-1821,-1957,12428,-1253,-2087,12393,463,-841,-5951,5662,
    -1000,-5748,5689,-2247,10559,4013,-1298,3766,6136,-1247,1967,6282,-1202,251,6275,-1176,-1292,6186,-1844,12596,-1803,-2388,10616,2321,
    -2228,10517,3174,1715,11570,2408,1715,11994,2349,1709,11609,2705,-2260,10926,4975,-2301,10337,5805,-2351,9688,6132,-2372,9412,6682,
    -2409,9992,6410,-3041,9391,6705,-2429,8667,6845,-3142,8526,6954,-3155,8455,7301,-2555,8469,7296,-3131,7683,7338,-3150,7656,6916,
    -2530,7802,6978,-2551,7668,7303,-3148,6732,6903,-3143,6948,7312,-2645,6903,7066,-3225,5915,6680,-3267,5256,6380,-3235,5635,6164,
    -3290,4977,5827,-3946,5010,5763,-4013,4768,4999,-3970,4375,4849,-3340,4795,5051,-3328,4302,4885,-4041,4665,4167,-3372,4585,4180,
    -4005,4292,4067,-3688,4593,3180,-3401,4595,3261,-3881,4726,2208,-3338,4799,2184,-3686,5104,1458,-3315,5101,1404,-3300,5749,1213,
    -3662,5602,1108,-3286,5942,644,-3606,6004,815,-3215,6763,476,-3276,6380,921,-2994,8495,452,-2943,9380,671,-2966,9526,1128,
    -2593,9590,1131,-2614,9394,719,-2909,10234,1414,-2523,10257,1478,-2862,10482,2188,-2864,10926,2347,-2925,11156,3391,-2794,10702,3311,
    -2807,10722,3781,-2917,11145,3826,-2918,10999,4937,-2896,10582,4972,-2983,10311,5876,-2848,10812,5633,-2986,10111,6426,-2999,9678,6182,
    -3666,7590,7285,-3705,7505,7012,-3825,6636,6866,-3757,6888,7259,-3914,5892,6668,-3846,5340,6397,-3873,5667,6100,2940,10318,3790,
    2950,10310,3492,2816,11387,3744,2820,11390,3452,
};
static const uint16_t kGrHT[5343] = {
    0,1,2,2,3,0,1,0,4,1,4,5,8,6,7,8,7,9,10,8,9,10,9,11,
    14,12,13,14,13,15,16,14,15,16,15,17,18,19,20,18,20,21,21,20,22,21,22,23,
    24,25,26,24,26,27,24,27,28,29,30,31,30,32,31,30,33,34,30,34,32,35,24,28,
    35,28,36,39,3,2,32,34,40,32,40,41,31,32,41,31,41,42,29,31,42,29,42,39,
    30,29,39,30,39,37,33,30,37,33,37,38,7,6,46,7,46,42,9,7,42,9,42,41,
    11,9,41,11,41,40,13,12,48,13,48,49,13,46,45,15,13,45,15,45,43,17,15,43,
    17,43,44,24,50,25,25,21,26,21,23,27,21,27,26,25,18,21,52,53,54,52,54,55,
    56,57,55,56,55,54,58,59,60,58,60,61,62,58,61,62,61,63,64,65,66,64,66,67,
    68,64,67,68,67,69,70,71,72,70,72,73,73,72,74,73,74,75,76,77,78,76,78,79,
    79,78,80,81,82,83,81,83,84,82,85,83,82,86,85,87,76,79,87,79,88,56,89,57,
    84,83,92,84,92,93,82,81,91,82,91,90,86,82,90,86,90,89,85,86,89,85,89,94,
    83,85,94,83,94,92,61,60,93,61,93,92,63,61,92,63,92,94,62,63,94,62,94,95,
    67,66,98,67,98,97,69,67,97,69,97,96,69,96,95,68,69,99,68,99,100,79,80,103,
    70,73,78,70,78,77,73,80,78,73,75,80,104,105,106,104,106,107,107,106,108,57,54,105,
    57,105,104,109,110,111,109,111,112,109,112,113,114,109,113,115,116,117,115,117,118,110,115,118,
    110,118,111,119,120,121,119,121,122,123,119,122,123,122,124,125,123,124,125,124,126,127,128,129,
    127,129,130,131,127,130,131,130,132,120,131,132,120,132,121,133,134,135,133,135,136,136,135,137,
    137,135,138,139,140,141,139,141,142,142,141,134,142,134,133,143,133,144,143,144,145,143,146,147,
    143,147,133,148,149,150,148,150,151,149,152,150,153,155,154,153,148,151,153,151,155,156,143,145,
    156,145,157,156,157,158,88,79,146,88,146,156,156,146,143,106,159,107,151,150,161,151,161,162,
    155,151,162,155,162,163,154,155,163,154,163,164,153,154,94,153,94,89,153,89,160,148,153,160,
    149,148,160,149,160,159,152,149,159,152,159,165,150,152,166,150,166,161,112,111,162,112,162,161,
    113,112,161,113,161,166,113,166,165,114,113,165,114,165,167,168,169,167,170,168,171,172,170,171,
    173,172,99,117,116,99,117,99,94,117,94,164,118,117,164,118,164,163,111,118,163,111,163,162,
    122,121,171,122,171,168,124,122,168,124,168,167,126,124,167,125,126,167,125,167,174,123,125,174,
    119,123,174,120,119,175,131,120,175,127,131,100,128,127,100,129,128,100,129,100,99,130,129,99,
    132,130,99,132,99,172,121,132,172,121,172,171,135,134,176,144,137,177,145,144,177,145,177,157,
    146,79,103,146,103,147,134,141,176,133,137,144,139,133,147,133,136,137,139,142,133,178,108,179,
    178,179,180,180,179,181,107,108,178,182,183,184,182,184,185,186,182,185,186,185,187,183,188,189,
    183,189,184,190,191,192,190,192,193,194,190,193,194,193,195,191,196,197,191,197,192,198,199,200,
    198,200,201,201,200,202,201,202,203,204,205,199,204,199,198,157,206,207,157,207,208,208,207,209,
    157,210,206,211,212,213,211,213,214,212,215,213,212,216,215,217,211,218,211,214,218,158,157,208,
    158,208,219,179,220,181,159,108,107,214,213,223,214,223,224,218,214,224,218,224,165,217,218,165,
    217,165,159,211,217,159,211,159,222,212,211,222,212,222,221,216,212,221,216,221,220,215,216,220,
    215,220,225,213,215,225,213,225,223,185,184,224,185,224,223,187,185,223,187,223,225,186,187,225,
    186,225,226,189,188,231,189,231,165,184,189,165,184,165,224,193,192,229,193,229,228,195,193,228,
    195,228,227,195,227,226,194,195,232,194,232,233,197,196,174,197,174,167,197,231,230,192,197,230,
    192,230,229,208,209,237,157,177,210,198,201,207,198,207,206,201,209,207,210,198,206,201,203,209,
    210,204,198,239,240,2,2,240,0,181,179,241,181,241,240,181,240,239,242,243,244,242,244,245,
    242,245,246,247,242,246,248,249,250,248,250,251,243,248,251,243,251,244,252,253,254,252,254,255,
    256,252,255,256,255,257,258,256,257,258,257,259,260,261,262,260,262,263,264,260,263,264,263,265,
    253,264,265,253,265,254,266,267,268,266,268,269,269,268,270,270,268,271,272,273,274,272,274,275,
    275,274,267,275,267,266,276,266,277,276,277,278,276,279,280,276,280,266,281,282,283,281,283,284,
    282,285,283,286,288,287,286,281,284,286,284,288,289,276,278,289,278,24,289,24,35,219,208,279,
    219,279,289,289,279,276,220,241,181,284,283,291,284,291,292,288,284,292,288,292,293,287,288,293,
    287,293,294,286,287,225,286,225,220,286,220,290,281,286,290,282,281,290,282,290,39,285,282,39,
    285,39,42,283,285,295,283,295,291,245,244,292,245,292,291,246,245,291,246,291,295,246,295,42,
    247,246,42,247,42,49,296,297,49,298,296,299,300,298,299,301,300,232,250,249,232,250,232,225,
    250,225,294,251,250,294,251,294,293,244,251,293,244,293,292,255,254,299,255,299,296,257,255,296,
    257,296,49,259,257,49,258,259,49,258,49,48,256,258,48,252,256,48,253,252,302,264,253,302,
    260,264,233,261,260,233,262,261,233,262,233,232,263,262,232,265,263,232,265,232,300,254,265,300,
    254,300,299,268,267,303,270,50,277,278,277,50,278,50,24,279,208,237,279,237,280,267,274,303,
    266,270,277,272,266,280,266,269,270,272,275,266,103,139,147,237,272,280,203,202,273,203,273,209,
    273,272,209,272,237,209,203,209,237,137,138,210,138,205,204,138,204,210,137,210,177,75,74,140,
    75,140,80,140,139,80,139,103,80,75,80,103,270,271,25,271,19,18,271,18,25,270,25,50,
    304,305,306,304,306,307,306,308,309,306,309,307,304,307,310,304,310,311,4,312,313,4,313,5,
    10,11,315,10,315,314,16,17,317,16,317,316,318,23,22,318,22,319,28,27,320,321,322,33,
    33,322,34,323,313,312,322,324,40,322,40,34,321,323,324,321,324,322,33,38,323,33,323,321,
    315,324,326,315,326,314,11,40,324,11,324,315,317,328,329,317,329,316,317,325,326,17,44,325,
    17,325,317,28,320,330,320,27,23,320,23,318,53,52,332,333,53,332,334,335,60,334,60,59,
    336,337,66,336,66,65,70,338,339,70,339,71,76,340,77,81,84,341,81,341,342,333,332,343,
    342,343,91,342,91,81,341,344,343,341,343,342,84,93,344,84,344,341,335,344,93,335,93,60,
    334,345,344,334,344,335,337,346,98,337,98,66,337,345,346,336,347,348,336,348,337,76,351,340,
    70,77,340,70,340,338,352,354,355,352,355,353,354,356,355,332,353,53,332,352,353,357,358,359,
    357,359,360,357,361,358,362,361,357,363,364,365,363,366,364,360,359,366,360,366,363,367,368,369,
    367,369,370,371,372,368,371,368,367,373,374,372,373,372,371,375,376,377,375,377,378,379,380,376,
    379,376,375,370,369,380,370,380,379,381,382,383,381,383,384,382,385,383,385,386,383,387,388,389,
    387,390,388,390,381,384,390,384,388,391,392,381,391,393,392,391,395,394,391,381,395,396,397,398,
    396,398,399,399,398,400,401,402,403,401,403,397,401,397,396,393,391,404,405,393,404,405,404,406,
    394,76,87,394,87,404,391,394,404,355,354,407,397,409,410,397,410,398,403,411,409,403,409,397,
    402,412,411,402,411,403,401,343,344,401,344,402,401,408,343,396,408,401,399,408,396,399,407,408,
    400,413,407,400,407,399,398,410,414,398,414,400,358,410,409,358,409,359,361,414,410,361,410,358,
    361,413,414,362,415,413,362,413,361,416,415,417,418,419,416,420,419,418,421,348,420,364,344,348,
    364,348,365,364,412,344,366,411,412,366,412,364,359,409,411,359,411,366,368,416,419,368,419,369,
    372,415,416,372,416,368,374,415,372,373,422,415,373,415,374,371,422,373,367,422,371,370,423,367,
    379,423,370,375,347,379,378,347,375,377,348,347,377,347,378,376,348,377,380,420,348,380,348,376,
    369,419,420,369,420,380,383,424,384,392,425,385,393,405,425,393,425,392,394,395,351,394,351,76,
    384,424,388,381,392,385,387,395,381,381,385,382,387,381,390,426,427,428,426,428,356,427,429,428,
    354,426,356,430,431,432,430,432,433,434,435,431,434,431,430,433,432,437,433,437,436,438,439,440,
    438,440,441,442,443,439,442,439,438,441,440,445,441,445,444,446,447,448,446,448,449,447,450,451,
    447,451,448,452,446,449,452,449,453,405,454,455,405,455,456,454,457,455,405,456,458,459,460,461,
    459,461,462,462,461,463,462,463,464,465,466,459,459,466,460,454,405,406,454,406,467,428,429,468,
    407,354,356,460,471,472,460,472,461,466,413,471,466,471,460,465,407,413,465,413,466,459,470,407,
    459,407,465,462,469,470,462,470,459,464,468,469,464,469,462,463,473,468,463,468,464,461,472,473,
    461,473,463,431,472,471,431,471,432,435,473,472,435,472,431,434,474,473,434,473,435,437,413,479,
    437,479,436,432,471,413,432,413,437,439,476,477,439,477,440,443,475,476,443,476,439,443,474,475,
    442,480,481,442,481,443,445,415,422,445,422,444,445,478,479,440,477,478,440,478,445,454,485,457,
    405,458,425,446,456,455,446,455,447,447,455,457,458,456,446,447,457,450,458,446,452,487,313,488,
    313,4,488,429,489,428,429,487,488,429,488,489,490,491,492,490,492,493,490,494,491,495,494,490,
    496,497,498,496,499,497,493,492,499,493,499,496,500,501,502,500,502,503,504,505,501,504,501,500,
    506,507,505,506,505,504,508,509,510,508,510,511,512,513,509,512,509,508,503,502,513,503,513,512,
    514,515,516,514,516,517,515,518,516,518,519,516,520,521,522,520,523,521,523,514,517,523,517,521,
    524,525,514,524,526,525,524,528,527,524,514,528,529,530,531,529,531,532,532,531,533,534,535,536,
    534,536,530,534,530,529,526,524,537,28,526,537,28,537,36,527,454,467,527,467,537,524,527,537,
    468,429,489,530,539,540,530,540,531,536,541,539,536,539,530,535,542,541,535,541,536,534,468,473,
    534,473,535,534,538,468,529,538,534,532,538,529,532,323,538,533,324,323,533,323,532,531,540,543,
    531,543,533,491,540,539,491,539,492,494,543,540,494,540,491,494,324,543,495,328,324,495,324,494,
    544,328,545,546,547,544,548,547,546,549,481,548,497,473,481,497,481,498,497,542,473,499,541,542,
    499,542,497,492,539,541,492,541,499,501,544,547,501,547,502,505,328,544,505,544,501,507,328,505,
    506,329,328,506,328,507,504,329,506,500,329,504,503,550,500,512,550,503,508,480,512,511,480,508,
    510,481,480,510,480,511,509,481,510,513,548,481,513,481,509,502,547,548,502,548,513,516,551,517,
    525,330,518,526,28,330,526,330,525,527,528,485,527,485,454,517,551,521,514,525,518,520,528,514,
    514,518,515,520,514,523,351,395,387,485,528,520,450,457,522,450,522,451,522,457,520,520,457,485,
    450,457,485,385,458,386,386,458,452,386,452,453,385,425,458,338,340,389,338,389,339,389,340,387,
    387,340,351,338,340,351,518,320,519,519,320,318,519,318,319,518,330,320,552,553,554,552,554,310,
    554,555,311,554,311,310,0,240,556,241,179,557,241,557,240,179,108,558,179,558,557,108,106,105,
    108,105,558,105,54,559,54,53,559,53,353,560,355,356,561,355,561,353,356,428,562,356,562,561,
    428,489,488,428,488,562,488,4,563,4,0,556,565,289,566,565,566,567,565,567,564,567,537,568,
    567,568,569,569,404,570,569,570,571,569,571,564,571,156,564,567,569,564,240,304,556,240,305,304,
    557,306,305,557,305,240,558,308,306,558,306,557,105,309,308,105,308,558,105,307,309,559,307,105,
    560,310,307,560,307,559,353,310,560,353,552,310,561,553,552,561,552,353,562,554,553,562,553,561,
    488,555,554,488,554,562,488,311,555,563,311,488,556,304,311,556,311,563,219,565,564,219,564,158,
    36,567,566,36,566,35,406,569,568,406,568,467,88,571,570,88,570,87,174,235,238,174,238,138,
    101,100,140,101,140,102,233,273,236,233,236,234,51,271,48,51,48,47,329,519,331,329,331,327,
    349,350,389,349,389,347,422,386,486,422,486,483,482,484,522,482,522,480,1,37,39,1,39,2,
    5,38,37,5,37,1,8,43,45,8,45,6,10,44,43,10,43,8,46,6,45,16,22,20,
    16,20,14,14,20,19,14,19,12,12,19,51,12,51,47,47,48,12,49,46,13,271,51,19,
    565,219,289,55,90,91,55,91,52,57,89,90,57,90,55,58,97,98,58,98,59,62,96,97,
    62,97,58,95,96,62,95,99,69,100,101,68,101,102,74,101,74,68,68,74,72,68,72,64,
    64,72,71,64,71,65,102,140,74,107,159,160,107,160,104,57,56,54,104,160,89,104,89,57,
    109,168,170,109,170,110,114,169,168,114,168,109,115,172,173,115,173,116,110,170,172,110,172,115,
    156,158,564,571,88,156,106,108,159,164,94,154,165,166,152,165,231,167,167,169,114,99,116,173,
    99,95,94,174,138,135,174,135,119,119,135,176,119,176,175,175,176,141,175,141,131,131,141,140,
    131,140,100,180,221,222,180,222,178,181,220,221,181,221,180,178,222,159,178,159,107,182,228,229,
    182,229,183,186,227,228,186,228,182,183,229,230,183,230,188,179,241,220,225,232,226,226,227,186,
    231,188,230,226,232,195,233,234,194,234,236,202,234,202,194,194,202,200,194,200,190,190,200,199,
    190,199,191,191,199,205,191,205,196,196,205,238,196,238,235,235,174,196,167,231,197,236,273,202,
    138,238,205,2,39,290,2,290,239,239,290,220,239,220,181,242,296,298,242,298,243,247,297,296,
    247,296,242,248,300,301,248,301,249,243,298,300,243,300,248,289,35,566,294,225,287,42,295,285,
    42,46,49,49,297,247,232,249,301,48,271,268,48,268,252,252,268,303,252,303,302,302,303,274,
    302,274,264,264,274,273,264,273,233,313,323,38,313,38,5,314,325,44,314,44,10,325,314,326,
    328,326,324,316,319,22,316,22,16,327,331,319,327,319,316,316,329,327,317,326,328,319,331,519,
    52,91,343,52,343,332,59,98,346,59,346,334,344,345,348,334,346,345,337,348,345,336,349,347,
    336,339,350,336,350,349,65,71,339,65,339,336,339,389,350,352,408,407,352,407,354,332,343,408,
    332,408,352,360,418,416,360,416,357,357,416,417,357,417,362,365,421,420,365,420,363,363,420,418,
    363,418,360,569,406,404,404,87,570,407,356,355,402,344,412,400,414,413,415,479,413,362,417,415,
    421,365,348,367,383,386,367,386,422,423,424,383,423,383,367,379,388,424,379,424,423,347,389,388,
    347,388,379,426,470,469,426,469,427,427,469,468,427,468,429,354,407,470,354,470,426,433,477,476,
    433,476,430,430,476,475,430,475,434,436,478,477,436,477,433,468,489,428,474,481,473,434,475,474,
    478,436,479,443,481,474,442,482,480,442,451,484,442,484,482,438,448,451,438,451,442,441,449,448,
    441,448,438,444,453,449,444,449,441,483,486,453,483,453,444,444,422,483,445,479,415,451,522,484,
    453,486,386,487,538,323,487,323,313,429,468,538,429,538,487,493,546,544,493,544,490,490,544,545,
    490,545,495,498,549,548,498,548,496,496,548,546,496,546,493,567,36,537,537,467,568,535,473,542,
    533,543,324,495,545,328,549,498,481,500,516,519,500,519,329,550,551,516,550,516,500,512,521,551,
    512,551,550,480,522,521,480,521,512,559,53,560,563,4,556,574,575,573,158,577,576,158,576,564,
    578,579,580,578,580,581,572,87,88,572,88,573,573,88,571,573,571,574,574,571,156,574,156,575,
    156,564,576,156,576,575,572,573,582,572,582,583,573,575,584,573,584,582,575,576,579,575,579,584,
    579,576,577,579,577,580,585,581,580,585,580,586,587,577,158,588,589,585,588,585,586,590,591,592,
    590,592,593,596,572,597,599,406,569,599,569,600,569,598,600,601,602,603,601,603,604,572,596,570,
    572,570,87,570,596,597,570,597,404,598,569,404,598,404,597,605,597,572,605,572,583,601,598,597,
    601,597,605,601,604,600,601,600,598,604,603,606,604,606,607,607,608,609,607,609,610,608,611,612,
    608,612,613,608,613,614,614,615,616,619,615,592,619,592,591,578,602,620,578,620,621,622,623,624,
    622,624,625,626,627,623,626,623,622,628,629,627,628,627,626,630,629,628,630,628,631,602,578,630,
    602,630,631,620,632,633,620,633,621,633,632,635,633,635,634,636,289,617,636,617,637,592,638,639,
    592,639,624,640,641,615,640,615,625,595,219,594,595,594,642,595,642,586,586,580,587,577,587,580,
    586,587,595,625,624,639,625,639,640,638,592,615,638,615,641,630,578,581,630,581,585,624,623,643,
    624,643,593,623,627,643,627,644,643,630,585,589,630,589,629,219,595,587,219,587,158,602,631,606,
    602,606,603,631,628,611,631,611,606,628,626,612,628,612,611,626,613,612,622,613,626,622,625,614,
    622,614,613,619,616,615,618,467,406,618,406,599,641,640,639,641,639,638,644,627,629,644,629,589,
    646,616,619,646,619,647,593,592,624,625,615,614,616,648,613,616,613,614,648,649,650,648,650,651,
    613,648,651,613,651,643,608,613,643,608,643,588,608,588,650,608,650,649,621,584,579,621,579,578,
    584,621,633,584,633,634,601,605,620,620,602,601,632,620,605,632,605,635,635,605,583,634,635,583,
    634,583,582,634,582,584,289,594,219,289,219,158,289,158,156,289,156,88,289,88,87,289,87,404,
    289,404,406,289,406,467,289,467,617,642,594,588,594,650,588,650,645,651,645,591,651,649,646,652,
    589,588,644,606,611,608,606,608,607,645,636,637,645,637,647,636,653,594,636,594,289,617,652,637,
    645,653,636,652,646,647,652,647,637,591,645,647,591,647,619,594,653,650,653,645,650,586,642,588,
    590,651,591,599,600,618,600,610,618,610,600,604,610,604,607,649,648,616,616,646,649,608,649,617,
    608,617,609,649,652,617,617,467,609,609,467,618,609,618,610,588,643,593,643,644,593,644,588,593,
    593,643,651,593,651,590,583,655,635,583,635,656,657,602,620,657,620,658,657,660,606,657,606,603,
    659,661,614,659,614,612,662,663,664,662,664,665,666,662,665,666,665,667,668,666,667,668,667,669,
    670,668,669,670,669,671,672,670,671,672,671,673,674,672,673,674,673,675,676,674,675,676,675,677,
    678,676,677,678,677,679,611,680,659,611,659,612,681,664,663,681,663,682,683,620,602,683,602,603,
    613,612,614,685,683,603,603,686,685,604,687,603,603,688,604,613,649,612,613,614,649,657,603,602,
    686,603,687,606,688,603,660,680,611,660,611,606,691,684,682,602,658,654,602,654,632,654,655,635,
    654,635,632,684,692,693,684,693,614,602,657,658,659,613,614,659,614,693,659,612,613,694,663,695,
    694,695,696,696,695,666,696,666,697,697,666,698,697,698,699,699,698,700,699,700,701,701,700,702,
    701,702,703,703,702,704,703,704,705,705,704,676,705,676,677,679,678,690,679,690,689,680,611,612,
    680,612,659,663,694,706,663,706,691,707,603,685,603,707,687,691,708,692,691,692,684,666,668,698,
    668,670,698,670,672,702,702,672,674,702,674,704,704,674,676,706,708,691,678,679,689,678,689,690,
    711,681,712,681,684,712,684,614,661,684,661,711,648,710,649,648,649,614,648,614,684,648,649,709,
    709,710,648,620,683,685,620,685,632,603,602,632,603,632,685,654,658,620,654,620,632,712,684,711,
    682,684,681,695,663,662,666,695,662,700,698,670,702,700,670,691,682,663,707,685,686,687,707,686,
    710,709,649,583,713,634,583,634,655,657,658,621,657,621,578,657,581,585,657,585,660,659,644,593,
    659,593,661,714,715,694,714,694,716,717,697,715,717,715,714,718,719,697,718,697,717,720,701,719,
    720,719,718,721,703,701,721,701,720,722,723,703,722,703,721,724,725,723,724,723,722,726,727,725,
    726,725,724,589,644,659,589,659,680,706,728,716,706,716,694,729,581,578,729,578,621,731,581,729,
    581,731,732,580,581,733,581,580,734,643,644,650,643,650,593,689,736,735,657,578,581,732,733,581,
    585,581,734,660,585,589,660,589,680,737,728,730,578,633,654,578,654,658,654,633,634,654,634,655,
    730,593,693,730,693,692,578,658,657,659,693,593,659,593,643,659,643,644,694,696,714,696,697,717,
    696,717,714,697,699,738,697,738,717,699,701,739,699,739,738,701,703,740,701,740,739,703,705,741,
    703,741,740,705,677,724,705,724,741,679,689,736,679,736,726,680,659,644,680,644,589,716,737,706,
    742,731,581,581,733,742,737,730,692,737,692,708,717,738,718,718,738,739,718,739,720,739,740,721,
    739,721,720,740,741,722,740,722,721,741,724,722,728,737,716,706,737,708,726,736,689,726,689,679,
    726,679,727,726,724,677,726,677,679,694,715,665,694,665,664,715,697,667,715,667,665,719,701,671,
    719,671,669,703,723,675,703,675,673,681,706,694,681,694,664,711,708,706,711,706,681,706,708,730,
    730,711,661,730,661,593,651,593,650,651,650,744,651,730,593,651,743,650,743,651,744,669,667,697,
    669,697,719,673,671,701,673,701,703,677,675,723,677,723,725,679,677,725,679,725,727,621,633,731,
    621,731,729,581,731,633,581,633,578,654,633,621,654,621,658,708,711,730,728,706,730,742,732,731,
    733,732,742,744,650,743,746,622,745,650,651,643,650,643,588,643,651,747,643,747,622,651,650,745,
    651,745,747,650,588,746,650,746,745,588,643,622,588,622,746,622,747,745,749,748,728,751,752,750,
    754,753,751,755,756,754,756,757,754,758,756,755,759,758,755,761,757,758,758,757,756,762,763,758,
    762,758,759,762,759,760,762,760,764,764,760,761,764,761,763,763,761,758,762,764,763,767,766,765,
    767,765,768,768,765,766,770,771,768,770,768,769,772,770,769,772,768,773,771,773,768,774,770,772,
    775,774,772,775,772,773,779,272,275,779,275,777,780,779,777,780,777,778,780,778,266,780,266,280,
    779,780,280,779,280,272,782,781,779,783,782,779,783,779,780,783,780,782,784,785,781,783,782,785,
    785,782,781,787,785,786,787,786,783,788,787,783,787,783,785,791,790,792,792,790,793,794,790,791,
    795,792,793,795,793,790,795,790,794,796,794,795,796,795,743,799,798,797,799,797,796,799,743,744,
    798,651,650,798,650,797,800,798,799,800,799,744,800,744,737,737,744,651,737,651,730,804,805,802,
    804,802,803,749,748,802,749,802,805,753,806,804,807,753,804,807,750,753,753,750,752,753,752,806,
    808,758,756,808,756,755,809,808,755,759,809,755,808,763,758,762,763,810,810,763,811,812,810,762,
    812,762,765,814,765,813,813,765,812,770,769,771,272,776,774,272,774,779,779,774,776,779,776,272,
    752,749,750,749,751,750,749,752,751,754,753,755,751,757,754,759,755,754,759,754,760,754,757,761,
    754,761,760,768,766,767,769,768,772,776,771,770,776,770,774,775,773,269,269,773,771,269,771,776,
    275,776,774,275,774,777,777,774,775,777,775,778,775,269,266,775,266,778,266,269,776,266,776,275,
    782,780,779,784,781,782,784,782,783,785,784,786,786,784,783,790,789,791,789,219,793,789,793,790,
    219,789,790,219,790,793,791,792,795,791,795,794,797,794,796,795,650,743,650,795,794,650,794,797,
    796,743,799,801,798,800,730,651,798,730,798,801,802,801,803,803,801,800,800,737,728,800,728,803,
    737,730,748,737,748,728,748,730,801,748,801,802,803,728,804,728,749,804,749,805,804,804,749,750,
    804,750,807,749,804,806,749,806,752,753,756,755,811,808,809,811,809,810,810,809,759,810,759,762,
    763,808,811,812,765,814,769,813,812,769,812,814,814,813,769,815,691,710,815,710,816,648,684,691,
    648,691,710,737,691,684,737,684,730,744,710,691,744,691,737,651,648,710,651,710,744,651,730,684,
    651,684,648,801,798,799,801,799,800,651,801,798,648,651,730,648,730,684,817,648,684,817,818,648,
    684,648,818,684,818,817,651,730,801,737,800,799,737,799,744,
};
static const uint8_t kGrHC[2457] = {
    88,85,63,89,87,66,87,84,64,84,81,64,90,87,65,90,88,66,98,93,75,93,90,70,
    79,78,64,84,82,66,79,78,64,82,80,65,92,88,69,103,100,82,85,81,67,82,80,66,
    86,82,68,82,78,65,90,86,66,88,85,66,85,81,66,86,84,66,84,80,65,86,83,65,
    83,81,67,94,91,70,87,84,64,87,84,64,82,79,65,98,95,75,89,87,65,101,97,79,
    83,81,65,88,86,64,83,82,66,80,76,63,87,80,67,87,84,66,87,84,65,90,85,67,
    82,79,65,92,88,74,89,83,67,80,79,65,78,77,63,83,80,64,87,83,68,85,82,64,
    88,85,67,87,82,67,90,85,68,88,85,66,99,95,75,89,87,65,96,93,71,92,89,68,
    86,84,65,87,84,64,88,85,62,90,88,66,88,85,63,89,86,64,113,109,89,93,90,69,
    90,86,68,88,85,66,88,85,64,93,91,69,101,98,78,108,105,84,95,87,66,92,85,67,
    92,86,67,94,86,64,92,87,67,93,87,66,90,88,65,88,84,64,93,85,68,90,87,64,
    94,89,69,92,89,68,87,85,62,93,90,69,91,87,66,97,94,74,85,83,62,128,123,117,
    124,110,103,89,86,67,86,84,65,89,86,67,84,81,66,84,81,66,85,82,66,88,84,67,
    86,84,67,86,83,67,86,83,67,86,82,66,87,84,66,91,88,68,90,88,68,89,83,66,
    89,86,64,96,93,71,89,85,65,90,87,67,89,86,65,91,88,65,96,94,71,88,85,63,
    87,84,62,104,98,78,103,99,79,91,88,66,98,95,75,116,112,93,92,89,68,88,85,65,
    91,88,66,88,85,63,88,85,63,98,95,73,94,88,68,138,132,114,108,102,83,90,85,63,
    116,111,92,103,100,81,85,83,62,89,85,66,87,84,63,92,86,66,97,91,71,94,89,70,
    90,88,64,97,93,72,93,88,71,96,87,67,92,88,69,93,87,67,92,88,65,90,88,65,
    89,84,64,90,87,64,89,86,63,89,85,65,88,85,63,89,86,64,92,89,68,88,86,64,
    120,116,96,89,87,65,89,86,66,89,87,65,121,107,99,89,85,66,123,114,108,92,86,68,
    85,83,64,88,80,66,79,78,64,91,85,71,84,81,64,91,84,68,84,81,64,83,80,66,
    90,85,70,103,99,79,101,98,79,81,79,65,88,85,70,101,96,80,88,83,68,90,84,69,
    88,82,68,90,85,68,88,85,64,91,87,65,93,91,69,94,91,70,90,88,65,93,90,68,
    90,87,66,89,86,65,105,99,80,91,88,67,93,90,68,100,97,77,86,82,63,89,86,66,
    91,88,67,87,83,63,98,94,74,105,102,83,96,93,73,91,88,68,90,86,64,89,87,67,
    87,83,63,89,86,64,88,86,66,87,84,63,88,85,63,85,83,63,89,87,65,89,86,66,
    85,83,67,87,85,67,93,88,68,92,89,67,97,94,72,86,83,62,94,91,70,94,91,70,
    98,95,74,89,86,65,88,85,64,117,104,98,87,84,65,88,85,66,88,85,66,85,81,68,
    82,80,65,92,87,71,88,82,65,90,86,70,83,81,65,84,81,66,85,82,65,84,82,65,
    87,82,66,88,84,68,88,85,65,90,84,66,86,84,65,84,82,67,92,86,70,90,88,67,
    92,89,67,94,88,68,87,84,62,88,85,62,90,86,64,89,86,64,111,108,87,97,94,74,
    88,85,62,90,86,65,99,94,74,91,86,64,87,84,65,90,87,67,92,91,73,91,88,69,
    95,91,70,92,89,67,133,128,109,95,92,72,92,88,66,97,94,74,100,94,76,94,88,70,
    108,104,85,96,92,75,112,104,98,96,89,68,91,86,66,131,121,117,99,87,68,90,86,67,
    129,121,117,89,85,66,90,86,66,126,116,112,87,85,67,88,84,65,83,81,66,88,85,66,
    98,93,85,88,85,63,90,87,66,87,85,63,90,87,66,89,85,65,91,88,66,105,102,82,
    86,84,62,83,80,68,86,84,65,84,80,65,80,79,64,88,82,68,96,92,76,85,81,66,
    85,83,66,129,125,108,93,89,69,80,79,65,86,83,66,94,89,69,89,85,69,90,85,70,
    88,86,63,90,88,64,90,87,64,92,89,66,90,88,65,90,88,65,92,89,66,91,88,65,
    85,82,65,89,86,66,100,96,78,88,86,67,91,84,67,88,85,67,88,84,64,85,83,64,
    87,85,64,88,85,64,88,85,65,89,85,67,84,81,65,82,79,64,88,84,68,86,84,65,
    84,79,65,85,83,66,85,82,65,85,84,65,88,86,66,88,84,66,126,122,102,88,85,64,
    94,91,72,91,87,66,93,88,67,90,84,66,88,84,64,93,88,68,95,91,71,88,84,66,
    89,82,67,83,81,64,84,82,65,86,83,67,82,81,65,86,84,64,85,82,64,85,82,64,
    88,85,64,89,86,64,95,92,71,89,86,65,89,86,64,107,103,82,87,85,63,88,84,63,
    94,91,69,104,101,80,121,115,96,94,91,71,93,90,69,92,89,68,88,85,63,87,84,64,
    88,85,64,88,85,63,89,87,64,101,98,77,92,90,69,146,141,122,125,122,102,88,85,63,
    88,86,64,88,85,65,100,97,77,90,86,66,87,85,64,92,87,67,90,88,65,90,88,68,
    93,90,69,89,86,64,89,86,67,88,85,64,88,84,64,85,83,64,90,86,65,91,88,66,
    88,85,64,90,87,65,89,87,64,89,85,65,88,85,63,88,85,64,91,88,67,89,86,65,
    118,114,94,88,86,64,86,84,63,86,84,63,122,116,110,90,87,65,119,108,101,88,85,66,
    85,83,65,81,79,64,84,81,66,86,81,68,86,81,66,85,81,65,83,81,65,83,81,65,
    89,86,70,109,104,85,90,88,67,80,79,65,87,83,66,91,86,67,83,82,65,81,80,64,
    84,82,66,83,80,63,90,88,66,98,95,74,89,87,65,96,93,72,91,88,65,91,88,67,
    99,93,75,93,89,67,116,112,92,94,91,70,93,87,66,99,89,70,88,85,65,88,86,64,
    92,89,68,87,85,65,95,91,72,101,98,78,99,95,76,92,89,68,91,87,65,95,87,66,
    90,85,67,89,84,66,91,86,64,92,87,68,90,84,64,87,83,63,88,85,64,89,85,65,
    89,84,65,89,85,65,89,85,65,87,84,62,95,88,68,93,89,68,91,87,66,97,94,73,
    91,88,67,86,83,62,91,88,67,119,111,106,86,83,64,87,84,66,85,83,64,83,81,66,
    88,82,68,85,83,66,86,82,65,94,90,74,89,87,71,88,83,67,85,82,65,84,81,64,
    88,86,68,84,81,66,88,85,66,85,82,64,88,85,66,85,81,64,85,82,64,90,85,64,
    89,87,64,91,87,67,96,93,72,88,87,64,89,86,64,99,96,75,92,87,66,92,89,68,
    88,86,64,99,96,75,108,104,84,87,85,63,88,85,65,87,84,63,86,84,63,101,98,77,
    89,86,64,89,87,65,101,98,77,96,93,73,89,86,64,93,89,68,92,88,68,106,101,81,
    109,105,86,89,86,65,90,86,65,89,85,64,91,85,65,93,87,65,88,84,64,86,84,64,
    91,86,65,92,86,65,94,89,69,88,85,62,86,84,66,89,86,66,84,82,67,87,85,64,
    88,85,64,88,85,63,92,90,68,87,84,63,92,89,67,90,87,66,88,86,64,101,98,78,
    85,84,62,82,79,66,86,83,65,79,78,62,84,80,66,81,79,64,92,88,72,85,81,65,
    83,81,64,88,84,64,89,87,66,79,78,64,85,82,64,94,91,71,85,82,66,85,84,68,
    92,90,66,92,89,66,90,88,64,91,88,65,90,87,66,88,86,63,92,89,67,111,108,87,
    90,87,65,99,96,74,89,86,64,92,88,67,97,93,83,79,76,63,77,75,62,78,76,63,
    77,75,62,92,89,79,90,87,76,94,86,74,138,135,134,138,134,132,141,140,139,149,145,144,
    161,156,155,161,157,156,124,113,109,156,150,149,145,135,133,132,124,122,126,119,116,123,118,117,
    138,132,131,134,123,120,161,158,158,137,129,127,122,117,115,132,121,118,119,116,115,145,138,137,
    131,120,118,127,119,115,137,126,123,138,127,124,142,142,142,151,148,147,146,142,142,128,122,121,
    151,146,146,139,135,134,122,118,116,129,124,123,137,131,130,131,126,123,131,124,122,147,144,142,
    141,134,132,151,150,149,144,142,142,130,123,122,131,123,121,118,110,108,131,127,126,138,131,129,
    154,150,149,135,122,118,139,132,130,149,144,142,127,120,116,128,112,107,116,107,104,122,116,114,
    120,113,111,117,110,108,112,105,102,118,111,108,112,106,103,116,105,101,114,104,100,116,104,101,
    135,127,124,137,124,122,137,127,125,136,126,123,146,142,140,139,127,124,109,108,106,115,109,106,
    111,110,109,116,115,114,144,144,144,116,108,104,129,118,114,151,149,149,146,139,136,148,142,141,
    122,111,108,133,121,118,118,110,107,117,108,104,150,138,136,150,148,146,131,128,127,131,127,126,
    130,107,100,141,141,141,143,142,142,138,136,135,138,138,137,158,155,154,147,137,134,140,133,132,
    156,149,147,148,140,138,145,136,134,143,136,134,145,134,130,144,135,131,148,135,132,147,134,130,
    149,140,138,143,133,131,154,146,144,145,137,134,147,134,130,141,134,133,147,140,138,135,128,126,
    138,137,137,148,141,139,148,138,136,152,148,146,120,107,103,142,133,131,141,136,136,134,124,121,
    128,120,119,148,135,132,149,135,132,126,115,112,104,102,101,112,111,111,137,124,121,146,140,139,
    151,151,151,138,134,132,130,119,116,151,151,150,136,106,98,135,131,129,137,125,122,134,121,117,
    148,133,129,129,128,126,145,140,139,111,104,102,140,139,138,133,123,121,127,116,111,146,146,146,
    153,152,152,136,130,128,146,133,131,141,135,133,135,127,124,142,133,130,149,140,138,138,131,127,
    142,128,124,152,142,140,161,156,155,138,124,120,147,130,125,140,134,133,144,133,131,136,131,129,
    129,121,119,152,141,139,121,112,109,141,121,117,141,132,129,139,125,122,139,125,122,149,141,139,
    151,143,140,128,120,117,150,147,147,153,150,148,140,135,133,152,147,145,105,101,100,125,119,117,
    122,106,102,116,104,100,124,110,106,114,99,92,133,126,124,142,135,133,146,138,136,149,139,137,
    141,132,130,142,134,133,151,141,139,137,129,128,151,141,139,157,147,144,135,126,124,145,136,134,
    151,140,138,151,140,138,137,127,125,143,135,132,156,147,145,138,128,125,146,134,131,145,134,131,
    148,135,132,138,132,130,139,134,132,140,131,129,158,143,139,150,136,133,151,143,141,158,144,140,
    136,126,123,151,140,138,150,138,136,143,138,136,132,126,125,143,137,136,121,116,115,140,129,127,
    153,142,139,125,116,114,148,136,133,134,123,121,150,139,136,145,138,136,148,142,140,150,144,142,
    134,125,124,127,119,117,152,144,142,126,117,116,146,139,137,146,136,134,140,132,130,139,131,128,
    133,127,125,136,128,126,152,148,147,146,142,141,150,144,142,166,164,163,148,143,142,151,143,141,
    148,142,141,152,145,144,145,135,132,141,134,131,145,136,134,139,132,131,146,140,139,150,145,145,
    154,146,145,152,143,140,149,142,141,
};
static const float kGrScale = 1.0f / 200000.0f;
static const int16_t kSoV[807] = {
    -1733,30333,-4767,-1733,25133,-3900,-1733,25133,-3033,-1733,30333,-3033,-2600,30333,-4767,-2600,25133,-3900,-2600,25133,-3033,-2600,30333,-3033,
    -1300,-1301,433,-3033,-4769,-1300,-3033,-5636,433,-1300,-5636,433,-3033,-1301,433,-1300,-1301,-1300,-1300,-4769,-1300,-3033,-1301,-1300,
    -1300,-5636,-433,-3033,-5636,-433,-2167,19067,3033,2167,22533,1300,-2167,22533,1300,2167,19067,3033,2167,19067,1300,2167,22533,3033,
    -2167,22533,3033,-2167,19067,1300,-2327,22403,1300,-2327,22403,-3900,-2754,19839,-3900,-2754,19839,1300,-4037,22688,1300,-4037,22688,-3900,
    -4464,20123,-3900,-4464,20123,1300,1300,27300,1300,1300,30767,1300,1300,27300,2167,1300,30767,2167,2167,27300,1300,2167,30767,1300,
    2167,27300,2167,2167,30767,2167,-1733,31200,-2600,0,31200,-2600,1733,31200,-2600,-867,-867,-867,-1589,0,-2456,-867,-4333,1733,
    -867,-867,1733,1733,867,1733,1733,867,-2600,1733,-867,-867,1733,-4333,-1733,1733,-4333,1733,1733,-867,1733,-867,-2600,-867,
    -867,-2600,3467,1733,-2600,-867,1733,-2600,3467,3467,27733,1733,-867,-867,-867,867,0,2600,867,0,1733,1733,867,-1733,
    1733,4333,-1733,867,4333,2600,3467,28600,867,3467,28600,-1733,-867,-867,1733,3467,27733,-2600,1838,867,-613,-245,867,1716,
    1838,-867,-613,-245,-867,1716,858,867,123,613,867,1838,858,-867,123,613,-867,1838,2600,23400,1733,2600,23400,-2600,
    867,31200,1733,2574,0,368,2329,0,2084,1716,1733,245,1471,1733,1961,0,-867,-867,867,867,-867,3467,867,1733,
    867,867,1733,3467,-867,1733,0,-867,1733,5883,867,1716,-867,-7800,-1733,1733,-7800,-1733,-867,-7800,867,1733,-7800,867,
    867,-867,-867,867,-4333,-1733,867,-4333,1733,867,-867,1733,-1733,867,1733,-1733,867,-2600,-1733,-867,-867,-1733,-4333,-1733,
    -1733,-4333,1733,-1733,-867,1733,867,-2600,-867,867,-2600,3467,-1733,-2600,-867,-1733,-2600,3467,-3467,27733,1733,-867,867,-867,
    -867,0,2600,-867,0,1733,-1733,867,-1733,-1733,4333,-1733,-867,4333,2600,-3467,28600,867,-3467,28600,-1733,-867,867,1733,
    -3467,27733,-2600,1838,-867,-613,-245,-867,1716,-245,867,1716,858,-867,123,613,-867,1838,858,867,123,613,867,1838,
    -2600,23400,1733,-2600,23400,-2600,-867,31200,1733,2574,0,368,2329,0,2084,1716,-1733,245,1471,-1733,1961,0,867,-867,
    1838,867,-613,867,-867,-867,3467,-867,1733,867,-867,1733,3467,867,1733,0,867,1733,0,27733,1733,0,27733,-2600,
    0,31200,1733,0,0,1733,0,0,2600,0,867,-1733,0,4333,-1733,0,4333,2600,0,23400,1733,0,23400,-2600,
    0,18200,-2600,867,-7800,-1733,-1733,-7800,-1733,867,-7800,867,-1733,-7800,867,2327,22403,1300,2327,22403,-3900,2754,19839,-3900,
    2754,19839,1300,4037,22688,1300,4037,22688,-3900,4464,20123,-3900,4464,20123,1300,1300,-4769,-1300,1300,-433,1300,3033,-4769,-1300,
    3033,-4769,1300,1300,-4769,1300,3033,-433,1300,1300,-1301,-1300,3033,-1301,-1300,3033,-433,-433,1300,-433,-433,3033,-1301,-433,
    1300,-1301,-433,-867,-1733,-2600,1733,-1733,-2600,867,-1733,-2600,-1733,-1733,-2600,6128,-867,0,6128,867,0,5883,-867,1716,
    5883,-867,1716,6128,867,0,6128,-867,0,5883,867,1716,-867,867,1733,867,867,1733,-1733,867,1733,1733,867,1733,
    -3033,-5636,-33,-1300,-5498,-571,-3033,-5498,-571,-1300,-5636,-33,3467,867,-867,3467,-867,-867,3467,867,-867,3467,-867,-867,
    -867,-4333,-1733,1589,0,-2456,0,18200,1733,-1589,0,1733,1589,0,1733,1733,433,433,1733,3033,-1300,1733,3900,-1300,
    1733,3900,433,867,3900,3900,867,433,3900,0,3900,3900,-1733,433,433,-1733,3033,-1300,-1733,3900,-1300,-1733,3900,433,
    -867,3900,3900,-867,433,3900,0,433,3900,0,3900,2601,-1648,3900,-1300,1647,3900,-1300,-1307,433,433,1647,3033,-1300,
    -1648,3033,-1300,1300,433,433,0,433,2601,1300,3900,433,-1307,3900,433,-880,433,2601,-880,3900,2601,867,3900,2600,
    867,433,2600,-1733,30333,-4333,-1733,25133,-4333,0,25133,-4333,3322,19067,1733,3322,19067,-2600,-3322,19067,-2600,-3322,19067,1733,
    -1733,31200,-2600,-1733,25133,-2600,-1733,28600,-3033,-1733,28600,-4767,-2600,28600,-3033,-2600,28600,-4767,1733,25133,-3900,1733,30333,-4767,
    0,19067,1300,0,19067,3033,0,22533,3033,0,22533,1300,1733,25133,-3033,1733,30333,-3033,2600,30333,-4767,2600,25133,-3900,
    2600,25133,-3033,2600,30333,-3033,1733,30333,-4333,1733,25133,-4333,0,31200,-2600,0,30333,-4333,1733,31200,-2600,1733,25133,-2600,
    0,25133,-2600,1733,28600,-3033,1733,28600,-4767,2600,28600,-3033,2600,28600,-4767,
};
static const uint8_t kSoG[269] = {
    0,0,0,0,0,0,0,0,5,5,5,5,5,5,5,5,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,2,2,
    4,2,2,4,2,2,4,4,4,4,4,0,8,1,1,1,1,1,0,0,8,0,9,9,9,9,10,10,10,10,0,0,0,10,10,10,10,8,8,8,8,8,8,9,3,3,3,3,
    7,5,5,7,5,5,7,5,5,7,7,7,7,7,0,11,1,1,1,1,1,0,0,11,0,12,12,12,13,13,13,13,0,0,0,13,13,13,13,11,12,11,11,11,11,11,0,0,
    0,1,1,1,1,1,0,0,0,6,6,6,6,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,3,3,6,6,9,9,9,12,12,12,12,3,6,6,3,
    5,5,5,5,8,8,11,11,2,5,0,2,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const uint16_t kSoT[1437] = {
    7,0,4,245,1,5,242,7,244,6,1,2,4,244,7,242,0,3,9,12,15,11,12,10,
    13,12,8,9,193,194,17,195,192,251,23,19,25,18,24,86,196,85,248,21,249,22,23,21,
    33,26,30,30,27,31,32,29,33,32,27,28,30,32,33,28,26,29,37,34,36,41,36,40,
    39,40,38,36,38,40,41,35,37,233,241,240,45,57,55,78,59,150,58,55,57,54,57,51,
    48,55,56,54,56,58,151,69,79,148,65,64,147,64,63,149,61,65,61,145,62,63,61,62,
    87,71,196,87,73,71,196,71,70,69,68,67,59,67,66,152,237,50,62,144,80,77,81,82,
    77,82,75,74,83,81,74,81,76,84,81,83,82,84,75,75,83,74,59,60,80,142,80,144,
    79,237,152,64,61,63,65,61,64,68,90,67,67,88,66,137,135,199,196,86,87,54,93,95,
    155,179,189,94,177,92,68,85,90,67,90,88,66,88,86,69,78,79,143,44,69,62,44,63,
    183,76,77,145,144,62,80,44,62,154,180,153,143,69,151,59,78,69,150,59,142,91,71,183,
    182,74,181,182,75,74,93,45,92,48,95,94,95,178,191,48,92,45,51,57,45,56,55,58,
    58,57,54,45,55,48,48,56,54,149,65,148,148,64,147,146,61,149,146,145,61,59,66,60,
    89,73,87,91,77,75,69,67,59,49,237,236,76,81,77,82,81,84,84,83,75,59,80,142,
    151,79,152,123,198,140,136,198,123,199,136,121,51,93,54,103,190,104,80,60,44,70,182,72,
    93,92,178,51,45,93,54,95,48,94,92,48,96,106,108,128,150,110,109,108,106,105,102,108,
    99,107,106,105,109,107,151,129,120,148,115,116,147,114,115,149,116,112,112,113,145,114,113,112,
    138,122,123,122,187,123,122,121,184,120,118,119,110,117,118,100,103,104,113,130,144,127,132,131,
    127,125,132,124,131,133,124,126,131,134,133,131,132,125,134,125,124,133,110,130,111,142,144,130,
    152,238,129,115,114,112,116,115,112,119,118,141,118,117,139,141,198,135,85,196,197,105,156,154,
    191,47,188,178,52,191,119,141,135,118,139,141,117,137,139,120,129,128,125,186,124,145,113,144,
    143,151,120,110,120,128,150,142,110,154,153,96,99,155,156,99,96,153,49,52,50,102,96,108,
    107,109,106,109,105,108,96,99,106,99,105,107,149,148,116,148,147,115,146,149,112,146,112,145,
    110,111,117,140,138,123,117,111,137,120,110,118,101,238,152,126,127,131,132,134,131,134,125,133,
    110,142,130,151,152,129,139,140,141,72,73,197,102,105,154,179,97,189,102,154,96,105,99,156,
    155,99,153,157,164,161,158,161,162,160,163,164,158,163,159,163,161,164,157,159,160,63,43,147,
    44,43,63,43,44,143,111,135,137,143,120,42,114,147,43,113,114,42,111,119,135,130,113,42,
    42,114,43,130,42,111,43,143,42,175,170,168,170,169,168,167,171,172,169,167,168,173,176,174,
    172,176,175,167,175,168,170,174,166,192,9,194,13,9,15,103,97,180,178,92,177,153,180,179,
    180,97,179,72,182,181,185,124,186,185,187,126,125,127,184,89,88,90,137,138,139,138,199,122,
    90,197,89,123,185,136,121,136,186,73,181,183,186,136,185,187,185,123,184,187,122,184,121,186,
    190,155,189,94,191,188,189,98,190,180,156,190,189,97,98,192,194,17,180,190,103,191,52,53,
    194,16,17,192,11,10,53,47,191,190,98,104,204,202,100,100,98,204,47,49,203,202,46,203,
    156,155,190,95,191,94,153,179,155,188,177,94,93,178,95,154,156,180,101,152,201,183,71,73,
    72,181,73,71,91,70,70,91,182,197,196,72,197,73,89,122,199,121,126,187,127,126,124,185,
    184,186,125,184,127,187,181,76,183,183,77,91,181,74,76,91,75,182,47,200,188,178,200,52,
    177,200,178,188,200,177,52,200,50,103,101,97,239,202,128,202,150,128,202,78,150,47,203,200,
    50,46,152,100,202,239,206,208,205,207,208,206,205,208,210,208,231,209,210,208,209,213,212,215,
    214,213,215,212,217,215,219,232,226,217,216,215,226,229,219,207,221,208,209,218,210,222,228,229,
    222,224,228,216,230,215,220,215,228,217,218,216,224,214,220,216,219,230,213,224,212,222,229,212,
    231,219,209,225,227,223,206,223,207,225,223,205,225,232,227,218,229,226,226,232,218,205,232,225,
    49,202,203,233,235,234,203,46,200,202,204,201,60,85,68,60,86,85,66,86,60,44,60,68,
    69,44,68,42,119,111,120,119,42,239,128,129,237,79,236,236,202,49,236,79,78,78,202,236,
    238,239,129,238,100,239,235,241,234,1,242,2,5,244,245,6,242,244,4,243,245,53,49,47,
    50,200,46,97,101,201,104,98,100,50,237,49,101,103,100,201,204,97,98,97,204,202,201,152,
    53,52,49,216,218,211,211,218,209,227,231,208,208,221,227,215,230,228,209,219,211,211,219,216,
    214,215,220,229,228,230,219,229,230,207,223,221,228,224,220,231,232,219,227,232,231,223,227,221,
    213,214,224,212,224,222,205,223,206,217,229,218,212,229,217,218,232,210,210,232,205,29,26,33,
    26,27,30,28,29,32,31,27,32,31,32,30,27,26,28,160,164,157,157,161,158,159,163,160,
    162,163,158,162,161,163,158,159,157,250,23,251,21,23,249,87,86,88,249,23,250,19,23,22,
    22,21,248,199,135,198,199,138,137,198,136,199,20,25,24,72,196,70,248,249,18,251,20,24,
    250,251,24,18,249,24,249,250,24,25,248,18,85,197,90,138,140,139,140,198,141,87,88,89,
    3,0,7,243,1,245,3,7,242,5,1,6,245,244,4,243,0,242,243,242,1,6,244,5,
    2,242,6,0,243,4,234,241,233,10,12,9,8,12,11,15,12,13,14,193,9,16,195,17,
    10,9,192,14,9,13,193,16,194,195,11,192,173,170,175,166,169,170,165,171,167,165,167,169,
    175,176,173,171,176,172,172,175,167,173,174,170,35,34,37,37,36,41,41,40,39,34,38,36,
    39,35,41,46,202,152,101,100,238,257,254,247,268,255,246,265,267,257,256,252,246,254,257,267,
    265,253,247,258,262,263,262,258,261,263,262,260,246,252,265,255,268,267,256,267,265,254,268,266,
    253,257,247,266,268,246,253,265,257,255,256,246,268,254,267,266,265,247,266,246,265,256,255,267,
    252,256,265,247,254,266,259,258,263,260,262,261,264,263,260,240,261,233,241,260,240,261,235,233,
    264,241,235,260,261,240,264,260,241,258,259,235,235,259,263,261,258,235,264,235,263,
};
static const uint8_t kSoUV[2874] = {
    180,48,188,56,188,48,172,32,156,24,156,32,172,56,180,48,172,48,156,48,152,40,152,48,180,32,172,48,180,48,172,8,180,24,180,8,196,56,204,16,196,16,212,64,204,16,204,64,
    196,0,204,16,204,0,196,64,199,80,199,64,200,64,201,80,201,64,234,0,224,16,224,0,216,48,224,48,224,16,116,112,116,88,124,120,234,64,224,48,234,48,216,48,224,16,224,48,
    192,240,204,255,204,240,192,216,168,200,168,216,168,240,192,255,192,240,168,240,160,216,160,240,192,216,168,240,192,240,168,176,192,200,192,176,204,184,208,216,204,216,200,184,204,216,200,216,
    196,184,200,216,196,216,204,216,200,224,200,216,200,184,204,176,204,184,236,88,244,144,244,80,88,176,100,192,88,192,32,72,36,32,20,72,104,168,116,208,104,208,100,144,88,160,88,144,
    100,144,88,160,108,160,124,144,112,160,124,160,0,72,16,32,12,72,128,208,124,168,120,208,128,240,120,208,120,240,128,168,124,128,124,168,124,128,128,120,124,120,120,240,100,248,104,248,
    104,88,104,80,116,88,104,88,96,80,104,80,116,88,104,80,116,64,40,32,44,8,44,24,60,32,44,24,56,24,0,120,16,112,16,120,124,120,128,112,124,112,72,72,80,88,72,88,
    96,16,100,0,104,16,116,16,112,8,120,0,116,16,120,0,124,16,72,104,80,88,80,104,100,0,108,8,104,16,72,112,80,104,80,112,36,32,36,8,24,0,20,32,24,0,20,0,
    12,72,16,112,0,120,120,208,100,248,120,240,100,208,100,248,120,208,96,128,96,120,104,128,104,128,104,112,116,128,116,112,124,120,116,88,116,88,116,112,104,88,100,144,88,128,100,128,
    80,255,64,200,84,176,80,255,64,200,68,255,96,128,84,120,96,120,104,128,96,120,104,112,116,128,104,112,116,112,40,32,60,72,40,72,0,32,8,0,16,32,104,248,120,255,120,240,
    92,24,84,16,92,16,128,120,128,112,124,120,100,255,120,255,104,248,16,255,16,200,4,255,0,32,16,32,0,72,60,32,60,72,40,32,20,72,36,32,20,32,104,24,104,80,96,24,
    116,24,116,16,124,24,116,24,108,16,116,16,100,160,88,176,88,160,112,144,124,128,112,128,56,255,40,200,60,176,100,144,88,128,88,144,100,176,100,192,88,176,116,168,116,208,104,168,
    108,160,88,160,100,144,88,144,88,160,100,144,112,144,112,160,124,144,128,168,124,168,128,208,128,208,120,208,128,240,128,128,124,128,128,168,128,128,128,120,124,128,60,32,56,24,56,8,
    96,88,96,80,104,88,104,24,96,16,104,16,40,32,44,24,60,32,60,120,40,112,60,112,80,72,80,88,72,72,72,88,80,88,72,104,72,104,80,104,72,112,36,32,24,0,20,32,
    0,72,12,72,0,120,96,80,84,88,96,88,84,64,84,88,96,80,116,88,124,64,116,64,88,144,88,128,100,144,44,168,60,176,60,168,88,216,100,224,92,255,116,64,116,24,124,64,
    16,255,4,255,16,200,100,176,88,176,100,160,124,144,124,128,112,144,100,128,88,128,100,144,88,176,88,192,100,192,32,72,20,72,36,32,104,168,104,208,116,208,100,144,88,144,88,160,
    100,144,108,160,88,160,124,144,124,160,112,160,0,72,12,72,16,32,128,208,120,208,124,168,128,240,120,240,120,208,128,168,124,168,124,128,124,128,124,120,128,120,120,240,104,248,100,248,
    104,88,104,80,96,80,104,80,96,24,96,80,104,80,116,64,108,24,40,32,44,24,44,8,60,32,56,24,44,24,60,120,44,168,60,168,124,120,124,112,128,112,72,72,72,88,80,88,
    96,16,104,16,100,0,116,16,120,0,112,8,116,16,124,16,120,0,72,104,80,104,80,88,100,0,104,16,108,8,72,112,80,112,80,104,36,32,24,0,36,8,20,32,20,0,24,0,
    0,120,16,112,12,72,120,208,120,240,100,248,100,208,120,208,100,248,96,128,104,128,96,120,104,128,116,128,104,112,96,120,84,88,84,120,124,120,116,88,124,88,100,144,100,128,88,128,
    36,176,24,168,24,176,40,200,44,168,60,176,96,128,96,120,84,120,104,128,104,112,96,120,116,128,116,112,104,112,40,32,40,72,60,72,108,16,116,24,116,16,128,120,124,120,128,112,
    0,32,0,72,16,32,60,32,40,32,60,72,20,72,20,32,36,32,100,160,88,160,88,176,112,144,112,128,124,128,100,144,88,144,88,128,60,120,44,168,40,120,100,176,88,176,100,192,
    116,168,104,168,116,208,108,160,100,144,88,160,88,144,100,144,88,160,112,144,124,144,112,160,128,168,128,208,124,168,128,208,128,240,120,208,128,128,128,168,124,128,128,128,124,128,128,120,
    60,32,56,8,56,24,96,88,104,88,96,80,116,128,124,128,116,112,40,32,60,32,44,24,16,120,16,112,0,120,80,72,72,72,80,88,72,88,72,104,80,88,72,104,72,112,80,104,
    36,32,20,32,24,0,0,72,0,120,12,72,104,112,96,88,96,120,84,64,96,80,84,88,88,144,100,144,88,128,64,200,68,168,84,176,100,176,100,160,88,176,124,144,112,144,124,128,
    100,128,100,144,88,128,204,255,192,240,204,240,168,200,192,216,168,216,192,255,168,240,192,240,160,216,168,240,160,240,168,240,192,216,192,240,192,200,168,176,192,176,120,240,128,255,128,240,
    120,255,128,255,120,240,0,0,8,0,0,32,124,128,124,120,116,112,0,32,16,32,8,0,120,240,128,240,128,255,104,248,120,240,120,255,84,128,96,128,84,120,100,255,104,248,120,255,
    120,255,120,240,128,255,88,216,92,255,100,224,0,0,0,32,8,0,208,104,216,96,216,136,216,96,224,136,216,136,204,136,196,104,204,104,216,152,204,136,216,136,204,88,196,96,196,88,
    204,104,196,96,204,96,204,136,208,104,216,136,216,96,208,80,216,80,201,64,196,56,199,62,188,16,196,56,196,16,16,168,4,168,16,200,16,200,4,255,4,200,4,255,16,200,4,200,
    16,200,4,168,4,200,124,64,116,24,124,24,124,24,116,16,116,24,84,24,92,24,84,16,104,16,96,16,104,24,96,88,104,112,96,120,116,112,104,88,104,112,104,88,116,88,104,80,
    96,120,84,88,96,88,96,80,84,24,84,64,116,64,124,64,116,24,96,80,84,24,92,24,116,24,124,64,124,24,92,24,84,24,96,80,104,24,96,24,104,80,108,24,116,64,116,24,
    36,176,24,255,24,176,24,255,36,176,24,176,24,176,24,168,36,176,40,200,56,255,60,176,84,176,68,168,84,168,201,64,199,62,200,64,40,200,60,176,44,168,60,176,44,168,60,168,
    199,64,200,80,200,64,201,64,204,80,204,64,36,168,24,168,36,176,36,176,24,168,36,168,20,128,20,120,36,120,36,120,24,168,20,128,24,168,36,120,20,128,84,120,64,128,84,128,
    36,255,24,255,36,176,36,255,36,176,24,255,68,255,64,200,80,255,84,176,64,200,80,255,44,255,40,200,56,255,44,255,56,255,40,200,16,120,0,120,0,128,96,24,104,80,96,80,
    84,64,84,24,96,80,104,80,108,24,116,64,116,64,108,24,116,24,124,88,116,88,124,64,84,88,96,80,96,88,104,80,116,88,116,64,84,16,92,24,92,16,124,16,116,16,124,24,
    108,24,116,24,108,16,104,24,96,16,96,24,84,24,84,16,92,24,96,24,96,16,104,24,124,24,116,16,124,16,108,24,108,16,116,24,84,168,68,168,84,176,16,200,4,168,16,168,
    4,200,4,168,16,200,84,176,68,168,64,200,16,168,4,168,16,120,16,168,16,120,4,168,36,112,20,120,32,72,20,120,20,72,32,72,20,120,32,72,20,72,84,168,84,128,68,168,
    16,120,0,128,0,120,36,120,20,120,36,112,164,128,172,120,172,152,164,120,172,120,164,128,172,152,172,120,188,152,172,120,181,112,188,112,188,152,172,120,188,120,164,128,172,152,172,120,
    164,120,164,128,172,120,172,152,188,152,172,120,188,64,184,96,188,96,188,152,188,120,172,120,188,96,184,96,188,64,164,120,164,119,172,120,188,120,192,152,188,152,168,96,168,64,184,96,
    168,96,160,72,168,64,188,112,182,112,172,120,164,119,172,120,172,116,188,152,192,152,188,120,163,128,164,120,163,120,188,112,182,104,182,112,164,128,163,128,164,152,172,155,182,159,172,152,
    181,112,182,104,188,112,168,96,168,64,160,72,164,128,163,128,164,120,162,152,163,128,164,152,168,96,184,96,168,64,188,168,182,159,182,168,182,168,182,160,188,168,172,152,182,160,172,156,
    36,120,20,120,20,128,236,88,228,144,236,144,84,128,64,128,68,168,84,120,84,128,64,128,84,128,84,120,96,128,124,128,116,112,124,120,116,128,116,112,124,128,92,255,100,224,100,248,
    16,32,8,0,16,8,92,255,100,248,100,224,16,32,16,8,8,0,60,112,60,72,40,72,40,112,40,72,60,112,36,112,20,120,36,120,60,112,40,72,60,72,32,72,20,120,36,112,
    40,112,60,112,40,72,40,112,60,120,60,112,252,160,244,144,244,160,156,16,172,8,156,8,156,40,172,48,172,32,156,48,172,56,172,48,180,32,172,24,172,32,36,168,36,120,24,168,
    16,120,4,168,0,128,4,168,16,120,0,128,36,168,24,168,36,120,40,120,40,112,60,120,40,120,44,168,60,120,64,128,84,128,68,168,84,168,68,168,84,128,84,120,64,128,64,120,
    60,168,44,168,60,120,188,120,192,152,192,120,192,120,192,152,188,120,172,115,181,112,172,120,172,120,164,119,172,115,172,120,182,112,172,116,188,112,182,104,188,104,188,104,182,104,188,112,
    164,120,172,120,164,119,184,96,168,64,184,64,188,64,184,96,184,64,164,120,163,128,163,119,168,64,160,72,160,64,184,64,184,96,188,64,168,64,184,96,184,64,160,72,168,64,160,64,
    164,128,164,120,163,128,164,152,163,128,162,152,164,152,163,128,164,128,188,160,182,159,188,168,172,152,182,159,188,160,188,168,182,160,188,160,188,160,182,160,172,152,192,255,204,255,192,240,
    192,200,168,200,192,216,168,255,192,255,168,240,168,216,160,216,168,240,168,216,168,240,192,216,168,200,192,200,168,176,192,255,192,240,204,255,192,200,192,216,168,200,168,255,168,240,192,255,
    168,216,168,240,160,216,168,216,192,216,168,240,168,200,168,176,192,200,234,16,224,16,234,0,224,48,224,16,234,48,104,88,116,112,104,112,234,48,224,16,234,16,216,16,224,16,216,48,
    224,64,224,48,234,64,116,88,124,120,124,88,116,88,104,88,116,112,124,88,124,64,116,88,216,16,216,48,224,16,124,64,116,88,116,64,234,64,234,48,224,48,234,0,224,0,224,16,
    234,16,234,0,224,16,224,48,234,48,224,16,234,48,234,16,224,16,224,64,234,64,224,48,84,120,84,88,96,120,104,88,96,88,104,112,96,88,84,88,96,120,104,88,104,112,96,88,
    180,56,188,56,180,48,172,24,156,24,172,32,180,56,180,48,172,56,156,40,152,40,156,48,172,32,172,48,180,32,172,24,180,24,172,8,172,24,172,8,156,16,156,48,172,48,156,40,
    156,56,172,56,156,48,180,24,172,24,180,32,236,144,244,144,236,88,204,64,204,16,196,56,212,16,204,16,212,64,196,16,204,16,196,0,196,80,199,80,196,64,200,80,201,80,200,64,
    204,64,196,56,201,64,188,56,196,56,188,16,199,80,200,80,199,64,201,80,204,80,201,64,208,96,216,96,208,104,224,96,224,136,216,96,196,136,196,104,204,136,204,152,204,136,216,152,
    204,96,196,96,204,88,196,104,196,96,204,104,204,104,208,104,204,136,208,96,208,80,216,96,208,184,208,216,204,184,204,184,204,216,200,184,200,184,200,216,196,184,204,224,200,224,204,216,
    200,176,204,176,200,184,64,128,84,120,64,120,40,120,60,120,40,112,180,48,188,48,188,56,172,32,156,32,156,24,172,56,172,48,180,48,156,48,152,48,152,40,180,32,180,48,172,48,
    172,8,180,8,180,24,236,88,244,80,244,144,236,72,236,88,228,88,244,144,244,80,252,80,156,16,156,8,172,8,156,40,172,32,172,48,156,48,172,48,172,56,180,32,172,32,172,24,
    180,56,180,48,188,56,172,24,172,32,156,24,180,56,172,56,180,48,156,40,156,48,152,40,172,32,180,32,172,48,172,24,172,8,180,24,172,24,156,16,172,8,156,48,156,40,172,48,
    156,56,156,48,172,56,180,24,180,32,172,24,236,144,236,88,244,144,228,72,236,72,228,88,252,144,244,144,252,80,236,72,228,88,236,88,244,144,252,80,244,80,228,88,228,144,236,88,
    252,144,244,144,252,160,228,72,228,88,236,72,252,144,252,80,244,144,236,88,236,144,228,144,252,160,244,160,244,144,228,88,236,88,228,144,252,144,252,160,244,144,
};
static const float kSoScale = 1.0f / 20000.0f;
// опорные точки скелета (метры, оси тела: x вправо, y вверх, z вперёд)
static const V3 kSoPivot[14] = {V3(0.0000f, 0.0000f, 0.0000f), V3(0.0000f, 1.6033f, -0.0433f), V3(0.0867f, 0.8667f, 0.0000f), V3(0.0867f, 0.5633f, 0.0000f), V3(0.0867f, 0.1300f, -0.0433f), V3(-0.0867f, 0.8667f, 0.0000f), V3(-0.0867f, 0.5633f, 0.0000f), V3(-0.0867f, 0.1300f, -0.0433f), V3(0.2167f, 1.4733f, 0.0000f), V3(0.4333f, 1.4733f, 0.0000f), V3(0.7367f, 1.4733f, 0.0433f), V3(-0.2167f, 1.4733f, 0.0000f), V3(-0.4333f, 1.4733f, 0.0000f), V3(-0.7367f, 1.4733f, 0.0433f)};
static const float kSoUA = 0.2167f, kSoLA = 0.3064f;
static const int kSoTexW = 64, kSoTexH = 32;
static const uint8_t kSoPal[87] = {
    15,15,15,18,18,18,19,68,110,24,24,24,28,28,28,31,110,179,32,32,32,33,29,30,
    33,33,33,34,34,34,36,36,36,42,42,42,43,43,43,51,51,51,53,53,53,59,59,59,
    76,76,76,85,85,85,86,86,86,92,92,92,106,110,107,107,98,75,107,107,107,112,112,112,
    126,138,88,137,137,137,152,152,152,179,177,172,207,175,124,
};
static const uint8_t kSoCls[29] = {
    3,3,4,3,3,4,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,1,2,2,
    1,2,2,2,0,
};
static const char* kSoTex =
    "B\?LLI77LLIIIIIIIIIIIIIII===I===I@@@@@@@@@@@@@@@JJ44JJJJJ444J4J;A<\?LLILLLLILHILLLLIIIIIII1==I==1I"
    "@@@@@@@K49<44@@JJ44JJJJJ4<9J4J6A8\?LLIEELLILIILLLLIIII==========I0@@@@@@K4444<@@444444J444<9J444A"
    "B\?LLILLLLILLLLLLLIIII7777777777I0@@@@@@K49<4<@@4<<<<4J4<444J4<4A7LLLIEELLILLLLLLLIIIILLLLLLLLLLI"
    "@@@@@@@K4444<@@499994J4<<99J<<4A7LEEILLLLIEEEELLLIIIIELEEEELEEEI@@@@@@DK49<44@@499994J44444J444A"
    "7LEEIEELLIEEEELLLIIIIELEEEELEEEI@@@@@@@K444444<444994JJJ444J4JJA7LLEILLLLIEEEELLLIIIILLLEELLLLLI"
    "@@@@@@@@@@@@@@@@J4444JJJ444J4JJA77LLI77LLILLLLLLLIIII77LLLL7777I@@@@@@@@49999999J44JJJJJJJJJJJJA"
    "L777IL777I77777III11I7777777777I@@@@@@@@4999GGG9J44JJJJJJ444JJJAL7LLIL7LLILLLLLIII==I7777777777I"
    "@@@@@@@@4499CCC9JJJJ44JJJ<<4444AL77LIL77LIEEEEEIII==ILLLLLLLLLLI@@@@@@@@@4444444J44J44JJJ9<4444A"
    "LL7LILL7LILLLLLIII==IEEELLLLLLLI@@@@@@@@@;;;;;;JJ44J4444J994444AEL77IEL77I77777III==IEEEL77777L3"
    "@@@@@@@@@4444<<JJ4444<<4J994444A7777I7777I77777IIIIIILLLL77777L7@@@@@@@@@4<<<<<<J4<<<994J994444A"
    "7777I7777I77777I77777EEEL77777LL@@@@@@@@@4<<<<<<J4999994J994444A77LLI77LLILLLLLI77777I7777II777L"
    "@@@@@@@@@4999CG9J4444444J<<4444ALLLEILLLEIEEEEEILLLLLI7777II777=@@@@@@@@@4499999JJJ444JJJ444444A"
    "ELEEIELEEIEEEEEIEEEEEI====II===5@@@@@@@@@@444499JJJ444JJJJJJJ44ALLLLIELELILLLLLILLLLEI1=111I1=15"
    "@@@@@@@@@@@4449@JJJJJJJJJJJJJ44A7777ILLLLI777LLI777LLI777IIIIIIL@@@@@@@@@@@4449@JJJJ;J49<499JJJA"
    "I777I:777I77777I77777I777IF:FILL@@@@@@@@@@@@@@@@JJJJ>J444444JJJAI777II777I77777I77777I===I:\?:ILE"
    "@@@@@@@@@@49<49<JJ6JJJ499<<4JJJAI777II777I77777I77777I111IF:FILE@@@@@@@@@@444444J666JJ4444<4JJJA"
    "I777IILLLI777LLI777LLIIIII:\?:ILE@@@@@@@@@@499994J;;;JD499<<4JJJAILLLIIEEEILLLLEILLLLEIIIIIF:FILE"
    "@@@@@@@@@@444444J;;;JK444444JJJAIEEEIIEEEIELEEEIELEEEIIIILLLLLLE@@@@@@@@@@4<99<4J666JD49<49<JJJA"
    "IEEEIIEEEIELLEEIELLEEI7LE55==LLL@@@@@@@@444<99<4JJ6JJJ49949<<44AIEEEIIEEEIEELEEIEELEEI7LE2======"
    "@@@@@@@@99444444JJJJJJJJJJJJJJJAIEEEIILLLIEELLLIEELLLI7LE==LLLLL@@@@@@@@4449<49<@@@@@@@@@@@@@@@@"
    "ILLLIIEEEILLLEEILLLEEI7LEL77777L@@@@@@@@@@49<49<<47@@@@@@@@@@@@@I777IILLLI77LLLI77LLLI7LE7777777"
    "@@@@@@@@@@49<49<<44@@@@@@@@@@@@@"
;

// =====================================================================
//  СЕТОЧНЫЕ МОДЕЛИ: АК-12, граната, солдат (скелетные сегменты)
// =====================================================================
struct RMesh { std::vector<V3> p, fn, cn; std::vector<uint16_t> t; std::vector<uint32_t> col; };
static RMesh g_mAk, g_mAkL, g_mGr, g_mGrH;
static int g_akMag0 = 0, g_akMag1 = 0;
static float g_akSightY = 0.05f, g_akMuzzleZ = 0.42f;
// солдат: локальные координаты вершин в системе своей кости, группа вершины, uv углов треугольников
enum { SO_VAR = 4 };
static std::vector<V3> g_soL; static std::vector<uint8_t> g_soG; static std::vector<uint16_t> g_soT; static std::vector<float> g_soUV;
static std::vector<uint32_t> g_soTriCol[SO_VAR];          // средний цвет треугольника (программный рендер)
enum { ATL_W = 128, ATL_H = 256, GRASS_ROW = 128, GRASS_SZ = 128 };
static std::vector<uint8_t> g_soAtlas;                    // RGBA 64x128: варианты формы солдат по 32 строки
static bool g_meshReady = false;

// нормали граней и сглаженные нормали углов (автосглаживание по углу)
static void meshFinish(RMesh& m, float smoothCos) {
    size_t nt = m.t.size() / 3;
    m.fn.assign(nt, V3(0, 1, 0)); m.cn.assign(nt * 3, V3(0, 1, 0));
    std::vector<V3> area(nt);
    std::vector<std::vector<int>> adj(m.p.size());
    for (size_t i = 0; i < nt; i++) {
        V3 a = m.p[m.t[i * 3]], b = m.p[m.t[i * 3 + 1]], c = m.p[m.t[i * 3 + 2]];
        V3 n = cross(b - a, c - a); area[i] = n;
        float l = len(n); m.fn[i] = l > 1e-12f ? n * (1.0f / l) : V3(0, 1, 0);
        for (int k = 0; k < 3; k++) adj[m.t[i * 3 + k]].push_back((int)i);
    }
    for (size_t i = 0; i < nt; i++)
        for (int k = 0; k < 3; k++) {
            V3 s(0, 0, 0);
            for (int j : adj[m.t[i * 3 + k]]) if (dot(m.fn[j], m.fn[i]) >= smoothCos) s = s + area[j];
            float l = len(s); m.cn[i * 3 + k] = l > 1e-12f ? s * (1.0f / l) : m.fn[i];
        }
}
static void meshLoad(RMesh& m, const int16_t* v, int nv3, float sc, const uint16_t* t, int nt3) {
    m.p.resize(nv3 / 3);
    for (int i = 0; i < nv3 / 3; i++) m.p[i] = V3(v[i * 3] * sc, v[i * 3 + 1] * sc, v[i * 3 + 2] * sc);
    m.t.assign(t, t + nt3);
}
static uint32_t akCol(int g) {   // металл с лёгким холодным оттенком, полимер чуть теплее
    g = clampi((int)(g * 0.9f), 0, 255);
    return RGBc(g, clampi(g + 1, 0, 255), clampi(g + 4, 0, 255));
}
static void soVariantCols(int var, uint8_t* out /*5 классов * 3*/) {
    // светлый камуфляж, тёмный камуфляж, снаряжение (серое), чёрное, визор
    static const uint8_t T[SO_VAR][15] = {
        {128, 136, 92,  84, 92, 62,   92, 98, 76,   42, 43, 39,  52, 104, 150},   // «Гром»
        {128, 136, 92,  84, 92, 62,   104, 100, 90, 42, 43, 39,  52, 104, 150},   // медик
        {96, 98, 106,   60, 62, 68,   78, 78, 82,   34, 34, 37,  170, 52, 32},    // синдикат
        {108, 124, 70,  76, 92, 52,   86, 98, 64,   40, 42, 36,  60, 96, 64}};    // «Сова»
    memcpy(out, T[var], 15);
}
static void meshInit() {
    if (g_meshReady) return;
    g_meshReady = true;
    // ---- АК-12 (вид от первого лица)
    meshLoad(g_mAk, kAkV, (int)(sizeof(kAkV) / sizeof(kAkV[0])), kAkScale, kAkT, (int)(sizeof(kAkT) / sizeof(kAkT[0])));
    for (size_t i = 0; i < g_mAk.t.size() / 3; i++) g_mAk.col.push_back(akCol(kAkPal[kAkM[i]]));
    g_akMag0 = kAkPart[2]; g_akMag1 = kAkPart[3];
    meshFinish(g_mAk, 0.82f);
    // высота мушки и дульный срез
    float sy = -1, mz = -1;
    for (int i = kAkPart[1]; i < kAkPart[2]; i++) for (int k = 0; k < 3; k++) { V3 p = g_mAk.p[g_mAk.t[i * 3 + k]]; if (p.z > 0.2f) sy = std::max(sy, p.y); }
    for (const V3& p : g_mAk.p) mz = std::max(mz, p.z);
    if (sy > 0) g_akSightY = sy;
    if (mz > 0) g_akMuzzleZ = mz;
    // ---- АК-12, упрощённая для NPC
    meshLoad(g_mAkL, kAkLV, (int)(sizeof(kAkLV) / sizeof(kAkLV[0])), kAkScale, kAkLT, (int)(sizeof(kAkLT) / sizeof(kAkLT[0])));
    for (size_t i = 0; i < g_mAkL.t.size() / 3; i++) g_mAkL.col.push_back(akCol(kAkPal[kAkLM[i]]));
    meshFinish(g_mAkL, 0.8f);
    // ---- граната (две детализации): цвета вершин → цвет треугольника
    auto loadGr = [](RMesh& m, const int16_t* v, int nv3, const uint16_t* t, int nt3, const uint8_t* vc) {
        meshLoad(m, v, nv3, kGrScale, t, nt3);
        for (size_t i = 0; i < m.t.size() / 3; i++) {
            int r = 0, g = 0, b = 0;
            for (int k = 0; k < 3; k++) { int j = m.t[i * 3 + k]; r += vc[j * 3]; g += vc[j * 3 + 1]; b += vc[j * 3 + 2]; }
            m.col.push_back(RGBc(r / 3, g / 3, b / 3));
        }
        meshFinish(m, 0.7f);
    };
    loadGr(g_mGr, kGrV, (int)(sizeof(kGrV) / sizeof(kGrV[0])), kGrT, (int)(sizeof(kGrT) / sizeof(kGrT[0])), kGrC);
    loadGr(g_mGrH, kGrHV, (int)(sizeof(kGrHV) / sizeof(kGrHV[0])), kGrHT, (int)(sizeof(kGrHT) / sizeof(kGrHT[0])), kGrHC);
    // ---- солдат
    int nv = (int)(sizeof(kSoG) / sizeof(kSoG[0]));
    g_soL.resize(nv); g_soG.assign(kSoG, kSoG + nv);
    for (int i = 0; i < nv; i++) g_soL[i] = V3(kSoV[i * 3] * kSoScale, kSoV[i * 3 + 1] * kSoScale, kSoV[i * 3 + 2] * kSoScale);
    g_soT.assign(kSoT, kSoT + sizeof(kSoT) / sizeof(kSoT[0]));
    int nt = (int)g_soT.size() / 3;
    g_soUV.resize(nt * 6);
    for (int i = 0; i < nt * 6; i++) g_soUV[i] = (kSoUV[i] + 0.5f) / 256.0f;
    // пучок травы 64x64: травинки с переходом от тёмного корня к светлому кончику, прозрачный фон
    g_soAtlas.assign((size_t)ATL_W * ATL_H * 4, 0);
    {
        uint32_t s = 0x9E3779B9u; auto rr = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFF) / 65535.0f; };
        for (int b = 0; b < 90; b++) {
            float x0 = 4 + rr() * 120, lean = (rr() - 0.5f) * 52, hgt = 0.45f + rr() * 0.55f, w = 1.6f + rr() * 2.2f, curve = (rr() - 0.3f) * 1.6f;
            float tone = 0.85f + rr() * 0.3f; bool dry = rr() < 0.18f;
            for (int y = 0; y < GRASS_SZ; y++) {
                float t = (GRASS_SZ - 1 - y) / (float)(GRASS_SZ - 1);              // 0 — корень (низ), 1 — верх
                if (t > hgt) continue;
                float k = t / hgt;
                float cx = x0 + lean * k * k * (1 + curve * k) * hgt;
                float half = w * (1 - k * 0.92f);
                for (int x = (int)floorf(cx - half - 1); x <= (int)ceilf(cx + half + 1); x++) {
                    if (x < 0 || x > GRASS_SZ - 1) continue;
                    float cov = clampf(half + 0.5f - fabsf(x + 0.5f - cx), 0, 1);
                    if (cov < 0.35f) continue;
                    uint8_t* d = &g_soAtlas[(((size_t)GRASS_ROW + y) * ATL_W + x) * 4];
                    float lum = (0.42f + 0.58f * k) * tone;
                    float r = lum * (dry ? 1.12f : 0.96f), g = lum, bl = lum * (dry ? 0.7f : 0.82f);
                    if (k > 0.8f) { r += 0.05f; bl -= 0.03f; }
                    d[0] = (uint8_t)clampi((int)(r * 230), 0, 255); d[1] = (uint8_t)clampi((int)(g * 230), 0, 255); d[2] = (uint8_t)clampi((int)(bl * 230), 0, 255); d[3] = 255;
                }
            }
        }
    }
    // атлас: по варианту формы на каждые kSoTexH строк
    int W = kSoTexW, H = kSoTexH;
    for (int var = 0; var < SO_VAR; var++) {
        uint8_t vc[15]; soVariantCols(var, vc);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            int pi = kSoTex[y * W + x] - 48;
            const uint8_t* src = &kSoPal[pi * 3];
            int cls = kSoCls[pi];
            float l = (src[0] + src[1] + src[2]) / 3.0f;
            float ref[5] = {169.0f, 93.0f, 100.0f, 32.0f, 110.0f};
            float k = clampf(l / ref[cls], 0.6f, 1.5f);
            uint8_t* d = &g_soAtlas[(((size_t)var * H + y) * ATL_W + x) * 4];
            for (int c = 0; c < 3; c++) d[c] = (uint8_t)clampi((int)(vc[cls * 3 + c] * k), 0, 255);
            d[3] = 255;
        }
        g_soTriCol[var].resize(nt);
        for (int i = 0; i < nt; i++) {
            float acc[3] = {0, 0, 0}; int n = 0;
            for (int s = 0; s < 7; s++) {   // выборка по треугольнику: вершины, середины рёбер, центр
                static const float w[7][3] = {{0.8f, 0.1f, 0.1f}, {0.1f, 0.8f, 0.1f}, {0.1f, 0.1f, 0.8f}, {0.45f, 0.45f, 0.1f}, {0.1f, 0.45f, 0.45f}, {0.45f, 0.1f, 0.45f}, {0.34f, 0.33f, 0.33f}};
                float u = 0, v = 0;
                for (int k = 0; k < 3; k++) { u += g_soUV[i * 6 + k * 2] * w[s][k]; v += g_soUV[i * 6 + k * 2 + 1] * w[s][k]; }
                int tx = clampi((int)(u * W), 0, W - 1), ty = clampi((int)(v * H), 0, H - 1);   // u,v — в пределах своей текстуры
                const uint8_t* d = &g_soAtlas[(((size_t)var * H + ty) * ATL_W + tx) * 4];
                for (int c = 0; c < 3; c++) acc[c] += d[c];
                n++;
            }
            g_soTriCol[var][i] = RGBc((int)(acc[0] / n), (int)(acc[1] / n), (int)(acc[2] / n));
        }
    }
}

// вывод треугольников сетки: GL — освещаемые вершины с гладкими нормалями; программно — плоское затенение
static void drawMesh(const RMesh& m, int t0, int t1, const Basis& B, V3 org, bool cam, float scale = 1.0f, int skip0 = -1, int skip1 = -1) {
    t1 = std::min(t1, (int)(m.t.size() / 3));
    if (g_useGL) {
        Basis W = B; V3 o = org;
        if (cam) { o = camToWorld(org); W.r = camDirToWorld(B.r); W.u = camDirToWorld(B.u); W.f = camDirToWorld(B.f); }
        std::vector<GV>& b = glBatch(RM_OPAQUE);
        b.reserve(b.size() + (size_t)(t1 - t0) * 3);
        for (int i = t0; i < t1; i++) {
            if (i >= skip0 && i < skip1) continue;
            uint32_t c = glCol(m.col[i], 1.0f);
            for (int k = 0; k < 3; k++) glPush(b, o + bx(W, m.p[m.t[i * 3 + k]] * scale), bx(W, m.cn[i * 3 + k]), c, 0, 0, 0, -1000, 0);
        }
        return;
    }
    static const V3 kL = norm(V3(0.4f, 0.8f, -0.45f));
    for (int i = t0; i < t1; i++) {
        if (i >= skip0 && i < skip1) continue;
        V3 w[3];
        for (int k = 0; k < 3; k++) w[k] = org + bx(B, m.p[m.t[i * 3 + k]] * scale);
        V3 n = bx(B, m.fn[i]);
        V3 eye = cam ? V3(0, 0, 0) : g_cam.pos;
        if (dot(n, eye - w[0]) <= 0) continue;
        float s = cam ? g_env.vmLight * (0.55f + 0.45f * std::max(0.0f, dot(n, kL))) : faceShade(n);
        V3 q[3];
        for (int k = 0; k < 3; k++) q[k] = cam ? w[k] : toCam(w[k]);
        drawPolyCam(q, 3, shadeC(m.col[i], s), RM_OPAQUE);
    }
}

// ---- солдат: позы сегментов и двухзвенная ИК рук
enum { SG_TORSO, SG_HEAD, SG_THR, SG_CALR, SG_FOOTR, SG_THL, SG_CALL, SG_FOOTL, SG_UAR, SG_LAR, SG_HANDR, SG_UAL, SG_LAL, SG_HANDL, SG_N };
struct SoPose { Basis b[SG_N]; V3 o[SG_N]; float sx[SG_N]; };
static int soVariant(int uni) {
    switch (uni) { case U_GROM: return 0; case U_DOC: return 1; case U_SYND: return 2; case U_SOVA: return 3; default: return -1; }
}
// поворот ноги вперёд на угол a (положительный — вперёд)
static Basis legRot(const Basis& B, float a) { return bmul(B, rotX(-a)); }
static void soLeg(SoPose& P, const Basis& B, V3 org, float lo, int side, float thA, float caA, float ankleH) {
    int th = side > 0 ? SG_THR : SG_THL, ca = th + 1, ft = th + 2;
    V3 hipL = kSoPivot[th] + V3(0, lo, 0);
    Basis T = legRot(B, thA);
    V3 hip = org + bx(B, hipL);
    V3 kneeRel = kSoPivot[ca] - kSoPivot[th];
    V3 knee = hip + bx(T, kneeRel);
    V3 ankRel = kSoPivot[ft] - kSoPivot[ca];
    if (ankleH > -50) {   // голень подбирается так, чтобы стопа оказалась на нужной высоте
        float kneeY = hipL.y + kneeRel.y * cosf(thA) + kneeRel.z * sinf(thA);
        float L = len(ankRel);
        caA = -acosf(clampf((kneeY - ankleH) / std::max(0.05f, L), -1, 1));
    }
    Basis C = legRot(B, caA);
    V3 ank = knee + bx(C, ankRel);
    P.b[th] = T; P.o[th] = hip; P.b[ca] = C; P.o[ca] = knee;
    P.b[ft] = legRot(B, std::max(caA, -0.4f) * 0.4f); P.o[ft] = ank;
}
static void soArm(SoPose& P, const Basis& B, V3 org, float lo, int side, V3 target) {
    int ua = side > 0 ? SG_UAR : SG_UAL, la = ua + 1, hd = ua + 2;
    V3 S = org + bx(B, kSoPivot[ua] + V3(0, lo, 0));
    V3 pole = norm(bx(B, V3(0.55f * side, -1.0f, -0.35f)));
    V3 d = target - S; float dl = len(d);
    float a = kSoUA, b = kSoLA, st = 1;
    float reach = (a + b) * 0.985f;
    if (dl > reach) { st = std::min(1.18f, dl / reach); a *= st; b *= st; }
    V3 dir = dl > 1e-4f ? d * (1.0f / dl) : bx(B, V3(0, 0, 1));
    float dd = std::min(dl, (a + b) * 0.999f);
    float x = (a * a - b * b + dd * dd) / (2 * std::max(1e-3f, dd));
    float h = sqrtf(std::max(0.0f, a * a - x * x));
    V3 perp = pole - dir * dot(pole, dir); float pl = len(perp);
    perp = pl > 1e-4f ? perp * (1.0f / pl) : bx(B, V3(0, -1, 0));
    V3 E = S + dir * x + perp * h;
    V3 Wr = S + dir * dd;
    auto frame = [&](V3 X, Basis& o) {
        X = norm(X); V3 Z = pole - X * dot(pole, X); float zl = len(Z);
        Z = zl > 1e-4f ? Z * (1.0f / zl) : perp;
        o.r = X; o.f = Z; o.u = cross(Z, X);
    };
    frame(E - S, P.b[ua]); P.o[ua] = S; P.sx[ua] = st;
    frame(Wr - E, P.b[la]); P.o[la] = E; P.sx[la] = st;
    P.b[hd] = P.b[la]; P.o[hd] = Wr; P.sx[hd] = 1;
}
static void drawSoldierMesh(const SoPose& P, int var, bool flash) {
    meshInit();
    static std::vector<V3> wp; wp.resize(g_soL.size());
    for (size_t i = 0; i < g_soL.size(); i++) {
        int g = g_soG[i]; V3 l = g_soL[i]; l.x *= P.sx[g];
        const Basis& b = P.b[g];
        wp[i] = P.o[g] + b.r * l.x + b.u * l.y + b.f * l.z;
    }
    int nt = (int)g_soT.size() / 3;
    float vh = (float)kSoTexH / ATL_H;
    if (g_useGL) {
        std::vector<GV>& B = glBatch(RM_OPAQUE);
        uint32_t c = flash ? glCol(RGBc(235, 225, 215), 1) : glCol(0xFFFFFFFFu, 1);
        for (int i = 0; i < nt; i++) {
            V3 a = wp[g_soT[i * 3]], b = wp[g_soT[i * 3 + 1]], cc = wp[g_soT[i * 3 + 2]];
            V3 n = cross(b - a, cc - a); float l = len(n); if (l < 1e-10f) continue; n = n * (1.0f / l);
            V3 v3[3] = {a, b, cc};
            for (int k = 0; k < 3; k++)
                glPush(B, v3[k], n, c, g_soUV[i * 6 + k * 2] * ((float)kSoTexW / ATL_W), (var + g_soUV[i * 6 + k * 2 + 1]) * vh, flash ? 0.0f : 20.0f, -1000, 0);
        }
        return;
    }
    for (int i = 0; i < nt; i++) {
        V3 a = wp[g_soT[i * 3]], b = wp[g_soT[i * 3 + 1]], cc = wp[g_soT[i * 3 + 2]];
        V3 n = cross(b - a, cc - a); float l = len(n); if (l < 1e-10f) continue; n = n * (1.0f / l);
        if (dot(n, g_cam.pos - a) <= 0) continue;
        V3 q[3] = {toCam(a), toCam(b), toCam(cc)};
        drawPolyCam(q, 3, shadeC(flash ? RGBc(235, 225, 215) : g_soTriCol[var][i], faceShade(n)), RM_OPAQUE);
    }
}
// оружие NPC в виде упрощённого АК-12; A — базис рук, sh — точка плеча
static const V3 kNpcAkOff(0.08f, -0.035f, 0.5f);
static void drawNpcAk(const Basis& A, V3 sh) {
    meshInit();
    drawMesh(g_mAkL, 0, (int)(g_mAkL.t.size() / 3), A, sh + bx(A, kNpcAkOff), false);
}
// граната Mk2 (масштаб s), центр c
static void drawGrenadeMesh(V3 c, const Basis& B, bool cam) {
    meshInit();
    const RMesh& m = cam ? g_mGrH : g_mGr;
    drawMesh(m, 0, (int)(m.t.size() / 3), B, c, cam);
}

// =====================================================================
//  ОТРИСОВКА СЦЕНЫ
// =====================================================================
static void drawShadow(V3 p, float r) {
    V3 c = p + V3(0, 0.03f, 0);
    if (!sphereVis(c, r)) return;
    V3 pts[8];
    for (int i = 0; i < 8; i++) { float a = i * PI_F / 4; pts[i] = c + V3(cosf(a) * r, 0, sinf(a) * r); }
    V3 cp[8];
    for (int i = 0; i < 8; i++) cp[i] = toCam(pts[i]);
    drawPolyCam(cp, 8, 0, RM_SHADOW);
}
struct UniCol { uint32_t torso, legs, head, vest, skin; };
static UniCol uniCol(int u) {
    UniCol c;
    c.skin = RGBc(185, 145, 115);
    switch (u) {
    case U_GROM: c.torso = RGBc(92, 100, 66); c.legs = RGBc(78, 84, 58); c.head = RGBc(70, 76, 52); c.vest = RGBc(60, 64, 44); break;
    case U_DOC: c.torso = RGBc(92, 100, 66); c.legs = RGBc(78, 84, 58); c.head = RGBc(70, 76, 52); c.vest = RGBc(64, 68, 46); break;
    case U_SYND: c.torso = RGBc(48, 48, 54); c.legs = RGBc(58, 58, 64); c.head = RGBc(28, 28, 30); c.vest = RGBc(36, 38, 40); break;
    case U_MILITIA: c.torso = RGBc(150, 130, 92); c.legs = RGBc(108, 98, 70); c.head = RGBc(125, 32, 30); c.vest = RGBc(90, 80, 55); break;
    case U_RAVEN: c.torso = RGBc(22, 22, 26); c.legs = RGBc(26, 26, 30); c.head = RGBc(35, 30, 28); c.vest = RGBc(18, 18, 20); break;
    case U_SOVA: c.torso = RGBc(84, 98, 58); c.legs = RGBc(74, 88, 50); c.head = RGBc(90, 104, 60); c.vest = RGBc(66, 80, 44); break;
    default: c.torso = RGBc(170, 160, 140); c.legs = RGBc(60, 60, 90); c.head = RGBc(40, 30, 25); c.vest = RGBc(140, 60, 50); break;
    }
    return c;
}
// оружие в руках NPC (в базисе рук A от точки плеча sh)
static void drawNpcGun(const Basis& A, V3 sh, int weapon, bool detail) {
    if (weapon == W_AR && (detail || g_useGL)) { drawNpcAk(A, sh); return; }
    uint32_t m = RGBc(34, 34, 32), d = RGBc(22, 22, 22), f = RGBc(58, 62, 46), w = RGBc(110, 80, 52);
    if (weapon == W_PISTOL) {
        drawPart(A, sh, V3(0.05f, -0.02f, 0.5f), V3(0.018f, 0.02f, 0.09f), m);
        drawPart(A, sh, V3(0.05f, -0.07f, 0.45f), V3(0.016f, 0.045f, 0.022f), d);
        return;
    }
    if (weapon == W_SNIPER) {
        drawPart(A, sh, V3(0.08f, -0.03f, 0.3f), V3(0.032f, 0.045f, 0.3f), w);
        drawPart(A, sh, V3(0.08f, -0.01f, 0.85f), V3(0.014f, 0.014f, 0.3f), d);
        drawPart(A, sh, V3(0.08f, 0.06f, 0.35f), V3(0.025f, 0.025f, 0.16f), d);
        return;
    }
    drawPart(A, sh, V3(0.08f, -0.03f, 0.36f), V3(0.032f, 0.05f, 0.2f), m);          // ствольная коробка
    drawPart(A, sh, V3(0.08f, -0.03f, 0.64f), V3(0.035f, 0.042f, 0.1f), f);         // цевьё
    drawPart(A, sh, V3(0.08f, -0.02f, 0.84f), V3(0.012f, 0.012f, 0.11f), d);        // ствол
    drawPart(A, sh, V3(0.08f, -0.03f, 0.07f), V3(0.026f, 0.055f, 0.11f), f);        // приклад
    drawPart(A, sh, V3(0.08f, -0.15f, 0.44f), V3(0.02f, 0.075f, 0.035f), d);        // магазин
    if (detail) {
        drawPart(A, sh, V3(0.08f, 0.03f, 0.38f), V3(0.016f, 0.012f, 0.05f), d);     // прицел
        drawPart(A, sh, V3(0.08f, -0.11f, 0.3f), V3(0.016f, 0.045f, 0.02f), d);     // рукоять
        drawPart(A, sh, V3(0.08f, -0.02f, 0.96f), V3(0.018f, 0.018f, 0.025f), d);   // ДТК
    }
}
static void drawHuman(const Actor& a) {
    float rad = 2.2f;
    if (!sphereVis(a.pos + V3(0, 0.9f, 0), rad)) return;
    bool detail = len(a.pos - g_cam.pos) < 24.0f;
    UniCol C = uniCol(a.uni);
    if (a.hitFlash > 0) { C.torso = C.legs = C.vest = RGBc(230, 220, 210); }
    uint32_t boot = RGBc(34, 30, 26), glove = RGBc(38, 38, 34), gear = shadeC(C.vest, 0.82f), strap = shadeC(C.vest, 0.65f);
    Basis B = basisYPR(a.yaw, 0, 0);
    V3 org = a.pos;
    float fall = 0;
    bool prone = a.stance == 2 || a.state == AS_DOWN;
    if (!a.alive) fall = -std::min(1.0f, a.deadT / 0.45f) * PI_F * 0.5f;
    if (!a.alive) { B = basisYPR(a.fallDir + PI_F, 0, 0); B = bmul(B, rotX(fall)); org = a.pos + V3(0, 0.05f, 0); }
    else if (prone) { B = bmul(B, rotX(PI_F * 0.5f)); org = a.pos - V3(sinf(a.yaw), 0, cosf(a.yaw)) * 0.85f + V3(0, 0.12f, 0); }
    bool crouch = a.alive && a.stance == 1 && !prone;
    float sw = (a.alive && !prone) ? sinf(a.walkPh) * 0.55f * std::min(1.0f, a.speedNow / 3.0f) : 0.0f;
    float lo = crouch ? -0.5f : 0.0f;
    bool armored = a.uni == U_GROM || a.uni == U_DOC || a.uni == U_SYND || a.uni == U_SOVA;
    int var = soVariant(a.uni);
    if (var >= 0 && (a.kind == AK_SOLDIER || a.kind == AK_ALLY)) {
        // ---- сеточная модель солдата: сегменты по костям, ноги и руки в позе
        SoPose P;
        float mlo = crouch ? -0.36f : 0.0f;
        for (int i = 0; i < SG_N; i++) { P.b[i] = B; P.o[i] = org; P.sx[i] = 1; }
        P.o[SG_TORSO] = org + bx(B, V3(0, mlo, 0));
        P.b[SG_HEAD] = bmul(B, rotX(a.alive ? -a.aimPitch * 0.5f : 0)); P.o[SG_HEAD] = org + bx(B, kSoPivot[SG_HEAD] + V3(0, mlo, 0));
        if (crouch) { soLeg(P, B, org, mlo, 1, 1.35f, 0, 0.13f); soLeg(P, B, org, mlo, -1, 0.2f, 0, 0.09f); }
        else for (int s = -1; s <= 1; s += 2) { float th = sw * s; soLeg(P, B, org, mlo, s, th, th < 0 ? th * 1.8f : th * 0.4f, -100); }
        Basis A = bmul(B, rotX(a.alive ? -a.aimPitch : 0));
        V3 sh = org + bx(B, V3(0, 1.40f + mlo, 0));
        bool pistol = a.weapon == W_PISTOL;
        V3 hr = sh + bx(A, pistol ? V3(0.05f, -0.08f, 0.42f) : V3(0.08f, -0.1f, 0.24f));
        V3 hl = sh + bx(A, pistol ? V3(0.01f, -0.09f, 0.4f) : V3(0.06f, -0.07f, 0.52f));
        soArm(P, B, org, mlo, 1, hr); soArm(P, B, org, mlo, -1, hl);
        drawSoldierMesh(P, var, a.hitFlash > 0);
        drawNpcGun(A, sh, a.weapon, detail);
        if (a.uni == U_DOC) {
            drawPart(B, org, V3(0, 1.28f + mlo, -0.2f), V3(0.15f, 0.19f, 0.08f), RGBc(150, 40, 40));
            if (detail) { drawPart(B, org, V3(0, 1.3f + mlo, -0.285f), V3(0.07f, 0.02f, 0.005f), RGBc(235, 235, 235)); drawPart(B, org, V3(0, 1.3f + mlo, -0.285f), V3(0.02f, 0.07f, 0.005f), RGBc(235, 235, 235)); }
        }
        if (a.uni == U_SOVA) for (int k = 0; k < 5; k++) drawPart(B, org, V3(-0.2f + k * 0.1f, 1.5f + mlo + (k & 1) * 0.06f, -0.12f), V3(0.045f, 0.12f, 0.05f), RGBc(90 + k * 4, 108, 62));
        if (detail && !strcmp(a.name, "Седой")) drawPart(B, org, V3(-0.1f, 1.75f + mlo, -0.2f), V3(0.006f, 0.3f, 0.006f), RGBc(20, 20, 20));
        if (a.muzzle > 0 && a.alive) drawSprite(sh + bx(A, V3(0.08f, -0.03f, pistol ? 0.62f : (a.weapon == W_AR ? 0.95f : 1.0f))), 0.35f, RGBc(255, 200, 110), RM_ADD);
        if (a.alive) drawShadow(a.pos, 0.45f);
        return;
    }
    // ---- ноги: бедро, голень с наколенником, ботинок
    for (int s = -1; s <= 1; s += 2) {
        if (crouch) {
            Basis T = bmul(B, rotX(s < 0 ? 1.3f : 1.45f));
            V3 hip = org + bx(B, V3(0.11f * s, 0.48f, 0));
            drawPart(T, hip, V3(0, -0.22f, 0), V3(0.085f, 0.22f, 0.095f), C.legs);
            V3 knee = hip + bx(T, V3(0, -0.44f, 0));
            drawPart(B, knee, V3(0, -0.2f, -0.03f), V3(0.075f, 0.22f, 0.08f), C.legs);
            drawPart(B, knee, V3(0, -0.43f, 0.02f), V3(0.08f, 0.05f, 0.13f), boot);
            if (detail && armored) drawPart(B, knee, V3(0, 0.0f, 0.05f), V3(0.07f, 0.06f, 0.05f), gear);
        } else {
            Basis L = bmul(B, rotX(sw * s));
            V3 hip = org + bx(B, V3(0.11f * s, 0.9f, 0));
            drawPart(L, hip, V3(0, -0.22f, 0), V3(0.09f, 0.23f, 0.1f), C.legs);
            drawPart(L, hip, V3(0, -0.62f, 0), V3(0.075f, 0.2f, 0.085f), C.legs);
            drawPart(L, hip, V3(0, -0.85f, 0.04f), V3(0.08f, 0.055f, 0.14f), boot);
            if (detail && armored) drawPart(L, hip, V3(0, -0.44f, 0.08f), V3(0.07f, 0.06f, 0.035f), gear);
            if (detail && s > 0 && (a.uni == U_GROM || a.uni == U_SYND)) drawPart(L, hip, V3(0.1f, -0.2f, 0), V3(0.03f, 0.08f, 0.07f), gear); // набедренная кобура
        }
    }
    // ---- корпус
    if (a.uni == U_RAVEN) {
        drawPart(B, org, V3(0, 0.95f + lo, 0), V3(0.25f, 0.55f, 0.16f), C.torso);              // длинное пальто
        drawPart(B, org, V3(0, 1.5f + lo, 0.06f), V3(0.13f, 0.06f, 0.13f), RGBc(200, 200, 195)); // ворот рубашки
    }
    drawPart(B, org, V3(0, 0.94f + lo, 0), V3(0.2f, 0.06f, 0.12f), strap);                       // таз/ремень
    drawPart(B, org, V3(0, 1.24f + lo, 0), V3(0.2f, 0.26f, 0.12f), C.torso);                      // торс
    drawPart(B, org, V3(0, 1.41f + lo, 0), V3(0.25f, 0.08f, 0.13f), C.torso);                     // плечи
    if (armored || a.uni == U_MILITIA) {
        drawPart(B, org, V3(0, 1.26f + lo, 0.02f), V3(0.215f, 0.19f, 0.15f), C.vest);             // плитник
        if (detail) {
            for (int k = -1; k <= 1; k++) drawPart(B, org, V3(k * 0.1f, 1.14f + lo, 0.18f), V3(0.042f, 0.075f, 0.035f), gear); // подсумки
            drawPart(B, org, V3(-0.14f, 1.34f + lo, 0.17f), V3(0.035f, 0.05f, 0.025f), gear);        // рация
            drawPart(B, org, V3(0.15f, 1.38f + lo, 0.165f), V3(0.03f, 0.04f, 0.02f), RGBc(30, 30, 30)); // фонарь/турникет
        }
    }
    if (a.uni == U_DOC) drawPart(B, org, V3(0, 1.3f + lo, -0.16f), V3(0.16f, 0.2f, 0.08f), RGBc(150, 40, 40)); // медрюкзак
    else if (armored && detail) drawPart(B, org, V3(0, 1.26f + lo, -0.16f), V3(0.15f, 0.18f, 0.06f), gear); // гидратор/рюкзак
    if (a.uni == U_DOC) drawPart(B, org, V3(-0.25f, 1.3f + lo, 0.0f), V3(0.07f, 0.04f, 0.075f), RGBc(235, 235, 235)); // нарукавная повязка
    if (a.uni == U_DOC && detail) { drawPart(B, org, V3(0, 1.32f + lo, -0.245f), V3(0.07f, 0.02f, 0.005f), RGBc(235, 235, 235)); drawPart(B, org, V3(0, 1.32f + lo, -0.245f), V3(0.02f, 0.07f, 0.005f), RGBc(235, 235, 235)); }
    if (a.uni == U_SOVA) for (int k = 0; k < 5; k++) drawPart(B, org, V3(-0.2f + k * 0.1f, 1.48f + lo + (k & 1) * 0.06f, -0.05f), V3(0.045f, 0.12f, 0.05f), RGBc(90 + k * 4, 108, 62));
    if (detail && !strcmp(a.name, "Седой")) drawPart(B, org, V3(-0.1f, 1.75f + lo, -0.17f), V3(0.006f, 0.3f, 0.006f), RGBc(20, 20, 20)); // антенна
    // ---- шея и голова
    drawPart(B, org, V3(0, 1.52f + lo, 0), V3(0.055f, 0.04f, 0.055f), a.uni == U_SYND ? RGBc(28, 28, 30) : C.skin);
    uint32_t face = a.uni == U_SYND ? RGBc(30, 30, 32) : C.skin;
    drawPart(B, org, V3(0, 1.65f + lo, 0), V3(0.1f, 0.12f, 0.105f), face);
    if (detail && a.alive) {
        if (a.uni == U_SYND) drawPart(B, org, V3(0, 1.67f + lo, 0.1f), V3(0.075f, 0.018f, 0.01f), C.skin); // прорезь балаклавы
        drawPart(B, org, V3(-0.04f, 1.675f + lo, 0.105f), V3(0.016f, 0.01f, 0.004f), RGBc(20, 18, 16));
        drawPart(B, org, V3(0.04f, 1.675f + lo, 0.105f), V3(0.016f, 0.01f, 0.004f), RGBc(20, 18, 16));
        if (a.uni == U_RAVEN || a.uni == U_MILITIA) drawPart(B, org, V3(0, 1.62f + lo, 0.104f), V3(0.035f, 0.008f, 0.005f), RGBc(60, 40, 30)); // усы
    }
    if (armored) {
        drawPart(B, org, V3(0, 1.75f + lo, -0.01f), V3(0.13f, 0.07f, 0.14f), C.head);           // шлем
        drawPart(B, org, V3(0, 1.69f + lo, -0.02f), V3(0.135f, 0.025f, 0.125f), shadeC(C.head, 0.85f)); // ушные чашки
        if (detail && a.uni != U_SOVA) {
            drawPart(B, org, V3(0, 1.79f + lo, 0.13f), V3(0.035f, 0.025f, 0.02f), RGBc(25, 25, 25)); // крепление ПНВ
            drawPart(B, org, V3(0, 1.71f + lo, 0.11f), V3(0.09f, 0.022f, 0.01f), RGBc(40, 40, 40));  // очки
        }
    } else if (a.uni == U_MILITIA) {
        drawPart(B, org, V3(0.02f, 1.78f + lo, 0), V3(0.12f, 0.035f, 0.12f), C.head);            // берет
        drawPart(B, org, V3(-0.05f, 1.79f + lo, 0.08f), V3(0.02f, 0.02f, 0.01f), RGBc(220, 190, 60));
    } else if (a.uni == U_RAVEN) {
        drawPart(B, org, V3(0, 1.77f + lo, -0.02f), V3(0.105f, 0.035f, 0.1f), C.head);             // волосы
    } else drawPart(B, org, V3(0, 1.77f + lo, -0.02f), V3(0.105f, 0.04f, 0.1f), C.head);
    // ---- руки и оружие
    if (a.kind == AK_SOLDIER || a.kind == AK_ALLY) {
        Basis A = bmul(B, rotX(a.alive ? -a.aimPitch : 0));
        V3 sh = org + bx(B, V3(0, 1.38f + lo, 0));
        drawPart(B, sh, V3(0.25f, -0.1f, 0.03f), V3(0.065f, 0.12f, 0.07f), C.torso);          // плечо правое
        drawPart(B, sh, V3(-0.25f, -0.1f, 0.05f), V3(0.065f, 0.12f, 0.07f), C.torso);         // плечо левое
        drawPart(A, sh, V3(0.17f, -0.17f, 0.2f), V3(0.055f, 0.055f, 0.14f), C.torso);         // предплечья
        drawPart(A, sh, V3(-0.09f, -0.12f, 0.32f), V3(0.055f, 0.055f, 0.16f), C.torso);
        drawPart(A, sh, V3(0.1f, -0.12f, 0.33f), V3(0.04f, 0.045f, 0.045f), glove);          // кисти
        drawPart(A, sh, V3(0.06f, -0.06f, 0.6f), V3(0.04f, 0.045f, 0.045f), glove);
        drawNpcGun(A, sh, a.weapon, detail);
        bool pistol = a.weapon == W_PISTOL;
        if (a.muzzle > 0 && a.alive) drawSprite(sh + bx(A, V3(0.08f, -0.02f, pistol ? 0.62f : 1.0f)), 0.35f, RGBc(255, 200, 110), RM_ADD);
    } else if (a.kind == AK_CONTACT || a.kind == AK_VIP) {
        drawPart(B, org, V3(0.25f, 1.15f + lo, 0), V3(0.06f, 0.28f, 0.065f), C.torso);
        drawPart(B, org, V3(-0.25f, 1.15f + lo, 0), V3(0.06f, 0.28f, 0.065f), C.torso);
        drawPart(B, org, V3(0.25f, 0.84f + lo, 0), V3(0.04f, 0.045f, 0.045f), C.skin);
        drawPart(B, org, V3(-0.25f, 0.84f + lo, 0), V3(0.04f, 0.045f, 0.045f), C.skin);
    }
    if (a.alive) drawShadow(a.pos, 0.45f);
}
static void drawTarget(const Actor& a) {
    if (!sphereVis(a.pos + V3(0, 1, 0), 1.2f)) return;
    Basis B = basisYPR(a.yaw, 0, 0);
    if (!a.alive) B = bmul(B, rotX(-std::min(1.0f, a.deadT / 0.3f) * PI_F * 0.48f));
    bool civ = a.kind == AK_CIVTARGET;
    drawPart(basisYPR(a.yaw, 0, 0), a.pos, V3(0, 0.3f, -0.05f), V3(0.04f, 0.3f, 0.04f), RGBc(90, 70, 50));
    V3 o = a.pos + V3(0, 0.55f, 0);
    drawPart(B, o, V3(0, 0.55f, 0), V3(0.25f, 0.52f, 0.02f), civ ? RGBc(220, 215, 200) : RGBc(70, 80, 55));
    drawPart(B, o, V3(0, 1.17f, 0), V3(0.12f, 0.12f, 0.02f), civ ? RGBc(220, 215, 200) : RGBc(70, 80, 55));
    drawPart(B, o, V3(0, 0.75f, 0.025f), V3(0.09f, 0.09f, 0.01f), civ ? RGBc(60, 90, 200) : RGBc(200, 40, 30));
    if (a.alive) drawShadow(a.pos, 0.35f);
}
static void drawDog(const Actor& a) {
    if (!sphereVis(a.pos + V3(0, 0.5f, 0), 1.2f)) return;
    Basis B = basisYPR(a.yaw, 0, 0);
    if (!a.alive) B = bmul(B, rotZ(std::min(1.0f, a.deadT / 0.4f) * PI_F * 0.5f));
    uint32_t c = a.hitFlash > 0 ? RGBc(220, 200, 190) : RGBc(70, 55, 40);
    V3 o = a.pos;
    drawPart(B, o, V3(0, 0.55f, 0), V3(0.14f, 0.15f, 0.42f), c);
    drawPart(B, o, V3(0, 0.75f, 0.5f), V3(0.1f, 0.1f, 0.14f), c);
    drawPart(B, o, V3(0, 0.71f, 0.68f), V3(0.06f, 0.05f, 0.08f), RGBc(30, 25, 20));
    drawPart(B, o, V3(0.06f, 0.88f, 0.46f), V3(0.025f, 0.05f, 0.02f), c);
    drawPart(B, o, V3(-0.06f, 0.88f, 0.46f), V3(0.025f, 0.05f, 0.02f), c);
    float sw = a.alive ? sinf(a.walkPh * 1.4f) * 0.6f * std::min(1.0f, a.speedNow / 3.0f) : 0.0f;
    for (int k = 0; k < 4; k++) {
        float z = k < 2 ? 0.3f : -0.3f, x = (k & 1) ? 0.09f : -0.09f;
        Basis L = bmul(B, rotX(((k == 0 || k == 3) ? sw : -sw)));
        drawPart(L, o + bx(B, V3(x, 0.45f, z)), V3(0, -0.22f, 0), V3(0.04f, 0.23f, 0.04f), c);
    }
    drawPart(bmul(B, rotX(-0.6f)), o + bx(B, V3(0, 0.65f, -0.42f)), V3(0, 0, -0.12f), V3(0.025f, 0.025f, 0.13f), c);
    if (a.alive) drawShadow(a.pos, 0.45f);
}
// граната: 0 — осколочная «яйцо» с чекой, 1 — светошумовая, 2 — дымовая
static void drawGrenadeModel(V3 c, const Basis& B, int type, bool cam) {
    if (type == 0) { drawGrenadeMesh(c, B, cam); return; }
    if (type == 0) {
        uint32_t body = RGBc(62, 72, 42);
        drawOBB(c + bx(B, V3(0, 0, 0)), B.r, B.u, B.f, V3(0.032f, 0.042f, 0.032f), body, cam);
        drawOBB(c + bx(B, V3(0, 0, 0)), B.r, B.u, B.f, V3(0.036f, 0.03f, 0.036f), shadeC(body, 0.85f), cam); // рубашка
        drawOBB(c + bx(B, V3(0, 0.05f, 0)), B.r, B.u, B.f, V3(0.013f, 0.012f, 0.013f), RGBc(90, 90, 85), cam);  // запал
        drawOBB(c + bx(B, V3(0.02f, 0.025f, 0)), B.r, B.u, B.f, V3(0.006f, 0.035f, 0.012f), RGBc(110, 110, 100), cam); // рычаг
        drawOBB(c + bx(B, V3(-0.025f, 0.055f, 0)), B.r, B.u, B.f, V3(0.012f, 0.012f, 0.002f), RGBc(170, 160, 120), cam); // кольцо
    } else {
        uint32_t body = type == 1 ? RGBc(58, 60, 70) : RGBc(120, 125, 118);
        drawOBB(c, B.r, B.u, B.f, V3(0.025f, 0.055f, 0.025f), body, cam);
        drawOBB(c + bx(B, V3(0, 0.065f, 0)), B.r, B.u, B.f, V3(0.014f, 0.01f, 0.014f), RGBc(90, 90, 85), cam);
        drawOBB(c + bx(B, V3(0.018f, 0.03f, 0)), B.r, B.u, B.f, V3(0.006f, 0.04f, 0.01f), RGBc(110, 110, 100), cam);
        drawOBB(c + bx(B, V3(0, type == 1 ? 0.02f : -0.02f, 0)), B.r, B.u, B.f, V3(0.027f, 0.008f, 0.027f), type == 1 ? RGBc(200, 60, 40) : RGBc(230, 230, 230), cam); // полоса маркировки
    }
}
// освещаемый усечённый конус/цилиндр: ось ax (0 — B.r, 1 — B.u, 2 — B.f), центр c в базисе B от org
static void litCone(const Basis& B, V3 org, V3 c, int ax, float r0, float r1, float hl, int n, uint32_t col, bool caps = true) {
    n = clampi(n, 3, 16);
    V3 a = ax == 0 ? B.r : (ax == 1 ? B.u : B.f);
    V3 p = ax == 0 ? B.u : B.r, q = ax == 2 ? B.u : B.f;
    V3 C = org + bx(B, c);
    V3 rim[16];
    for (int i = 0; i < n; i++) { float t = i * 2 * PI_F / n; rim[i] = p * cosf(t) + q * sinf(t); }
    V3 A0 = C - a * hl, A1 = C + a * hl;
    float slope = (r0 - r1) / std::max(1e-4f, 2 * hl);
    std::vector<GV>* gb = g_useGL ? &glBatch(RM_OPAQUE) : nullptr;
    uint32_t gc = glCol(col, 1);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        V3 w[4] = {A0 + rim[i] * r0, A0 + rim[j] * r0, A1 + rim[j] * r1, A1 + rim[i] * r1};
        V3 ni = norm(rim[i] + a * slope), nj = norm(rim[j] + a * slope);
        if (gb) {
            glPush(*gb, w[0], ni, gc, 0, 0, 0, -1000, 0); glPush(*gb, w[1], nj, gc, 0, 0, 0, -1000, 0); glPush(*gb, w[2], nj, gc, 0, 0, 0, -1000, 0);
            glPush(*gb, w[0], ni, gc, 0, 0, 0, -1000, 0); glPush(*gb, w[2], nj, gc, 0, 0, 0, -1000, 0); glPush(*gb, w[3], ni, gc, 0, 0, 0, -1000, 0);
        } else {
            V3 nm = norm(ni + nj);
            if (dot(nm, g_cam.pos - w[0]) <= 0) continue;
            V3 qc[4] = {toCam(w[0]), toCam(w[1]), toCam(w[2]), toCam(w[3])};
            drawPolyCam(qc, 4, shadeC(col, faceShade(nm)), RM_OPAQUE);
        }
    }
    if (!caps) return;
    for (int e = 0; e < 2; e++) {
        V3 cc = e ? A1 : A0; float r = e ? r1 : r0; V3 nrm = e ? a : -a;
        if (r < 1e-4f) continue;
        if (gb) {
            for (int i = 0; i < n; i++) { glPush(*gb, cc, nrm, gc, 0, 0, 0, -1000, 0); glPush(*gb, cc + rim[i] * r, nrm, gc, 0, 0, 0, -1000, 0); glPush(*gb, cc + rim[(i + 1) % n] * r, nrm, gc, 0, 0, 0, -1000, 0); }
        } else {
            if (dot(nrm, g_cam.pos - cc) <= 0) continue;
            uint32_t sc = shadeC(col, faceShade(nrm));
            for (int i = 0; i < n; i++) { V3 qc[3] = {toCam(cc), toCam(cc + rim[i] * r), toCam(cc + rim[(i + 1) % n] * r)}; drawPolyCam(qc, 3, sc, RM_OPAQUE); }
        }
    }
}
static void litCyl(const Basis& B, V3 org, V3 c, int ax, float r, float hl, int n, uint32_t col, bool caps = true) { litCone(B, org, c, ax, r, r, hl, n, col, caps); }

// основной боевой танк: гусеницы с траками, катки, экраны и ДЗ, башня с оптикой, пушка с термокожухом
static void drawTank(const Vehicle& v) {
    V3 o = v.pos;
    if (!sphereVis(o + V3(0, 1.4f, 0), 6.5f)) return;
    Basis B = basisYPR(v.yaw, 0, 0);
    bool alive = v.alive;
    float dist = len(o - g_cam.pos);
    bool near = dist < 45, close = dist < 22;
    int seg = close ? 14 : (near ? 10 : 7);
    uint32_t hull = alive ? (v.team == 0 ? RGBc(86, 96, 64) : RGBc(120, 112, 84)) : RGBc(34, 32, 30);
    uint32_t dark = shadeC(hull, 0.72f), era = shadeC(hull, 0.9f), light = shadeC(hull, 1.08f);
    uint32_t trackC = RGBc(34, 33, 31), steel = alive ? RGBc(62, 62, 58) : RGBc(28, 27, 26), rubber = RGBc(24, 24, 23);
    uint32_t wheelC = alive ? shadeC(hull, 0.85f) : RGBc(26, 24, 22), glass = RGBc(40, 58, 72), black = RGBc(20, 20, 20);
    // ---------------- ходовая часть
    for (int s = -1; s <= 1; s += 2) {
        float tx = 1.6f * s;
        // гусеница: нижняя ветвь, наклонные участки, верхняя ветвь под полкой
        drawPart(B, o, V3(tx, 0.05f, 0), V3(0.33f, 0.05f, 2.75f), trackC);
        drawPart(bmul(B, rotX(-0.62f)), o + bx(B, V3(tx, 0.32f, 3.0f)), V3(0, 0, 0), V3(0.33f, 0.05f, 0.42f), trackC);
        drawPart(bmul(B, rotX(0.55f)), o + bx(B, V3(tx, 0.34f, -3.02f)), V3(0, 0, 0), V3(0.33f, 0.05f, 0.42f), trackC);
        drawPart(B, o, V3(tx, 0.9f, 0), V3(0.33f, 0.04f, 3.1f), trackC);
        if (near) for (int k = -16; k <= 16; k++)                                             // грунтозацепы траков
            drawPart(B, o, V3(tx, 0.0f, k * 0.17f), V3(0.34f, 0.018f, 0.035f), rubber);
        if (close) for (int k = -15; k <= 15; k++) drawPart(B, o, V3(tx + 0.335f * s, 0.06f, k * 0.17f + 0.08f), V3(0.012f, 0.03f, 0.05f), steel); // пальцы траков
        // опорные катки (сдвоенные), ступицы
        for (int k = 0; k < 6; k++) {
            float z = -2.5f + k * 1.0f;
            litCyl(B, o, V3(tx + 0.17f * s, 0.38f, z), 0, 0.33f, 0.07f, seg, wheelC);
            litCyl(B, o, V3(tx - 0.17f * s, 0.38f, z), 0, 0.33f, 0.07f, seg, wheelC);
            litCyl(B, o, V3(tx + 0.17f * s, 0.38f, z), 0, 0.345f, 0.03f, seg, rubber, false);  // резиновые бандажи
            if (near) litCyl(B, o, V3(tx + 0.25f * s, 0.38f, z), 0, 0.11f, 0.03f, 8, steel);  // ступица
            if (close) for (int b = 0; b < 6; b++) { float a = b * PI_F / 3; litCyl(B, o, V3(tx + 0.25f * s, 0.38f + cosf(a) * 0.2f, z + sinf(a) * 0.2f), 0, 0.022f, 0.02f, 5, steel); } // болты/облегчения
        }
        // поддерживающие ролики
        for (int k = 0; k < 3; k++) litCyl(B, o, V3(tx, 0.84f, -1.6f + k * 1.6f), 0, 0.08f, 0.12f, 8, steel);
        // ленивец и ведущее колесо с зубьями
        litCyl(B, o, V3(tx, 0.55f, 3.05f), 0, 0.27f, 0.16f, seg, steel);
        litCyl(B, o, V3(tx, 0.58f, -3.05f), 0, 0.29f, 0.16f, seg, dark);
        if (near) for (int t = 0; t < 12; t++) {
            float a = t * PI_F / 6;
            drawPart(bmul(B, rotX(a)), o + bx(B, V3(tx, 0.58f + cosf(a) * 0.3f, -3.05f + sinf(a) * 0.3f)), V3(0, 0, 0), V3(0.12f, 0.035f, 0.035f), steel);
        }
        litCyl(B, o, V3(tx + 0.18f * s, 0.58f, -3.05f), 0, 0.12f, 0.03f, 8, black);
        // надгусеничная полка, брызговики
        drawPart(B, o, V3(1.55f * s, 1.0f, 0.1f), V3(0.42f, 0.03f, 3.3f), hull);
        drawPart(bmul(B, rotX(-0.5f)), o + bx(B, V3(1.55f * s, 0.92f, 3.5f)), V3(0, 0, 0), V3(0.42f, 0.025f, 0.22f), hull);
        drawPart(B, o, V3(1.55f * s, 0.85f, -3.35f), V3(0.4f, 0.14f, 0.02f), rubber);
        // бортовые экраны с блоками ДЗ в передней части и резинотканевые в задней
        for (int k = 0; k < 6; k++) {
            float z = 2.55f - k * 1.02f;
            bool eraPanel = k < 3;
            drawPart(B, o, V3(1.98f * s, 0.72f, z), V3(0.025f, eraPanel ? 0.26f : 0.2f, 0.49f), eraPanel ? era : rubber);
            if (eraPanel && near) for (int b = 0; b < 2; b++) drawPart(B, o, V3(2.01f * s, 0.72f, z - 0.24f + b * 0.48f), V3(0.012f, 0.2f, 0.2f), shadeC(era, 0.94f));
        }
        // фары и крюки
        litCyl(B, o, V3(1.0f * s, 1.32f, 3.15f), 2, 0.08f, 0.05f, 8, steel);
        if (near) { litCyl(B, o, V3(1.0f * s, 1.32f, 3.205f), 2, 0.06f, 0.006f, 8, RGBc(200, 200, 170), false); drawPart(B, o, V3(1.0f * s, 1.42f, 3.17f), V3(0.11f, 0.012f, 0.06f), dark); }
        drawPart(B, o, V3(0.7f * s, 0.6f, 3.34f), V3(0.06f, 0.08f, 0.1f), dark);           // буксирные крюки
        drawPart(B, o, V3(0.75f * s, 0.62f, -3.27f), V3(0.06f, 0.08f, 0.08f), dark);
        if (near) drawPart(B, o, V3(1.45f * s, 1.05f, 1.2f), V3(0.07f, 0.03f, 0.9f), black); // ЗИП/лом
    }
    // ---------------- корпус
    drawPart(B, o, V3(0, 0.8f, 0), V3(1.15f, 0.42f, 3.2f), hull);
    drawPart(B, o, V3(0, 1.33f, -0.2f), V3(1.55f, 0.15f, 2.9f), hull);
    Basis G = bmul(B, rotX(-0.95f));                                                      // ВЛД
    V3 gp = o + bx(B, V3(0, 1.08f, 3.05f));
    drawPart(G, gp, V3(0, 0, 0), V3(1.3f, 0.07f, 0.55f), light);
    drawPart(bmul(B, rotX(0.75f)), o + bx(B, V3(0, 0.62f, 3.28f)), V3(0, 0, 0), V3(1.12f, 0.05f, 0.3f), hull); // НЛД
    if (near) {
        for (int r = 0; r < 2; r++) for (int k = 0; k < 6; k++)                            // ДЗ на ВЛД
            drawPart(G, gp, V3(-1.05f + k * 0.42f, 0.09f, 0.3f - r * 0.42f), V3(0.18f, 0.035f, 0.18f), era);
        drawPart(bmul(B, rotX(-0.35f)), o + bx(B, V3(0, 1.35f, 3.3f)), V3(0, 0, 0), V3(1.25f, 0.05f, 0.08f), dark); // волноотбойный щиток
    }
    drawPart(B, o, V3(0, 1.5f, 2.15f), V3(0.32f, 0.04f, 0.26f), dark);                    // люк мехвода
    if (near) { for (int k = -1; k <= 1; k++) drawPart(B, o, V3(k * 0.16f, 1.54f, 2.42f), V3(0.05f, 0.04f, 0.03f), glass); } // приборы наблюдения
    // МТО: решётки, жалюзи, выхлоп, бочки, бревно
    drawPart(B, o, V3(0, 1.52f, -2.4f), V3(1.3f, 0.06f, 0.65f), dark);
    for (int k = 0; k < 3; k++) {
        drawPart(B, o, V3(-0.8f + k * 0.8f, 1.585f, -2.4f), V3(0.32f, 0.01f, 0.52f), RGBc(32, 32, 30));
        if (near) for (int b = 0; b < 8; b++) drawPart(B, o, V3(-0.8f + k * 0.8f, 1.6f, -2.82f + b * 0.12f), V3(0.31f, 0.012f, 0.012f), dark);
    }
    drawPart(B, o, V3(0, 1.2f, -3.22f), V3(1.0f, 0.22f, 0.03f), RGBc(30, 30, 28));         // кормовой лист с жалюзи
    if (near) for (int b = 0; b < 4; b++) drawPart(B, o, V3(-0.55f, 1.07f + b * 0.08f, -3.25f), V3(0.35f, 0.012f, 0.02f), RGBc(55, 50, 45));
    for (int s = -1; s <= 1; s += 2) litCyl(B, o, V3(0.85f * s, 1.62f, -3.0f), 0, 0.27f, 0.42f, seg, dark);   // топливные бочки
    if (near) for (int s = -1; s <= 1; s += 2) for (int b = -1; b <= 1; b += 2) litCyl(B, o, V3(0.85f * s + b * 0.32f, 1.62f, -3.0f), 0, 0.285f, 0.02f, seg, steel, false);
    litCyl(B, o, V3(0, 1.98f, -3.0f), 0, 0.13f, 1.45f, seg, alive ? RGBc(92, 70, 46) : RGBc(30, 26, 22)); // бревно самовытаскивания
    // ---------------- башня
    Basis T = bmul(B, basisYPR(v.orbitA, 0, 0));
    V3 tp = o + bx(B, V3(0, 1.48f, 0.2f));
    litCone(T, tp, V3(0, 0.07f, 0), 1, 1.18f, 1.18f, 0.07f, seg + 4, dark);               // погон
    drawPart(T, tp, V3(0, 0.32f, 0), V3(1.15f, 0.32f, 1.3f), hull);
    drawPart(bmul(T, rotX(0.18f)), tp + bx(T, V3(0, 0.64f, 0.1f)), V3(0, 0, 0), V3(1.05f, 0.04f, 1.1f), light); // крыша
    drawPart(T, tp, V3(0, 0.3f, -1.55f), V3(0.95f, 0.26f, 0.35f), shadeC(hull, 0.92f));   // кормовая ниша
    for (int s = -1; s <= 1; s += 2) {
        Basis W = bmul(T, basisYPR(0.45f * s, 0, 0));                                       // лобовые скулы
        V3 wp = tp + bx(T, V3(0.62f * s, 0.3f, 1.25f));
        drawPart(W, wp, V3(0, 0, 0), V3(0.55f, 0.3f, 0.32f), hull);
        Basis K = bmul(T, basisYPR(0.6f * s, -0.15f, 0));                                   // клиновидные модули ДЗ
        drawPart(K, tp + bx(T, V3(0.66f * s, 0.36f, 1.62f)), V3(0, 0, 0), V3(0.5f, 0.24f, 0.12f), era);
        if (near) for (int b = 0; b < 3; b++) drawPart(K, tp + bx(T, V3(0.66f * s, 0.36f, 1.62f)), V3(-0.33f + b * 0.33f, 0.0f, 0.13f), V3(0.15f, 0.2f, 0.02f), shadeC(era, 0.93f));
        for (int k = 0; k < 4; k++) drawPart(T, tp, V3(0.95f * s - s * k * 0.28f, 0.68f, 1.15f), V3(0.12f, 0.03f, 0.18f), era);   // ДЗ на крыше
        // ящики ЗИП на бортах башни
        drawPart(T, tp, V3(1.2f * s, 0.32f, -0.35f), V3(0.07f, 0.17f, 0.55f), shadeC(hull, 0.95f));
        if (near) drawPart(T, tp, V3(1.27f * s, 0.32f, -0.35f), V3(0.012f, 0.12f, 0.48f), dark);
        // дымовые гранатомёты (наклонные трубы)
        for (int k = 0; k < 4; k++) {
            Basis Sm = bmul(T, basisYPR(0.25f * s, 0.45f, 0));
            litCyl(Sm, tp + bx(T, V3(1.13f * s, 0.5f + (k & 1) * 0.1f, 0.75f - (k >> 1) * 0.18f)), V3(0, 0, 0), 2, 0.045f, 0.12f, 6, RGBc(50, 52, 44));
        }
    }
    // корзина в кормовой нише
    if (near) {
        for (int s = -1; s <= 1; s += 2) drawPart(T, tp, V3(0.9f * s, 0.62f, -1.75f), V3(0.015f, 0.12f, 0.25f), black);
        drawPart(T, tp, V3(0, 0.74f, -2.0f), V3(0.92f, 0.015f, 0.015f), black);
        drawPart(T, tp, V3(0, 0.62f, -2.0f), V3(0.015f, 0.12f, 0.015f), black);
        drawPart(T, tp, V3(-0.35f, 0.66f, -1.72f), V3(0.42f, 0.11f, 0.2f), RGBc(84, 80, 58));   // брезент
        drawPart(T, tp, V3(0.45f, 0.64f, -1.75f), V3(0.25f, 0.09f, 0.18f), RGBc(64, 70, 50));   // рюкзаки
    }
    // пушка: маска, термокожух с хомутами, эжектор, дульный срез
    drawPart(T, tp, V3(0, 0.32f, 1.48f), V3(0.34f, 0.24f, 0.2f), dark);
    if (near) drawPart(T, tp, V3(0, 0.32f, 1.69f), V3(0.24f, 0.18f, 0.02f), rubber);
    litCyl(T, tp, V3(0, 0.35f, 2.2f), 2, 0.115f, 0.55f, seg, dark);
    litCone(T, tp, V3(0, 0.35f, 2.95f), 2, 0.15f, 0.15f, 0.22f, seg, shadeC(dark, 1.05f));
    litCone(T, tp, V3(0, 0.35f, 3.22f), 2, 0.15f, 0.1f, 0.05f, seg, dark, false);
    litCone(T, tp, V3(0, 0.35f, 4.05f), 2, 0.1f, 0.088f, 0.8f, seg, dark);
    if (near) for (int k = 0; k < 4; k++) litCyl(T, tp, V3(0, 0.35f, 1.85f + k * 0.4f + (k > 1 ? 1.05f : 0)), 2, 0.122f, 0.02f, seg, steel, false);
    litCone(T, tp, V3(0, 0.35f, 4.9f), 2, 0.1f, 0.1f, 0.05f, seg, RGBc(30, 30, 28));
    if (close) drawPart(T, tp, V3(0, 0.47f, 4.84f), V3(0.03f, 0.02f, 0.03f), black);       // датчик изгиба ствола
    // командирская башенка с приборами и пулемётом
    litCyl(T, tp, V3(0.5f, 0.74f, -0.2f), 1, 0.3f, 0.09f, seg, dark);
    litCyl(T, tp, V3(0.5f, 0.85f, -0.25f), 1, 0.24f, 0.025f, seg, hull);
    if (near) for (int k = 0; k < 6; k++) { float a = k * PI_F / 3; drawPart(bmul(T, basisYPR(a, 0, 0)), tp + bx(T, V3(0.5f + sinf(a) * 0.27f, 0.79f, -0.2f + cosf(a) * 0.27f)), V3(0, 0, 0), V3(0.06f, 0.035f, 0.02f), glass); }
    drawPart(T, tp, V3(0.5f, 0.95f, 0.05f), V3(0.05f, 0.06f, 0.16f), black);              // пулемёт: короб
    litCyl(T, tp, V3(0.5f, 0.96f, 0.5f), 2, 0.018f, 0.3f, 6, black);                     // ствол
    drawPart(T, tp, V3(0.6f, 0.92f, 0.0f), V3(0.05f, 0.06f, 0.07f), RGBc(64, 70, 50));     // патронный короб
    drawPart(T, tp, V3(0.5f, 0.89f, -0.05f), V3(0.02f, 0.05f, 0.02f), black);
    // прицел наводчика и тепловизор
    drawPart(T, tp, V3(-0.5f, 0.76f, 0.55f), V3(0.2f, 0.12f, 0.16f), dark);
    drawPart(T, tp, V3(-0.5f, 0.78f, 0.715f), V3(0.15f, 0.07f, 0.01f), glass);
    if (near) { drawPart(T, tp, V3(-0.5f, 0.9f, 0.6f), V3(0.21f, 0.015f, 0.12f), hull); drawPart(T, tp, V3(0.85f, 0.75f, 0.4f), V3(0.13f, 0.1f, 0.14f), dark); drawPart(T, tp, V3(0.85f, 0.76f, 0.545f), V3(0.09f, 0.06f, 0.008f), glass); }
    drawPart(T, tp, V3(-0.4f, 0.68f, -0.55f), V3(0.24f, 0.035f, 0.24f), dark);            // люк наводчика
    // датчики облучения, антенны, ветровой датчик
    if (near) for (int s = -1; s <= 1; s += 2) drawPart(T, tp, V3(0.9f * s, 0.74f, 0.9f), V3(0.04f, 0.04f, 0.04f), black);
    if (v.tag != -7) {
        litCyl(T, tp, V3(-0.7f, 1.25f, -1.2f), 1, 0.008f, 0.6f, 4, black, false);
        litCyl(T, tp, V3(0.7f, 1.05f, -1.3f), 1, 0.008f, 0.4f, 4, black, false);
        litCyl(T, tp, V3(-0.7f, 0.68f, -1.2f), 1, 0.04f, 0.04f, 6, dark);
    }
    if (near) litCyl(T, tp, V3(0.15f, 0.85f, -1.0f), 1, 0.02f, 0.18f, 5, black);           // датчик ветра
    if (!alive) for (int k = 0; k < 3; k++) drawPart(B, o, V3(-0.9f + k * 0.9f, 1.36f + 0.02f * k, -0.6f + k * 0.7f), V3(0.35f, 0.01f, 0.3f), RGBc(18, 17, 16)); // копоть
    if (alive && v.fireCD > 6.6f) drawSprite(tp + bx(T, V3(0, 0.35f, 5.15f)), 1.6f, RGBc(255, 210, 120), RM_ADD);
    drawShadow(o, 3.2f);
}
static void drawVehicle(const Vehicle& v) {
    if (!v.active) return;
    if (v.kind == VK_TANK) { drawTank(v); return; }
    Basis B = basisYPR(v.yaw, v.kind == VK_HELI ? v.pitch : 0, v.roll);
    V3 o = v.pos;
    uint32_t body = v.team == 0 ? RGBc(78, 88, 62) : RGBc(70, 72, 68);
    if (!v.alive) body = RGBc(30, 28, 26);
    switch (v.kind) {
    case VK_TRUCK: case VK_TECH: case VK_JEEP: case VK_TEL: {
        if (!sphereVis(o + V3(0, 1.2f, 0), 6)) return;
        bool jeep = v.kind == VK_JEEP, tel = v.kind == VK_TEL, tech = v.kind == VK_TECH;
        float L = jeep ? 1.9f : (tel ? 4.5f : 2.8f);
        uint32_t bc = tel ? RGBc(85, 95, 70) : (tech ? RGBc(185, 175, 150) : body);
        if (!v.alive) bc = body;
        drawPart(B, o, V3(0, 0.85f, 0), V3(jeep ? 0.85f : 1.15f, 0.35f, L), bc);
        drawPart(B, o, V3(0, 1.55f, L - (jeep ? 0.9f : 0.9f)), V3(jeep ? 0.8f : 1.1f, jeep ? 0.4f : 0.55f, jeep ? 0.5f : 0.85f), bc);
        drawPart(B, o, V3(0, 1.75f, L - 0.05f), V3(jeep ? 0.7f : 1.0f, 0.2f, 0.03f), RGBc(40, 60, 80));
        for (int k = 0; k < (tel ? 8 : 4); k++) {
            float z = tel ? (-L + 0.8f + (k / 2) * (2 * L - 1.6f) / 3.0f) : ((k < 2) ? L - 0.8f : -L + 0.8f);
            drawPart(B, o, V3((k & 1) ? 1.05f : -1.05f, 0.45f, z), V3(0.18f, 0.45f, 0.45f), RGBc(25, 25, 25));
        }
        if (v.kind == VK_TRUCK) drawPart(B, o, V3(0, 1.95f, -0.7f), V3(1.2f, 0.75f, 1.9f), v.alive ? RGBc(88, 96, 70) : body);
        if (tech) {
            drawPart(B, o, V3(0, 1.6f, -1.5f), V3(0.06f, 0.4f, 0.06f), RGBc(40, 40, 40));
            drawPart(B, o, V3(0, 2.05f, -1.2f), V3(0.08f, 0.08f, 0.6f), RGBc(30, 30, 30));
        }
        if (tel) {
            Basis M = bmul(B, rotX(-0.95f));
            V3 piv = o + bx(B, V3(0, 1.6f, -2.5f));
            drawPart(M, piv, V3(0, 0, 3.0f), V3(0.38f, 0.38f, 3.2f), RGBc(225, 225, 215));
            drawPart(M, piv, V3(0, 0, 6.4f), V3(0.28f, 0.28f, 0.3f), RGBc(190, 40, 30));
        }
        drawShadow(o, jeep ? 1.6f : 2.6f);
    } break;
    case VK_HELI: {
        if (!sphereVis(o, 8)) return;
        uint32_t bc = v.alive ? (v.team == 0 ? RGBc(70, 80, 58) : RGBc(42, 44, 46)) : body;
        drawPart(B, o, V3(0, 0, 0.3f), V3(1.0f, 0.95f, 2.4f), bc);
        drawPart(B, o, V3(0, -0.15f, 2.7f), V3(0.8f, 0.6f, 0.5f), RGBc(50, 70, 90));
        drawPart(B, o, V3(0, 0.3f, -4.2f), V3(0.22f, 0.25f, 2.6f), bc);
        drawPart(B, o, V3(0, 0.95f, -6.6f), V3(0.08f, 0.7f, 0.4f), bc);
        drawPart(B, o, V3(-0.8f, -1.2f, 0.3f), V3(0.06f, 0.06f, 1.8f), RGBc(30, 30, 30));
        drawPart(B, o, V3(0.8f, -1.2f, 0.3f), V3(0.06f, 0.06f, 1.8f), RGBc(30, 30, 30));
        drawPart(B, o, V3(0, 1.05f, 0.2f), V3(0.15f, 0.15f, 0.15f), RGBc(30, 30, 30));
        for (int k = 0; k < 2; k++) {
            Basis Rr = bmul(B, basisYPR(v.rotor + k * PI_F * 0.5f, 0, 0));
            drawPart(Rr, o + bx(B, V3(0, 1.2f, 0.2f)), V3(0, 0, 0), V3(0.12f, 0.02f, 5.8f), RGBc(20, 20, 20));
        }
        if (v.spot && v.alive) {
            V3 lp = o + bx(B, V3(0, -0.9f, 2.6f));
            V3 sp = v.spotP + V3(0, 0.05f, 0);
            V3 side = norm(cross(sp - lp, V3(0, 1, 0))) * 3.5f;
            drawTriW(lp, sp + side, sp - side, RGBc(28, 28, 22), RM_ADD);
            drawSprite(sp + V3(0, 0.2f, 0), 9, RGBc(70, 70, 55), RM_ADD);
        }
        if (v.alive && v.team == 1 && !v.spot && fmodf(v.t, 4.0f) < 1.2f && ((int)(v.t * 20) & 1)) drawSprite(o + V3(0, -1.0f, 0), 0.6f, RGBc(255, 210, 120), RM_ADD);
    } break;
    case VK_JET: {
        if (!sphereVis(o, 10)) return;
        uint32_t c = RGBc(110, 115, 120);
        drawPart(B, o, V3(0, 0, 0), V3(0.7f, 0.7f, 6.5f), c);
        drawPart(B, o, V3(0, 0, -1.0f), V3(5.5f, 0.12f, 1.6f), c);
        drawPart(B, o, V3(0, 1.2f, -5.5f), V3(0.1f, 1.2f, 0.8f), c);
        drawSprite(o - bx(B, V3(0, 0, 7.0f)), 1.4f, RGBc(255, 160, 80), RM_ADD);
    } break;
    default: break;
    }
}
static void drawGround() {
    if (!G.hasGround && !G.sea) return;
    float gy = G.groundY;
    V3 cp = g_cam.pos;
    int bx0 = (int)floorf(cp.x / 128.0f), bz0 = (int)floorf(cp.z / 128.0f);
    float far = std::min(g_cam.farZ, G.sea ? 400.0f : 3000.0f);
    // мелкие плитки
    for (int bz = bz0 - 1; bz <= bz0 + 1; bz++)
        for (int bxk = bx0 - 1; bxk <= bx0 + 1; bxk++)
            for (int tz = 0; tz < 8; tz++)
                for (int tx = 0; tx < 8; tx++) {
                    float x0 = bxk * 128.0f + tx * 16, z0 = bz * 128.0f + tz * 16;
                    V3 c(x0 + 8, gy, z0 + 8);
                    if (len(c - cp) > far + 12) continue;
                    if (!sphereVis(c, 11.4f)) continue;
                    uint32_t h = hash2((int)(x0 / 16), (int)(z0 / 16));
                    uint32_t col = mixC(G.gcol1, G.gcol2, (h & 255) / 255.0f);
                    if (G.sea) col = shadeC(col, 0.8f + 0.4f * fabsf(sinf(G.time * 1.3f + (h & 63) * 0.3f)));
                    float s = faceShade(V3(0, 1, 0));
                    if (!G.sea && len(c - cp) < 70) {
                        V3 q[4] = {toCam(V3(x0, gy, z0)), toCam(V3(x0 + 16, gy, z0)), toCam(V3(x0 + 16, gy, z0 + 16)), toCam(V3(x0, gy, z0 + 16))};
                        float U[4] = {x0, x0 + 16, x0 + 16, x0}, Vv[4] = {z0, z0, z0 + 16, z0 + 16};
                        g_texId = TX_GROUND; g_texVert = false;
                        drawPolyCamT(q, U, Vv, 4, shadeC(col, s));
                    } else drawQuadW(V3(x0, gy, z0), V3(x0 + 16, gy, z0), V3(x0 + 16, gy, z0 + 16), V3(x0, gy, z0 + 16), shadeC(col, s), RM_OPAQUE);
                }
    // крупные плитки
    int R = (int)(far / 128.0f) + 1;
    for (int bz = bz0 - R; bz <= bz0 + R; bz++)
        for (int bxk = bx0 - R; bxk <= bx0 + R; bxk++) {
            if (abs(bz - bz0) <= 1 && abs(bxk - bx0) <= 1) continue;
            float x0 = bxk * 128.0f, z0 = bz * 128.0f;
            V3 c(x0 + 64, gy, z0 + 64);
            if (len(c - cp) > far + 91) continue;
            if (!sphereVis(c, 91)) continue;
            uint32_t col = mixC(G.gcol1, G.gcol2, (hash2(bxk, bz) & 255) / 255.0f);
            drawQuadW(V3(x0, gy, z0), V3(x0 + 128, gy, z0), V3(x0 + 128, gy, z0 + 128), V3(x0, gy, z0 + 128), shadeC(col, faceShade(V3(0, 1, 0))), RM_OPAQUE);
        }
}
// трава в программном режиме: 2–3 травинки на кустик вблизи камеры
static void swTuft(V3 t, float h, uint32_t col, uint32_t seed, int blades) {
    float sh = g_env.amb + g_env.sunI * 0.6f;
    for (int b = 0; b < blades; b++) {
        uint32_t q = hash2((int)seed + b * 131, 99);
        float a = (q & 1023) / 1023.0f * 6.2832f, la = ((q >> 10) & 1023) / 1023.0f * 6.2832f;
        float lean = 0.15f + ((q >> 20) & 15) / 15.0f * 0.35f, hb = h * (0.6f + ((q >> 24) & 15) / 15.0f * 0.45f);
        float wv = sinf(G.time * 1.9f + t.x * 0.37f + t.z * 0.23f) * 0.07f;
        V3 s(cosf(a) * 0.035f, 0, sinf(a) * 0.035f), d(cosf(la), 0, sinf(la));
        V3 tip = t + d * (lean * hb) + V3(wv, hb, wv * 0.5f);
        drawTriW(t - s, t + s, tip, shadeC(mixC(shadeC(col, 0.7f), col, 0.5f + (b & 1) * 0.3f), sh), RM_OPAQUE);
    }
}
static void drawGrass() {
    for (auto& g : G.grass) {
        V3 c = (g.mn + g.mx) * 0.5f;
        float r = len(g.mx - g.mn) * 0.5f + 1;
        if (len(c - g_cam.pos) > 60 + r) continue;
        if (!sphereVis(c, r)) continue;
        for (size_t k = 0; k < g.tufts.size(); k++) {
            const V3& t = g.tufts[k];
            float dd = len(t - g_cam.pos);
            if (dd > 55) continue;
            swTuft(t, 0.62f + (k % 5) * 0.1f, shadeC(g.col, 0.85f + (k % 7) * 0.05f), (uint32_t)k * 7 + 1, dd < 18 ? 4 : 2);
        }
    }
    for (size_t k = 0; k < G.lawn.size(); k++) {
        const Tuft& t = G.lawn[k];
        V3 d = t.p - g_cam.pos;
        if (fabsf(d.x) > 20 || fabsf(d.z) > 20) continue;
        V3 cp = toCam(t.p + V3(0, 0.2f, 0));
        if (cp.z < 0.3f || fabsf(cp.x) > cp.z * 1.2f) continue;
        swTuft(t.p, t.h, t.col, (uint32_t)k * 13 + 5, len(d) < 10 ? 3 : 2);
    }
}
static void drawDynamic();
static void drawWorld() {
    if (g_useGL) { glScene(); return; }
    std::fill(g_zb.begin(), g_zb.end(), 0.0f);
    drawGround();
    for (const MGroup& g : G.mgrp) {                 // горы и воронки
        if (!sphereVis(g.c, g.r)) continue;
        for (int i = g.t0; i < g.t1; i++) {
            const MTri& t = G.mtri[i];
            V3 n = norm(cross(t.b - t.a, t.c - t.a));
            if (dot(n, g_cam.pos - t.a) <= 0) continue;
            V3 q[3] = {toCam(t.a), toCam(t.b), toCam(t.c)};
            drawPolyCam(q, 3, shadeC(t.col, faceShade(n)), RM_OPAQUE);
        }
    }
    for (const Prop& p : G.props) if (len(p.c - g_cam.pos) < 90) drawOBB(p.c, p.B.r, p.B.u, p.B.f, p.h, p.col);
    for (const Box& b : G.box) {
        if (!b.vis) continue;
        drawBoxW(b.mn, b.mx, b.col, b.glow, b.mat == MT_GLASS ? RM_BLEND : RM_OPAQUE, b.tex);
    }
    // скатные крыши
    for (const Prism& pr : G.prisms) {
        V3 c = (pr.mn + pr.mx) * 0.5f;
        if (!sphereVis(c, len(pr.mx - pr.mn) * 0.5f)) continue;
        V3 a0, a1, b0, b1, r0, r1;  // свесы по длинным сторонам и конёк
        float y0 = pr.mn.y, y1 = pr.mx.y;
        if (pr.alongX) {
            float zm = c.z;
            a0 = V3(pr.mn.x, y0, pr.mn.z); a1 = V3(pr.mx.x, y0, pr.mn.z); b0 = V3(pr.mn.x, y0, pr.mx.z); b1 = V3(pr.mx.x, y0, pr.mx.z);
            r0 = V3(pr.mn.x, y1, zm); r1 = V3(pr.mx.x, y1, zm);
        } else {
            float xm = c.x;
            a0 = V3(pr.mn.x, y0, pr.mn.z); a1 = V3(pr.mn.x, y0, pr.mx.z); b0 = V3(pr.mx.x, y0, pr.mn.z); b1 = V3(pr.mx.x, y0, pr.mx.z);
            r0 = V3(xm, y1, pr.mn.z); r1 = V3(xm, y1, pr.mx.z);
        }
        auto slope = [&](V3 p0, V3 p1, V3 q1, V3 q0) {
            V3 n = norm(cross(p1 - p0, q0 - p0));
            if (n.y < 0) n = -n;
            if (dot(g_cam.pos - p0, n) <= 0) return;
            uint32_t fc = shadeC(pr.col, faceShade(n));
            V3 cp[4] = {toCam(p0), toCam(p1), toCam(q1), toCam(q0)};
            float U[4], Vv[4]; V3 ws[4] = {p0, p1, q1, q0};
            float sl = sqrtf(1 + n.y * n.y);
            for (int k = 0; k < 4; k++) { U[k] = pr.alongX ? ws[k].x : ws[k].z; Vv[k] = (ws[k].y - y0) * 1.6f * sl; }
            if (len((p0 + q1) * 0.5f - g_cam.pos) < 55) { g_texId = TX_ROOF; g_texVert = false; drawPolyCamT(cp, U, Vv, 4, fc); }
            else drawPolyCam(cp, 4, fc, RM_OPAQUE);
        };
        slope(a0, a1, r1, r0); slope(b0, b1, r1, r0);
        // фронтоны
        V3 g0[3] = {a0, b0, r0}, g1[3] = {a1, b1, r1};
        for (int s = 0; s < 2; s++) {
            V3* g = s ? g1 : g0;
            V3 n = pr.alongX ? V3(s ? 1.0f : -1.0f, 0, 0) : V3(0, 0, s ? 1.0f : -1.0f);
            if (dot(g_cam.pos - g[0], n) <= 0) continue;
            V3 cp[3] = {toCam(g[0]), toCam(g[1]), toCam(g[2])};
            drawPolyCam(cp, 3, shadeC(pr.gable, faceShade(n)), RM_OPAQUE);
        }
    }
    drawGrass();
    drawDynamic();
    postSkyFog();
}
static void csDrawTower();
static void drawDynamic() {
    csDrawTower();
    for (const Vehicle& v : G.veh) drawVehicle(v);
    if (G.P.tank && G.P.adsT < 0.5f) {
        Vehicle tv; tv.kind = VK_TANK; tv.team = 0; tv.tag = -7; tv.pos = G.P.pos; tv.yaw = G.P.hull; tv.orbitA = wrapPi(G.P.turret - G.P.hull);
        tv.fireCD = G.P.treload > 3.2f ? 7.0f : 0.0f;
        drawTank(tv);
    }
    for (const Actor& a : G.act) {
        if (a.hidden || a.state == AS_CARRIED) continue;
        if (len(a.pos - g_cam.pos) > g_cam.farZ) continue;
        if (a.kind == AK_TARGET || a.kind == AK_CIVTARGET) drawTarget(a);
        else if (a.kind == AK_DOG) drawDog(a);
        else drawHuman(a);
    }
    for (const Grenade& g : G.gren) {
        if (g.type == 3) {
            V3 dv = norm(g.v);
            Basis rb = basisYPR(atan2f(dv.x, dv.z), asinf(clampf(dv.y, -1, 1)), 0);
            drawOBB(g.p, rb.r, rb.u, rb.f, V3(0.05f, 0.05f, 0.4f), RGBc(70, 80, 60));
            drawSprite(g.p - dv * 0.5f, 0.4f, RGBc(255, 200, 90), RM_ADD);
        } else drawGrenadeModel(g.p, basisYPR(g.spin, g.spin * 0.7f, 0), g.type, false);
    }
    if (G.waterOn) {
        float y = G.waterY;
        V3 a(G.waterMn.x, y, G.waterMn.z), b(G.waterMx.x, y, G.waterMn.z), c(G.waterMx.x, y, G.waterMx.z), d(G.waterMn.x, y, G.waterMx.z);
        drawQuadW(a, b, c, d, RGBc(20, 40, 60), RM_BLEND);
    }
    for (const Part& p : G.parts) {
        float k = p.life / p.maxLife;
        uint32_t c = p.col;
        if (p.mode == RM_ADD) c = shadeC(c, k);
        drawSprite(p.p, p.size, c, p.mode);
    }
    for (const Tracer& t : G.tracers) drawLine3D(t.a, t.b, t.col, RM_ADD);
    if (G.snow) {
        for (int i = 0; i < 220; i++) {
            uint32_t h = hash2(i, 9);
            float x = (float)((h & 1023) / 1023.0f - 0.5f) * 30, z = (float)(((h >> 10) & 1023) / 1023.0f - 0.5f) * 30;
            float y = fmodf((float)((h >> 20) & 255) / 255.0f * 12 - G.time * 1.6f, 12.0f);
            if (y < 0) y += 12;
            V3 p(floorf(g_cam.pos.x / 30) * 30 + x + sinf(G.time + i) * 0.5f, g_cam.pos.y - 4 + y, floorf(g_cam.pos.z / 30) * 30 + z + G.time * 1.2f);
            while (p.x < g_cam.pos.x - 15) p.x += 30;
            while (p.x > g_cam.pos.x + 15) p.x -= 30;
            while (p.z < g_cam.pos.z - 15) p.z += 30;
            while (p.z > g_cam.pos.z + 15) p.z -= 30;
            if (len(p - g_cam.pos) < 1.5f) continue;
            drawSprite(p, 0.05f, RGBc(235, 238, 245), RM_OPAQUE);
        }
    }
    if (G.rain && g_cam.pos.y > -0.5f) {
        for (int i = 0; i < 160; i++) {
            uint32_t h = hash2(i, 7);
            float x = (float)((h & 1023) / 1023.0f - 0.5f) * 24, z = (float)(((h >> 10) & 1023) / 1023.0f - 0.5f) * 24;
            float y = fmodf((float)((h >> 20) & 255) / 255.0f * 14 - G.time * 16.0f, 14.0f);
            if (y < 0) y += 14;
            V3 p = V3(floorf(g_cam.pos.x / 24) * 24 + x, g_cam.pos.y - 5 + y, floorf(g_cam.pos.z / 24) * 24 + z);
            if (p.x < g_cam.pos.x - 12) p.x += 24;
            if (p.x > g_cam.pos.x + 12) p.x -= 24;
            if (p.z < g_cam.pos.z - 12) p.z += 24;
            if (p.z > g_cam.pos.z + 12) p.z -= 24;
            if (len(p - g_cam.pos) < 1.5f) continue;
            drawLine3D(p, p + V3(0.3f, -0.6f, 0), RGBc(60, 65, 75), RM_ADD);
        }
    }
}

// ------------------------------------------------------------ оружие в руках
struct VPart { float cx, cy, cz, hx, hy, hz; uint32_t col; };
static const uint32_t VC_M = 0xFF2A2C2Au, VC_D = 0xFF1A1A1Au, VC_P = 0xFF3C4232u, VC_W = 0xFF6E5034u;
static const uint32_t VC_R = 0xFF232522u;
static const VPart kVAR[] = {
    {0, 0, 0.05f, 0.03f, 0.045f, 0.16f, VC_M}, {0, -0.005f, 0.30f, 0.032f, 0.038f, 0.10f, VC_P}, {0, 0.005f, 0.47f, 0.012f, 0.012f, 0.09f, VC_D},
    {0, 0.032f, 0.40f, 0.008f, 0.012f, 0.008f, VC_D}, {0, 0.05f, 0.40f, 0.003f, 0.012f, 0.003f, VC_D},
    {0.0095f, 0.058f, -0.06f, 0.0025f, 0.007f, 0.006f, VC_D}, {-0.0095f, 0.058f, -0.06f, 0.0025f, 0.007f, 0.006f, VC_D}, {0, 0.048f, -0.06f, 0.014f, 0.004f, 0.01f, VC_D},
    {0, -0.11f, 0.10f, 0.02f, 0.07f, 0.035f, VC_D}, {0, -0.02f, -0.22f, 0.025f, 0.05f, 0.12f, VC_P}, {0, -0.08f, -0.02f, 0.02f, 0.05f, 0.025f, VC_D},
    {0, 0.044f, 0.12f, 0.012f, 0.004f, 0.12f, VC_R},          // планка
    {0.034f, 0.012f, -0.0f, 0.006f, 0.008f, 0.018f, VC_D},    // рукоять затвора
    {0, 0.005f, 0.575f, 0.017f, 0.017f, 0.028f, VC_D},        // ДТК
    {0, -0.05f, 0.03f, 0.004f, 0.012f, 0.035f, VC_D},         // спусковая скоба
    {0.033f, -0.005f, 0.30f, 0.003f, 0.012f, 0.08f, VC_R}, {-0.033f, -0.005f, 0.30f, 0.003f, 0.012f, 0.08f, VC_R}, // прорези цевья
    {0, -0.05f, 0.33f, 0.014f, 0.035f, 0.016f, VC_D},         // тактическая рукоять
    {0, 0.02f, -0.21f, 0.022f, 0.012f, 0.08f, VC_D},          // щека приклада
};
static const VPart kVSMG[] = {
    {0, 0, 0.02f, 0.028f, 0.04f, 0.13f, VC_M}, {0, 0.0f, 0.2f, 0.012f, 0.012f, 0.07f, VC_D}, {0, -0.1f, 0.07f, 0.018f, 0.07f, 0.024f, VC_D},
    {0, -0.02f, -0.18f, 0.012f, 0.03f, 0.1f, VC_D}, {0, -0.075f, -0.04f, 0.018f, 0.045f, 0.022f, VC_D},
    {0, 0.045f, 0.13f, 0.003f, 0.01f, 0.003f, VC_D}, {0.009f, 0.051f, -0.07f, 0.0025f, 0.006f, 0.005f, VC_D}, {-0.009f, 0.051f, -0.07f, 0.0025f, 0.006f, 0.005f, VC_D},
    {0, 0.04f, 0.03f, 0.01f, 0.004f, 0.09f, VC_R},            // планка
    {0, -0.05f, 0.17f, 0.014f, 0.03f, 0.016f, VC_D},          // передняя рукоять
    {0, -0.005f, 0.13f, 0.03f, 0.03f, 0.05f, VC_P},           // цевьё
    {0.03f, 0.015f, 0.05f, 0.005f, 0.006f, 0.014f, VC_D},     // взвод
    {0, -0.06f, -0.27f, 0.012f, 0.035f, 0.012f, VC_D},        // затыльник
};
static const VPart kVSN[] = {
    {0, 0, 0.0f, 0.03f, 0.04f, 0.18f, VC_M}, {0, 0.01f, 0.46f, 0.013f, 0.013f, 0.28f, VC_D}, {0, 0.085f, 0.0f, 0.025f, 0.025f, 0.16f, VC_D},
    {0, 0.085f, 0.17f, 0.032f, 0.032f, 0.02f, VC_D}, {0, 0.05f, 0.0f, 0.01f, 0.02f, 0.05f, VC_D},
    {0, -0.02f, -0.3f, 0.03f, 0.06f, 0.14f, VC_W}, {0.04f, 0.01f, -0.08f, 0.02f, 0.006f, 0.006f, VC_D}, {0, -0.05f, 0.18f, 0.03f, 0.03f, 0.12f, VC_W},
    {0, 0.085f, -0.17f, 0.03f, 0.03f, 0.02f, VC_D},           // окуляр
    {0, 0.115f, 0.0f, 0.01f, 0.008f, 0.012f, VC_M}, {0.03f, 0.085f, 0.0f, 0.008f, 0.01f, 0.012f, VC_M}, // барабаны
    {0.015f, -0.04f, 0.5f, 0.005f, 0.005f, 0.12f, VC_D}, {-0.015f, -0.04f, 0.5f, 0.005f, 0.005f, 0.12f, VC_D}, // сошки
    {0, 0.025f, -0.28f, 0.024f, 0.012f, 0.07f, VC_W},         // щека
    {0, 0.01f, 0.75f, 0.02f, 0.02f, 0.03f, VC_D},             // ДТК
    {0, -0.065f, -0.02f, 0.012f, 0.03f, 0.02f, VC_D},         // магазин
};
static const VPart kVSG[] = {
    {0, 0, 0.0f, 0.03f, 0.045f, 0.14f, VC_M}, {0, 0.015f, 0.38f, 0.014f, 0.014f, 0.26f, VC_D}, {0, -0.02f, 0.32f, 0.012f, 0.012f, 0.2f, VC_D},
    {0, 0.033f, 0.63f, 0.004f, 0.004f, 0.004f, 0xFFB0B0A0u}, {0, -0.02f, -0.25f, 0.028f, 0.055f, 0.13f, VC_W}, {0, -0.075f, -0.03f, 0.018f, 0.045f, 0.022f, VC_D},
    {0, 0.032f, 0.38f, 0.017f, 0.004f, 0.22f, VC_R},          // кожух
    {0.031f, 0.01f, 0.03f, 0.002f, 0.02f, 0.04f, VC_D},       // окно выброса
    {0, -0.05f, 0.02f, 0.004f, 0.012f, 0.035f, VC_D},         // скоба
    {0, -0.075f, -0.37f, 0.03f, 0.065f, 0.012f, VC_D},        // затыльник
};
static const VPart kVPI[] = {
    {0, 0.02f, 0.05f, 0.015f, 0.018f, 0.09f, VC_M}, {0, -0.005f, 0.04f, 0.014f, 0.012f, 0.08f, VC_D}, {0, -0.06f, -0.02f, 0.016f, 0.05f, 0.022f, VC_D},
    {0, 0.042f, 0.13f, 0.003f, 0.007f, 0.003f, VC_D}, {0.008f, 0.042f, -0.03f, 0.003f, 0.007f, 0.004f, VC_D}, {-0.008f, 0.042f, -0.03f, 0.003f, 0.007f, 0.004f, VC_D},
    {0, 0.035f, -0.04f, 0.005f, 0.008f, 0.006f, VC_D},        // курок
    {0, -0.03f, 0.04f, 0.004f, 0.01f, 0.03f, VC_D},           // скоба
    {0.016f, 0.02f, -0.01f, 0.002f, 0.012f, 0.025f, VC_R},    // насечки затвора
    {0, -0.105f, -0.025f, 0.017f, 0.006f, 0.024f, VC_R},      // пятка магазина
};
static const float kAkVmZ = 0.22f;   // сдвиг АК-12 вперёд: рукоять под правой кистью
static float sightY(int def) {
    switch (def) { case W_AR: return g_akSightY; case W_SMG: return 0.055f; case W_SNIPER: return 0.085f; case W_SHOTGUN: return 0.037f; default: return 0.049f; }
}
static float muzzleZ(int def) {
    switch (def) { case W_AR: return g_akMuzzleZ + kAkVmZ; case W_SMG: return 0.29f; case W_SNIPER: return 0.76f; case W_SHOTGUN: return 0.66f; default: return 0.16f; }
}
static float g_vmLagX = 0, g_vmLagY = 0;
static void drawViewModel(float dt) {
    Player& P = G.P;
    std::fill(g_zb.begin(), g_zb.end(), 0.0f);
    float oldNear = g_cam.nearZ; g_cam.nearZ = 0.02f;
    float vfov = 62 * DEG;
    float savedFocal = g_cam.focal;
    g_cam.focal = (RH * (16.0f / 9.0f) * 0.5f) / tanf(vfov * 0.5f * lerpf(1.0f, 0.8f, P.adsT));
    g_vmFocal = g_cam.focal;
    if (P.dead || scopeView() || P.tank) { g_cam.nearZ = oldNear; g_cam.focal = savedFocal; return; }
    g_inVM = g_useGL;
    g_vmLagX = lerpf(g_vmLagX, clampf(IN.lookX * 0.004f, -0.05f, 0.05f), std::min(1.0f, dt * 10));
    g_vmLagY = lerpf(g_vmLagY, clampf(IN.lookY * 0.004f, -0.05f, 0.05f), std::min(1.0f, dt * 10));
    const WState& W = P.w[P.slot];
    bool dragging = P.carry >= 0 && G.iv[22] == 2;
    float bobX = sinf(P.bobPh) * 0.014f * P.bobAmt * (1 - P.adsT * 0.85f);
    float bobY = -fabsf(cosf(P.bobPh)) * 0.012f * P.bobAmt * (1 - P.adsT * 0.85f);
    float yawO = -g_vmLagX * (1 - P.adsT), pitO = g_vmLagY * (1 - P.adsT), rollO = 0;
    V3 pos;
    if (W.def >= 0 && !dragging) {
        V3 hip(0.12f, -0.14f, 0.46f), ads(0, -sightY(W.def), W.def == W_AR ? 0.34f : 0.30f);
        if (W.def == W_PISTOL) { hip = V3(0.1f, -0.12f, 0.4f); ads = V3(0, -sightY(W.def), 0.36f); }
        pos = lerpv(hip, ads, P.adsT) + V3(bobX, bobY, 0);
        pos.z -= 0.04f * P.kick; pitO += 0.09f * P.kick * (1 - P.adsT * 0.6f);
        if (W.reloadT > 0 && W.reloadDur > 0) {
            float ph = 1 - W.reloadT / W.reloadDur; float d = sinf(ph * PI_F);
            pos.y -= 0.09f * d; rollO += 0.55f * d; pitO -= 0.25f * d;
        }
        if (W.boltT > 0 && WD[W.def].cycle > 0) { float d = sinf(clampf(W.boltT / WD[W.def].cycle, 0, 1) * PI_F); rollO += 0.25f * d; pos.y -= 0.02f * d; }
        if (P.switchT > 0) { float d = P.switchT > 0.3f ? (0.6f - P.switchT) / 0.3f : P.switchT / 0.3f; pos.y -= 0.3f * d; pitO -= 0.6f * d; }
        if (P.sprint) { pos.x -= 0.04f; pos.y -= 0.03f; yawO += 0.6f; rollO += 0.35f; }
        if (P.throwAnim > 0 || P.cookType >= 0) { pos.y -= 0.12f; pitO -= 0.3f; }
        Basis B = basisYPR(yawO, pitO, rollO);
        const VPart* parts; int n;
        switch (W.def) {
        case W_AR: parts = kVAR; n = (int)(sizeof(kVAR) / sizeof(VPart)); break;
        case W_SMG: parts = kVSMG; n = (int)(sizeof(kVSMG) / sizeof(VPart)); break;
        case W_SNIPER: parts = kVSN; n = (int)(sizeof(kVSN) / sizeof(VPart)); break;
        case W_SHOTGUN: parts = kVSG; n = (int)(sizeof(kVSG) / sizeof(VPart)); break;
        default: parts = kVPI; n = (int)(sizeof(kVPI) / sizeof(VPart)); break;
        }
        bool reloadMagOut = W.reloadT > 0 && !WD[W.def].shellReload && (1 - W.reloadT / std::max(0.01f, W.reloadDur)) > 0.2f && (1 - W.reloadT / std::max(0.01f, W.reloadDur)) < 0.55f;
        if (W.def == W_AR) {
            meshInit();
            drawMesh(g_mAk, 0, (int)(g_mAk.t.size() / 3), B, pos + bx(B, V3(0, 0, kAkVmZ)), true, 1.0f, reloadMagOut ? g_akMag0 : -1, reloadMagOut ? g_akMag1 : -1);
            n = 0;
        }
        for (int i = 0; i < n; i++) {
            const VPart& q = parts[i];
            if (reloadMagOut && q.cy < -0.09f && q.hy > 0.06f) continue;
            V3 lc(q.cx, q.cy, q.cz);
            if (W.def == W_SHOTGUN && i == 2 && W.boltT > 0) lc.z -= 0.06f * sinf(clampf(W.boltT / 0.75f, 0, 1) * PI_F);
            uint32_t col = shadeC(q.col, 1.0f);
            drawOBB(pos + bx(B, lc), B.r, B.u, B.f, V3(q.hx, q.hy, q.hz), col, true);
        }
        if (W.silenced && W.def == W_AR) drawOBB(pos + bx(B, V3(0, -0.017f, muzzleZ(W.def) + 0.075f)), B.r, B.u, B.f, V3(0.016f, 0.016f, 0.075f), VC_D, true);
        else if (W.silenced) drawOBB(pos + bx(B, V3(0, 0.02f, 0.22f)), B.r, B.u, B.f, V3(0.014f, 0.014f, 0.07f), VC_D, true);
        uint32_t glove = RGBc(45, 45, 40), sleeve = P.ghillie ? RGBc(80, 95, 55) : RGBc(85, 92, 62);
        if (W.def == W_PISTOL) {
            drawOBB(pos + bx(B, V3(0, -0.07f, -0.03f)), B.r, B.u, B.f, V3(0.032f, 0.035f, 0.035f), glove, true);
            drawOBB(pos + bx(B, V3(0.05f, -0.12f, -0.15f)), B.r, B.u, B.f, V3(0.035f, 0.035f, 0.11f), sleeve, true);
            drawOBB(pos + bx(B, V3(-0.05f, -0.12f, -0.14f)), B.r, B.u, B.f, V3(0.035f, 0.035f, 0.11f), sleeve, true);
        } else {
            drawOBB(pos + bx(B, V3(0, -0.075f, -0.03f)), B.r, B.u, B.f, V3(0.034f, 0.04f, 0.04f), glove, true);
            drawOBB(pos + bx(B, V3(0.06f, -0.13f, -0.15f)), B.r, B.u, B.f, V3(0.04f, 0.04f, 0.12f), sleeve, true);
            float lz = (reloadMagOut ? 0.1f : 0.28f);
            if (W.def == W_SNIPER) lz = 0.2f;
            drawOBB(pos + bx(B, V3(0, -0.05f, lz)), B.r, B.u, B.f, V3(0.035f, 0.035f, 0.04f), glove, true);
            drawOBB(pos + bx(B, V3(-0.08f, -0.11f, lz - 0.1f)), B.r, B.u, B.f, V3(0.04f, 0.04f, 0.13f), sleeve, true);
        }
        if (P.muzzle > 0 && !W.silenced) {
            V3 m = pos + bx(B, V3(0, W.def == W_AR ? -0.017f : 0.005f, muzzleZ(W.def) + 0.03f));
            float s = 0.05f + frand() * 0.03f;
            V3 a[3] = {m + B.r * s, m - B.r * s * 0.5f + B.u * s, m - B.r * s * 0.5f - B.u * s};
            V3 b[3] = {m - B.r * s, m + B.r * s * 0.5f + B.u * s, m + B.r * s * 0.5f - B.u * s};
            drawPolyCam(a, 3, RGBc(255, 200, 110), RM_ADD);
            drawPolyCam(b, 3, RGBc(255, 170, 80), RM_ADD);
        }
    }
    if (P.cookType >= 0) {
        V3 gp(-0.13f, -0.075f, 0.33f);
        Basis gb; gb.r = V3(1, 0, 0); gb.u = V3(0, 1, 0); gb.f = V3(0, 0, 1);
        drawGrenadeModel(gp, gb, P.cookType, true);
        drawOBB(gp + V3(0.03f, -0.1f, -0.07f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(0.04f, 0.04f, 0.1f), RGBc(85, 92, 62), true);
    }
    if (dragging) {
        drawOBB(V3(0.14f, -0.25f, 0.4f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(0.045f, 0.045f, 0.2f), RGBc(85, 92, 62), true);
        drawOBB(V3(-0.14f, -0.25f, 0.4f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(0.045f, 0.045f, 0.2f), RGBc(85, 92, 62), true);
    }
    g_inVM = false;
    g_cam.nearZ = oldNear; g_cam.focal = savedFocal;
}

// =====================================================================
//  РЕНДЕР НА ВИДЕОКАРТЕ: OpenGL ES 2.0 (или OpenGL 2.1 на ПК).
//  Попиксельное освещение, тени от солнца, процедурные текстуры в шейдере,
//  туман, небо с солнцем, постэффекты. Если контекст не создаётся — программный рендер.
// =====================================================================
#if defined(_WIN32)
#define GROM_GLAPI __stdcall
#else
#define GROM_GLAPI
#endif
namespace gk {
const unsigned DEPTH_BUFFER_BIT = 0x100, COLOR_BUFFER_BIT = 0x4000, TRIANGLES = 0x0004, DEPTH_TEST = 0x0B71, BLEND = 0x0BE2,
               CULL_FACE = 0x0B44, POLYGON_OFFSET_FILL = 0x8037, SRC_ALPHA = 0x0302, ONE_MINUS_SRC_ALPHA = 0x0303, ONE = 1, ZERO = 0,
               DST_COLOR = 0x0306, LEQUAL = 0x0203, ARRAY_BUFFER = 0x8892, STATIC_DRAW = 0x88E4, STREAM_DRAW = 0x88E0, FLOAT = 0x1406,
               UNSIGNED_BYTE = 0x1401, TEXTURE_2D = 0x0DE1, TEXTURE0 = 0x84C0, TEXTURE_MIN_FILTER = 0x2801, TEXTURE_MAG_FILTER = 0x2800,
               LINEAR = 0x2601, NEAREST = 0x2600, TEXTURE_WRAP_S = 0x2802, TEXTURE_WRAP_T = 0x2803, REPEAT = 0x2901, CLAMP_TO_EDGE = 0x812F,
               RGBA = 0x1908, FRAMEBUFFER = 0x8D40, RENDERBUFFER = 0x8D41, COLOR_ATTACHMENT0 = 0x8CE0, DEPTH_ATTACHMENT = 0x8D00,
               DEPTH_COMPONENT16 = 0x81A5, DEPTH_COMPONENT24 = 0x81A6, FRAMEBUFFER_COMPLETE = 0x8CD5, VERTEX_SHADER = 0x8B31,
               FRAGMENT_SHADER = 0x8B30, COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82, EXTENSIONS = 0x1F03, FRAMEBUFFER_BINDING = 0x8CA6;
}
struct GLFn {
    void(GROM_GLAPI* Clear)(unsigned);
    void(GROM_GLAPI* ClearColor)(float, float, float, float);
    void(GROM_GLAPI* Viewport)(int, int, int, int);
    void(GROM_GLAPI* Enable)(unsigned);
    void(GROM_GLAPI* Disable)(unsigned);
    void(GROM_GLAPI* BlendFunc)(unsigned, unsigned);
    void(GROM_GLAPI* DepthMask)(unsigned char);
    void(GROM_GLAPI* DepthFunc)(unsigned);
    void(GROM_GLAPI* PolygonOffset)(float, float);
    unsigned(GROM_GLAPI* CreateShader)(unsigned);
    void(GROM_GLAPI* ShaderSource)(unsigned, int, const char* const*, const int*);
    void(GROM_GLAPI* CompileShader)(unsigned);
    void(GROM_GLAPI* GetShaderiv)(unsigned, unsigned, int*);
    void(GROM_GLAPI* GetShaderInfoLog)(unsigned, int, int*, char*);
    unsigned(GROM_GLAPI* CreateProgram)();
    void(GROM_GLAPI* AttachShader)(unsigned, unsigned);
    void(GROM_GLAPI* BindAttribLocation)(unsigned, unsigned, const char*);
    void(GROM_GLAPI* LinkProgram)(unsigned);
    void(GROM_GLAPI* GetProgramiv)(unsigned, unsigned, int*);
    void(GROM_GLAPI* GetProgramInfoLog)(unsigned, int, int*, char*);
    void(GROM_GLAPI* UseProgram)(unsigned);
    int(GROM_GLAPI* GetUniformLocation)(unsigned, const char*);
    void(GROM_GLAPI* Uniform1i)(int, int);
    void(GROM_GLAPI* Uniform1f)(int, float);
    void(GROM_GLAPI* Uniform2f)(int, float, float);
    void(GROM_GLAPI* Uniform3f)(int, float, float, float);
    void(GROM_GLAPI* Uniform4f)(int, float, float, float, float);
    void(GROM_GLAPI* UniformMatrix4fv)(int, int, unsigned char, const float*);
    void(GROM_GLAPI* GenBuffers)(int, unsigned*);
    void(GROM_GLAPI* BindBuffer)(unsigned, unsigned);
    void(GROM_GLAPI* BufferData)(unsigned, ptrdiff_t, const void*, unsigned);
    void(GROM_GLAPI* DeleteBuffers)(int, const unsigned*);
    void(GROM_GLAPI* EnableVertexAttribArray)(unsigned);
    void(GROM_GLAPI* DisableVertexAttribArray)(unsigned);
    void(GROM_GLAPI* VertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void*);
    void(GROM_GLAPI* DrawArrays)(unsigned, int, int);
    void(GROM_GLAPI* GenTextures)(int, unsigned*);
    void(GROM_GLAPI* BindTexture)(unsigned, unsigned);
    void(GROM_GLAPI* TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    void(GROM_GLAPI* TexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    void(GROM_GLAPI* TexParameteri)(unsigned, unsigned, int);
    void(GROM_GLAPI* ActiveTexture)(unsigned);
    void(GROM_GLAPI* DeleteTextures)(int, const unsigned*);
    void(GROM_GLAPI* GenFramebuffers)(int, unsigned*);
    void(GROM_GLAPI* BindFramebuffer)(unsigned, unsigned);
    void(GROM_GLAPI* FramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int);
    void(GROM_GLAPI* GenRenderbuffers)(int, unsigned*);
    void(GROM_GLAPI* BindRenderbuffer)(unsigned, unsigned);
    void(GROM_GLAPI* RenderbufferStorage)(unsigned, unsigned, int, int);
    void(GROM_GLAPI* FramebufferRenderbuffer)(unsigned, unsigned, unsigned, unsigned);
    unsigned(GROM_GLAPI* CheckFramebufferStatus)(unsigned);
    void(GROM_GLAPI* DeleteFramebuffers)(int, const unsigned*);
    void(GROM_GLAPI* DeleteRenderbuffers)(int, const unsigned*);
    void(GROM_GLAPI* GetIntegerv)(unsigned, int*);
    const unsigned char*(GROM_GLAPI* GetString)(unsigned);
};
static GLFn GLF;
static SDL_GLContext g_glc = nullptr;
static bool g_glDesktop = false;
template <typename T> static bool glLoad(T& fn, const char* name) {
    void* p = SDL_GL_GetProcAddress(name);
    memcpy(&fn, &p, sizeof(p));
    return p != nullptr;
}
static bool glLoadAll() {
    bool ok = true;
#define GL_L(f) ok = glLoad(GLF.f, "gl" #f) && ok
    GL_L(Clear); GL_L(ClearColor); GL_L(Viewport); GL_L(Enable); GL_L(Disable); GL_L(BlendFunc); GL_L(DepthMask); GL_L(DepthFunc);
    GL_L(PolygonOffset); GL_L(CreateShader); GL_L(ShaderSource); GL_L(CompileShader); GL_L(GetShaderiv); GL_L(GetShaderInfoLog);
    GL_L(CreateProgram); GL_L(AttachShader); GL_L(BindAttribLocation); GL_L(LinkProgram); GL_L(GetProgramiv); GL_L(GetProgramInfoLog);
    GL_L(UseProgram); GL_L(GetUniformLocation); GL_L(Uniform1i); GL_L(Uniform1f); GL_L(Uniform2f); GL_L(Uniform3f); GL_L(Uniform4f);
    GL_L(UniformMatrix4fv); GL_L(GenBuffers); GL_L(BindBuffer); GL_L(BufferData); GL_L(DeleteBuffers); GL_L(EnableVertexAttribArray);
    GL_L(DisableVertexAttribArray); GL_L(VertexAttribPointer); GL_L(DrawArrays); GL_L(GenTextures); GL_L(BindTexture); GL_L(TexImage2D);
    GL_L(TexSubImage2D); GL_L(TexParameteri); GL_L(ActiveTexture); GL_L(DeleteTextures); GL_L(GenFramebuffers); GL_L(BindFramebuffer);
    GL_L(FramebufferTexture2D); GL_L(GenRenderbuffers); GL_L(BindRenderbuffer); GL_L(RenderbufferStorage); GL_L(FramebufferRenderbuffer);
    GL_L(CheckFramebufferStatus); GL_L(DeleteFramebuffers); GL_L(DeleteRenderbuffers); GL_L(GetIntegerv); GL_L(GetString);
#undef GL_L
    return ok;
}

// ------------------------------------------------------------ шейдеры
static const char* kVsMain = R"(
attribute vec3 aPos; attribute vec3 aNrm; attribute vec4 aCol; attribute vec2 aUV; attribute vec3 aPar;
uniform mat4 uVP; uniform mat4 uSVP; uniform vec4 uWind; uniform vec4 uTileOff;
varying vec3 vW; varying vec3 vN; varying vec4 vC; varying vec2 vUV; varying vec3 vP; varying vec4 vS;
void main() {
    vec3 p = aPos; vec3 par = aPar;
    float fadeS = 42.0, fadeL = 22.0;
    if (aPar.z > 5.5) {   // плитка травяного ковра: сдвиг к месту, зеркалирование для разнообразия
        if (uTileOff.w < 0.0) p.x = 16.0 - p.x;
        p.xz += uTileOff.xy; p.y += uTileOff.z;
        fadeS = 27.0; fadeL = 12.0;
    }
    if (aPar.z > 4.5) {   // травинка: aPar.y — доля высоты; качание ветром, исчезает вдали
        float k = aPar.y * aPar.y, t = uWind.x;
        float dist = length(p.xz - uWind.zw);
        p.y = mix(p.y, uWind.y, clamp((dist - fadeS) / fadeL, 0.0, 1.0));
        float gust = 0.6 + 0.4 * sin(t * 0.45 + p.x * 0.05);
        p.x += (sin(t * 1.9 + p.x * 0.37 + p.z * 0.23) * 0.07 + sin(t * 0.8 + p.z * 0.11) * 0.06) * k * gust;
        p.z += cos(t * 1.55 + p.z * 0.29 + p.x * 0.13) * 0.05 * k * gust;
        par = vec3(aPar.x, -1000.0, 0.0);
    }
    vW = p; vN = aNrm; vC = aCol; vUV = aUV; vP = par;
    vS = uSVP * vec4(p, 1.0);
    gl_Position = uVP * vec4(p, 1.0);
}
)";
static const char* kFsMain = R"(
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif
uniform vec3 uSun; uniform vec3 uSunCol; uniform vec3 uSkyAmb; uniform vec3 uGndAmb;
uniform vec3 uFog; uniform vec2 uFogR; uniform vec3 uCam; uniform float uTime; uniform float uShadowOn; uniform float uShadowTexel;
uniform sampler2D uNoise; uniform sampler2D uShadow; uniform sampler2D uAtlas; uniform sampler2D uGMask;
uniform vec4 uGRect; uniform vec3 uGrassA; uniform vec3 uGrassB;
varying vec3 vW; varying vec3 vN; varying vec4 vC; varying vec2 vUV; varying vec3 vP; varying vec4 vS;
float N(vec2 p) { return texture2D(uNoise, p).r; }
float F(vec2 p) { return texture2D(uNoise, p).g; }
float H(vec2 c) { return texture2D(uNoise, (c + 0.5) / 256.0).g; }
float texMul(float id, vec2 uv) {
    float m = 1.0;
    if (id < 0.5) return 1.0;
    else if (id < 1.5) { m = 1.0 + (N(uv / 32.0) - 0.5) * 0.28 + (F(uv) - 0.5) * 0.06; }
    else if (id < 2.5) {
        float row = floor(uv.y / 0.19); float x = uv.x / 0.5 + mod(row, 2.0) * 0.5;
        vec2 f = vec2(fract(x), fract(uv.y / 0.19));
        float mortar = max(step(f.y, 0.13), step(f.x, 0.06));
        m = mix(0.98 + (H(vec2(floor(x), row)) - 0.5) * 0.25 - (F(uv * 0.7) - 0.5) * 0.05, 0.72, mortar);
    }
    else if (id < 3.5) { vec2 g = fract(uv / 1.5); float seam = max(step(g.x, 0.015), step(g.y, 0.015)); m = mix(1.0 + (N(uv / 16.0) - 0.5) * 0.18 + (F(uv * 0.5) - 0.5) * 0.05, 0.8, seam); }
    else if (id < 4.5) { float fy = fract(uv.y / 0.19); m = mix(0.97 + (N(vec2(uv.x / 32.0, uv.y * 0.5)) - 0.5) * 0.3, 0.72, step(fy, 0.1)); }
    else if (id < 5.5) { m = 0.9 + 0.1 * sin(uv.x * 52.0) + (N(uv / 12.0) - 0.5) * 0.12; }
    else if (id < 6.5) { float fy = fract(uv.y * 4.0); float col = floor(uv.x / 0.3 + floor(uv.y * 4.0) * 0.5); m = (1.02 - fy * 0.18 + (H(vec2(col, floor(uv.y * 4.0))) - 0.5) * 0.12) * mix(0.7, 1.0, step(0.08, fy)); }
    else if (id < 7.5) { m = 0.98 + (N(uv / 24.0) - 0.5) * 0.55 + (N(uv / 3.0) - 0.5) * 0.2; }
    else if (id < 8.5) { m = 1.0 + (N(uv / 96.0) - 0.5) * 0.4 + (N(uv / 12.0) - 0.5) * 0.16 + (F(uv * 2.0) - 0.5) * 0.08; }
    else if (id < 9.5) { float row = floor(uv.y * 4.0); float x = uv.x / 0.44 + mod(row, 2.0) * 0.5; vec2 f = vec2(fract(x), fract(uv.y * 4.0)) - 0.5; m = 1.02 - dot(f, f) * 0.9; }
    else if (id < 10.5) { m = 1.0 + (N(uv / 24.0) - 0.5) * 0.1 + (F(uv * 3.0) - 0.5) * 0.1; }
    else if (id < 11.95) {   // дорога: uv.x — поперёк от оси, uv.y — вдоль; полуширина в дробной части номера
        float hw = (id - 11.0) * 20.0, ax = abs(uv.x);
        m = 0.95 + (N(uv / 7.0) - 0.5) * 0.14 + (F(uv * 2.5) - 0.5) * 0.14;
        if (H(floor(vec2((uv.x + 50.0) / 1.7, uv.y / 4.3))) > 0.88) m *= 0.86;                      // заплаты
        float cr = abs(N(uv * 0.7 + 3.1) - 0.5);
        m *= mix(0.62, 1.0, smoothstep(0.004, 0.02, cr));                                           // трещины
        float lane = hw * 0.5, t1 = abs(ax - lane + 0.85), t2 = abs(ax - lane - 0.85), t = min(t1, t2);
        m *= 1.0 - 0.09 * exp(-t * t * 6.0);                                                        // накатанная колея
        m *= mix(1.0, 0.8, smoothstep(hw - 0.5, hw - 0.05, ax));                                    // грязь у бордюра
    }
    else if (id < 12.5) {   // брусчатка
        float row = floor(uv.y / 0.24), x = uv.x / 0.2 + mod(row, 2.0) * 0.5;
        vec2 f = vec2(fract(x), fract(uv.y / 0.24)) - 0.5;
        float joint = smoothstep(0.37, 0.47, max(abs(f.x), abs(f.y)));
        m = mix(0.93 + (H(vec2(floor(x), row)) - 0.5) * 0.3 - dot(f, f) * 0.3, 0.6, joint) + (N(uv / 9.0) - 0.5) * 0.12;
    }
    else if (id < 13.5) {   // плитка тротуара
        vec2 g = fract(uv / 0.5);
        float seam = max(step(g.x, 0.03), step(g.y, 0.03));
        m = mix(1.02 + (H(floor(uv / 0.5)) - 0.5) * 0.12 + (F(uv) - 0.5) * 0.06, 0.78, seam);
    }
    else if (id < 14.95) {   // грунтовка: укатанные колеи
        float ax = abs(uv.x), rut = exp(-(ax - 0.95) * (ax - 0.95) * 14.0);
        m = 1.0 + (N(uv / 5.0) - 0.5) * 0.25 + (F(uv * 2.0) - 0.5) * 0.12 - rut * 0.18;
    }
    float d = length(vW - uCam);
    return mix(1.0, m, clamp((90.0 - d) / 50.0, 0.0, 1.0));
}
float unpackD(vec4 c) { return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0)); }
float shadowF(float ndl) {
    if (uShadowOn < 0.5) return 1.0;
    vec3 p = vS.xyz * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
    float bias = 0.0015 + 0.004 * (1.0 - ndl);
    float s = 0.0; float t = uShadowTexel;
    s += step(p.z - bias, unpackD(texture2D(uShadow, p.xy + vec2(-t, -t))));
    s += step(p.z - bias, unpackD(texture2D(uShadow, p.xy + vec2(t, -t))));
    s += step(p.z - bias, unpackD(texture2D(uShadow, p.xy + vec2(-t, t))));
    s += step(p.z - bias, unpackD(texture2D(uShadow, p.xy + vec2(t, t))));
    return s * 0.25;
}
void main() {
    vec4 c = vC; float fl = vP.z;
    vec3 col = c.rgb; float a = c.a;
    if (fl > 2.5 && fl < 3.5) {
        float r = dot(vUV, vUV);
        if (r > 1.0) discard;
        a *= 1.0 - r * r;
    } else if (fl > 3.5) {
        float w = sin(vW.x * 0.35 + uTime * 1.3) * sin(vW.z * 0.27 - uTime * 1.1);
        col *= 0.85 + 0.3 * w * w + (N(vW.xz / 40.0 + uTime * 0.01) - 0.5) * 0.3;
    } else if (fl < 0.5) {
        vec3 n = normalize(vN);
        float m = 1.0;
        if (vP.x > 21.5) {   // травяной ковёр: густота и сухость из маски уровня
            vec4 mk = texture2D(uGMask, (vW.xz - uGRect.xy) * uGRect.zw);
            if (mk.r < c.a) discard;
            vec4 tg = texture2D(uAtlas, vUV); if (tg.a < 0.5) discard;
            vec3 gc = mix(uGrassA, uGrassB, mk.g) * (0.92 + 0.16 * N(vW.xz / 7.0));
            col = gc * col * tg.rgb * 1.25; a = 1.0;
        } else if (vP.x > 20.5) { vec4 tg = texture2D(uAtlas, vUV); if (tg.a < 0.5) discard; col *= tg.rgb * 1.1; }
        else if (vP.x > 19.5) col *= texture2D(uAtlas, vUV).rgb; else m = texMul(vP.x, vUV);
        if (vP.x > 7.5 && vP.x < 8.5) {
            if (dot(col, vec3(0.333)) > 0.72) {   // снег: наветренные гребни и искры
                float dr = N(vec2(vW.x / 30.0, vW.z / 6.0));
                col *= 0.94 + 0.1 * dr;
                col += vec3(0.3) * step(0.994, F(vW.xz * 3.0)) * clamp((26.0 - length(vW - uCam)) / 20.0, 0.0, 1.0);
            } else {                              // земля: пятна сухой и сочной травы, проплешины
                float g1 = N(vW.xz / 34.0), g2 = N(vW.xz / 9.0 + 0.37);
                col *= mix(vec3(1.07, 1.02, 0.86), vec3(0.88, 1.0, 0.9), g1) * (0.94 + 0.12 * g2);
            }
        }
        if (vP.x > 10.95 && vP.x < 11.95) {   // разметка (стёртая краска) и люки
            float hw = (vP.x - 11.0) * 20.0, ax = abs(vUV.x);
            float edge = step(abs(ax - (hw - 0.35)), 0.07);
            float center = hw > 2.6 ? step(ax, 0.07) * step(fract(vUV.y / 6.0), 0.5) : 0.0;
            float paint = max(edge, center) * smoothstep(0.25, 0.4, N(vUV * 1.7 + 5.0));
            col = mix(col, vec3(0.86, 0.85, 0.8) / max(m, 0.5), paint * 0.85);
            vec2 mh = vec2(ax - hw * 0.5, mod(vUV.y, 23.0) - 11.5); float r = length(mh);
            if (r < 0.38) col *= r > 0.33 ? 0.55 : 0.68 + 0.1 * step(0.5, fract((mh.x + mh.y) * 6.0));
        }
        if (vP.x > 13.95 && vP.x < 14.95) {   // грунтовка в снегу: грязные колеи, белые обочины
            float hw = (vP.x - 14.0) * 20.0, ax = abs(vUV.x);
            float rut = exp(-(ax - 0.95) * (ax - 0.95) * 10.0);
            col = mix(col, vec3(0.5, 0.46, 0.42), rut * 0.45);
            col = mix(col, vec3(0.93, 0.94, 0.97), smoothstep(hw - 1.3, hw, ax) * 0.8);
        }
        if (vP.y > -999.0 && abs(n.y) < 0.5) m *= clamp((vW.y - vP.y) * 1.4 + 0.6, 0.6, 1.0);
        float ndl = max(dot(n, uSun), 0.0);
        float sh = ndl > 0.0 ? shadowF(ndl) : 1.0;
        vec3 amb = mix(uGndAmb, uSkyAmb, n.y * 0.5 + 0.5);
        vec3 v = normalize(uCam - vW);
        float spec = pow(max(dot(reflect(-uSun, n), v), 0.0), 24.0) * 0.12 * sh;
        col = col * m * (amb + uSunCol * ndl * sh) + uSunCol * spec;
    }
    float d = length(vW - uCam);
    float f = clamp((d - uFogR.x) / (uFogR.y - uFogR.x), 0.0, 1.0);
    if (fl < 2.5 || fl > 3.5) col = mix(col, uFog, f);
    else a *= 1.0 - f;
    gl_FragColor = vec4(col, a);
}
)";
static const char* kVsDepth = R"(
attribute vec3 aPos; attribute vec3 aPar;
uniform mat4 uSVP; varying float vD; varying float vF;
void main() { vec4 p = uSVP * vec4(aPos, 1.0); vD = p.z * 0.5 + 0.5; vF = aPar.z; gl_Position = p; }
)";
static const char* kFsDepth = R"(
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif
varying float vD; varying float vF;
void main() {
    if (vF > 0.5) discard;
    vec4 e = fract(vec4(1.0, 255.0, 65025.0, 16581375.0) * vD);
    e -= e.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
    gl_FragColor = e;
}
)";
static const char* kVsSky = R"(
attribute vec2 aXY; attribute vec3 aDir; varying vec3 vDir;
void main() { vDir = aDir; gl_Position = vec4(aXY, 0.999, 1.0); }
)";
static const char* kFsSky = R"(
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif
uniform vec3 uTop; uniform vec3 uHor; uniform vec3 uFog; uniform vec3 uSun; uniform vec3 uSunCol; uniform float uFlash;
uniform float uTime; uniform float uCloud; uniform sampler2D uNoise;
varying vec3 vDir;
float fbm(vec2 p) {
    return texture2D(uNoise, p).r * 0.47 + texture2D(uNoise, p * 2.13 + 0.37).r * 0.23
         + texture2D(uNoise, p * 1.07 + 0.61).b * 0.18 + texture2D(uNoise, p * 2.71 + 0.13).b * 0.12;
}
void main() {
    vec3 d = normalize(vDir);
    float h = max(d.y, 0.0);
    float mu = dot(d, uSun);
    float smu = max(mu, 0.0);
    float sunMax = max(uSunCol.r, max(uSunCol.g, uSunCol.b));
    vec3 sunTint = uSunCol / max(0.001, sunMax);
    // рассеяние: глубокий зенит, светлый горизонт, ореол вокруг солнца
    vec3 sky = mix(uHor, uTop, 1.0 - exp(-h * 3.2));
    sky += uHor * sunTint * (0.28 * pow(smu, 3.0) * (1.0 - h * 0.7));
    sky *= 1.0 - 0.14 * max(-mu, 0.0) * h;
    float disc = smoothstep(0.99955, 0.99985, mu);
    sky += uSunCol * (disc * 5.0 + pow(smu, 700.0) * 1.4 + pow(smu, 48.0) * 0.32 + pow(smu, 7.0) * 0.07);
    // облака: две высоты, ветер, подсветка со стороны солнца
    if (d.y > 0.0) {
        float k = 1.0 / (d.y + 0.11);
        vec2 wind = vec2(uTime * 0.0021, uTime * 0.0009);
        vec2 uv = d.xz * k * 0.085 + wind;
        float f = fbm(uv);
        float thr = 1.0 - uCloud;
        float c = smoothstep(thr - 0.05, thr + 0.24, f);
        float f2 = fbm(uv + uSun.xz * 0.03);
        float lit = clamp(0.62 + (f - f2) * 4.0, 0.15, 1.0);
        vec3 shadowC = mix(uTop, uHor, 0.5) * 0.52 + vec3(0.03);
        vec3 litC = uHor * 0.75 + uSunCol * 0.62;
        vec3 cc = mix(shadowC, litC, lit);
        cc = mix(cc, cc * 0.72, smoothstep(0.6, 1.0, uCloud) * c);           // тучи темнее
        cc += uSunCol * pow(smu, 10.0) * (1.0 - c) * 0.9;                    // серебристая кромка
        // перистые облака выше
        vec2 uv2 = d.xz * k * 0.03 + wind * 1.7 + 0.5;
        float ci = smoothstep(0.55, 0.85, texture2D(uNoise, vec2(uv2.x * 3.0, uv2.y * 0.6)).b * 0.6 + texture2D(uNoise, uv2).r * 0.5);
        float fade = smoothstep(0.0, 0.22, d.y);
        sky = mix(sky, uHor * 0.9 + uSunCol * 0.4, ci * fade * 0.28 * (1.0 - c));
        sky = mix(sky, cc, c * fade * 0.97);
    }
    // дымка у горизонта, ниже горизонта — цвет тумана
    sky = mix(sky, uFog, pow(1.0 - h, 7.0) * 0.65);
    if (d.y <= 0.0) sky = uFog;
    sky = mix(sky, vec3(0.86, 0.88, 1.0), uFlash);
    gl_FragColor = vec4(sky, 1.0);
}
)";
static const char* kVsQuad = R"(
attribute vec2 aXY; attribute vec2 aUV; varying vec2 vUV;
void main() { vUV = aUV; gl_Position = vec4(aXY, 0.0, 1.0); }
)";
static const char* kFsPost = R"(
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D uTex; uniform vec4 uFx; uniform vec4 uTint; uniform float uAspect;
varying vec2 vUV;
void main() {
    vec3 c = texture2D(uTex, vUV).rgb;
    vec2 q = vUV - 0.5;
    float vig = clamp((length(vec2(q.x * 0.9, q.y)) * 2.0 - 0.55) / 0.6, 0.0, 1.0);
    c *= 1.0 - vig * vig * 0.25;
    c = mix(c, c * uTint.rgb, uTint.a);
    c = mix(c, vec3(0.55, 0.0, 0.0), vig * uFx.x);
    c *= 1.0 - max(uFx.z, vig * uFx.z * 1.6);
    c = mix(c, vec3(1.0), uFx.y);
    if (uFx.w > 0.5) {
        float r = length(vec2(q.x * uAspect, q.y)) / 0.47;
        c *= clamp((1.0 - r) * 12.0, 0.0, 1.0);
    }
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, 0.86);                                            // приглушённые цвета
    c = c * (1.0 + 0.08 * (c - 0.5));                                     // мягкая S-кривая
    c += vec3(0.012, 0.004, -0.012) * smoothstep(0.35, 0.9, l) + vec3(-0.01, 0.0, 0.012) * (1.0 - smoothstep(0.05, 0.4, l));
    c = (c - 0.5) * 1.07 + 0.5;
    gl_FragColor = vec4(c, 1.0);
}
)";
static const char* kFsUI = R"(
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D uTex; varying vec2 vUV;
void main() { gl_FragColor = texture2D(uTex, vUV).bgra; }
)";

struct GLProg { unsigned id = 0; int uVP = -1, uSVP = -1, uSun = -1, uSunCol = -1, uSkyAmb = -1, uGndAmb = -1, uFog = -1, uFogR = -1, uCam = -1, uTime = -1,
                uShadowOn = -1, uShadowTexel = -1, uNoise = -1, uShadow = -1, uTop = -1, uHor = -1, uFlash = -1, uTex = -1, uFx = -1, uTint = -1, uAspect = -1, uAtlas = -1, uCloud = -1, uWind = -1, uTileOff = -1, uGMask = -1, uGRect = -1, uGrassA = -1, uGrassB = -1; };
static GLProg g_pMain, g_pDepth, g_pSky, g_pPost, g_pUI;
static unsigned g_texNoise = 0, g_texAtlas = 0, g_glTexUI = 0, g_vboStatic = 0, g_vboGround = 0, g_vboDyn = 0, g_vboQuad = 0, g_vboGrass = 0, g_vboTileD = 0, g_vboTileS = 0, g_texGMask = 0;
static int g_nStatic = 0, g_nGround = 0, g_nGrass = 0, g_nTileD = 0, g_nTileS = 0, g_gmUploaded = -1, g_levelSerial = 0, g_builtSerial = -1;
static unsigned g_fbo = 0, g_fboTex = 0, g_fboDepth = 0; static int g_fboW = 0, g_fboH = 0;
static unsigned g_sfbo = 0, g_sfboTex = 0, g_sfboDepth = 0; static int g_sfboSize = 0;
static int g_defFbo = 0; static unsigned g_depthFmt = gk::DEPTH_COMPONENT16;
static float g_shadowVP[16];

static unsigned glCompile(unsigned type, const char* src) {
    std::string s = g_glDesktop ? std::string("#version 120\n") : std::string("#version 100\n");
    s += src;
    const char* p = s.c_str();
    unsigned sh = GLF.CreateShader(type);
    GLF.ShaderSource(sh, 1, &p, nullptr);
    GLF.CompileShader(sh);
    int ok = 0; GLF.GetShaderiv(sh, gk::COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; int n = 0; GLF.GetShaderInfoLog(sh, 1023, &n, log); log[std::max(0, std::min(n, 1023))] = 0; SDL_Log("Шейдер: %s", log); return 0; }
    return sh;
}
static bool glMakeProg(GLProg& P, const char* vs, const char* fs, const char* const* attrs, int nattr) {
    unsigned v = glCompile(gk::VERTEX_SHADER, vs), f = glCompile(gk::FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    P.id = GLF.CreateProgram();
    GLF.AttachShader(P.id, v); GLF.AttachShader(P.id, f);
    for (int i = 0; i < nattr; i++) GLF.BindAttribLocation(P.id, (unsigned)i, attrs[i]);
    GLF.LinkProgram(P.id);
    int ok = 0; GLF.GetProgramiv(P.id, gk::LINK_STATUS, &ok);
    if (!ok) { char log[1024]; int n = 0; GLF.GetProgramInfoLog(P.id, 1023, &n, log); log[std::max(0, std::min(n, 1023))] = 0; SDL_Log("Программа: %s", log); return false; }
    const char* names[29] = {"uVP", "uSVP", "uSun", "uSunCol", "uSkyAmb", "uGndAmb", "uFog", "uFogR", "uCam", "uTime", "uShadowOn", "uShadowTexel",
                             "uNoise", "uShadow", "uTop", "uHor", "uFlash", "uTex", "uFx", "uTint", "uAspect", "uAtlas", "uCloud", "uWind", "uTileOff", "uGMask", "uGRect", "uGrassA", "uGrassB"};
    int* slots[29] = {&P.uVP, &P.uSVP, &P.uSun, &P.uSunCol, &P.uSkyAmb, &P.uGndAmb, &P.uFog, &P.uFogR, &P.uCam, &P.uTime, &P.uShadowOn, &P.uShadowTexel,
                      &P.uNoise, &P.uShadow, &P.uTop, &P.uHor, &P.uFlash, &P.uTex, &P.uFx, &P.uTint, &P.uAspect, &P.uAtlas, &P.uCloud, &P.uWind, &P.uTileOff, &P.uGMask, &P.uGRect, &P.uGrassA, &P.uGrassB};
    for (int i = 0; i < 29; i++) *slots[i] = GLF.GetUniformLocation(P.id, names[i]);
    return true;
}
static void glTexParams(bool linear, bool repeat) {
    GLF.TexParameteri(gk::TEXTURE_2D, gk::TEXTURE_MIN_FILTER, (int)(linear ? gk::LINEAR : gk::NEAREST));
    GLF.TexParameteri(gk::TEXTURE_2D, gk::TEXTURE_MAG_FILTER, (int)(linear ? gk::LINEAR : gk::NEAREST));
    GLF.TexParameteri(gk::TEXTURE_2D, gk::TEXTURE_WRAP_S, (int)(repeat ? gk::REPEAT : gk::CLAMP_TO_EDGE));
    GLF.TexParameteri(gk::TEXTURE_2D, gk::TEXTURE_WRAP_T, (int)(repeat ? gk::REPEAT : gk::CLAMP_TO_EDGE));
}
static bool glInitRenderer(SDL_Window* win) {
    g_glc = SDL_GL_CreateContext(win);
    if (!g_glc) return false;
    if (!glLoadAll()) { SDL_GL_DeleteContext(g_glc); g_glc = nullptr; return false; }
    const char* ext = (const char*)GLF.GetString(gk::EXTENSIONS);
    if (g_glDesktop || (ext && strstr(ext, "GL_OES_depth24"))) g_depthFmt = gk::DEPTH_COMPONENT24;
    static const char* aMain[5] = {"aPos", "aNrm", "aCol", "aUV", "aPar"};
    static const char* aDepth[5] = {"aPos", "aNrm_", "aCol_", "aUV_", "aPar"};
    static const char* aSky[2] = {"aXY", "aDir"};
    static const char* aQuad[2] = {"aXY", "aUV"};
    if (!glMakeProg(g_pMain, kVsMain, kFsMain, aMain, 5) || !glMakeProg(g_pDepth, kVsDepth, kFsDepth, aDepth, 5) ||
        !glMakeProg(g_pSky, kVsSky, kFsSky, aSky, 2) || !glMakeProg(g_pPost, kVsQuad, kFsPost, aQuad, 2) || !glMakeProg(g_pUI, kVsQuad, kFsUI, aQuad, 2)) {
        SDL_GL_DeleteContext(g_glc); g_glc = nullptr; return false;
    }
    std::vector<uint8_t> nz(256 * 256 * 4);
    for (int i = 0; i < 256 * 256; i++) { nz[i * 4] = g_noise[i]; nz[i * 4 + 1] = g_fine[i]; nz[i * 4 + 2] = g_noise2[i]; nz[i * 4 + 3] = 255; }
    GLF.GenTextures(1, &g_texNoise); GLF.BindTexture(gk::TEXTURE_2D, g_texNoise);
    GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, 256, 256, 0, gk::RGBA, gk::UNSIGNED_BYTE, nz.data());
    glTexParams(true, true);
    meshInit();
    GLF.GenTextures(1, &g_texAtlas); GLF.BindTexture(gk::TEXTURE_2D, g_texAtlas);
    GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, ATL_W, ATL_H, 0, gk::RGBA, gk::UNSIGNED_BYTE, g_soAtlas.data());
    glTexParams(true, false);   // сглаживание: мягкие края травинок
    GLF.GenTextures(1, &g_glTexUI); GLF.BindTexture(gk::TEXTURE_2D, g_glTexUI);
    GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, UIW, UIH, 0, gk::RGBA, gk::UNSIGNED_BYTE, nullptr);
    glTexParams(false, false);
    GLF.GenBuffers(1, &g_vboStatic); GLF.GenBuffers(1, &g_vboGround); GLF.GenBuffers(1, &g_vboDyn); GLF.GenBuffers(1, &g_vboQuad); GLF.GenBuffers(1, &g_vboGrass);
    {   // плитка травяного ковра 16x16 м: плотная и разреженная версии
        std::vector<GV> D, Sp;
        uint32_t s = 0x2545F491u; auto rr = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFF) / 65535.0f; };
        const float T = 16.0f, step = 0.36f;
        int n = (int)(T / step), idx = 0;
        float v0 = (GRASS_ROW + GRASS_SZ - 0.5f) / ATL_H, v1 = (GRASS_ROW + 0.5f) / ATL_H;
        for (int iz = 0; iz < n; iz++)
            for (int ix = 0; ix < n; ix++, idx++) {
                V3 base((ix + rr()) * step, 0, (iz + rr()) * step);
                float h = 0.2f + rr() * rr() * 0.5f + rr() * 0.12f, w = h * (1.3f + rr() * 0.8f), a0 = rr() * 3.1416f, th = rr() * 0.97f;
                std::vector<GV>& B = (idx % 3 == 0) ? Sp : D;
                {
                    std::vector<GV>& Out = B;
                    uint32_t cb = glCol(RGBc(150, 150, 150), th), ct = glCol(RGBc(255, 255, 255), th);
                    for (int c = 0; c < 2; c++) {
                        float a = a0 + c * 1.5708f;
                        V3 sv(cosf(a) * w * 0.5f, 0, sinf(a) * w * 0.5f);
                        bool flip = rr() < 0.5f; float u0 = flip ? 1.0f : 0.0f, u1 = 1.0f - u0;
                        V3 nr = norm(V3(-sv.z * 0.3f, 1.0f, sv.x * 0.3f));
                        V3 b0 = base - sv, b1 = base + sv, t0 = base - sv * 1.1f + V3(0, h, 0), t1 = base + sv * 1.1f + V3(0, h, 0);
                        glPush(Out, b0, nr, cb, u0, v0, 22, 0, 6); glPush(Out, b1, nr, cb, u1, v0, 22, 0, 6); glPush(Out, t1, nr, ct, u1, v1, 22, 1, 6);
                        glPush(Out, b0, nr, cb, u0, v0, 22, 0, 6); glPush(Out, t1, nr, ct, u1, v1, 22, 1, 6); glPush(Out, t0, nr, ct, u0, v1, 22, 1, 6);
                    }
                }
            }
        GLF.GenBuffers(1, &g_vboTileD); GLF.GenBuffers(1, &g_vboTileS);
        GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboTileD); GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(D.size() * sizeof(GV)), D.data(), gk::STATIC_DRAW); g_nTileD = (int)D.size();
        GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboTileS); GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(Sp.size() * sizeof(GV)), Sp.data(), gk::STATIC_DRAW); g_nTileS = (int)Sp.size();
        GLF.GenTextures(1, &g_texGMask);
    }
    GLF.GetIntegerv(gk::FRAMEBUFFER_BINDING, &g_defFbo);
    return true;
}
static bool glEnsureFbo(unsigned& fbo, unsigned& tex, unsigned& depth, int w, int h, bool linear) {
    if (fbo) { GLF.DeleteFramebuffers(1, &fbo); GLF.DeleteTextures(1, &tex); GLF.DeleteRenderbuffers(1, &depth); }
    GLF.GenFramebuffers(1, &fbo); GLF.GenTextures(1, &tex); GLF.GenRenderbuffers(1, &depth);
    GLF.BindTexture(gk::TEXTURE_2D, tex);
    GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, w, h, 0, gk::RGBA, gk::UNSIGNED_BYTE, nullptr);
    glTexParams(linear, false);
    GLF.BindRenderbuffer(gk::RENDERBUFFER, depth);
    GLF.RenderbufferStorage(gk::RENDERBUFFER, g_depthFmt, w, h);
    GLF.BindFramebuffer(gk::FRAMEBUFFER, fbo);
    GLF.FramebufferTexture2D(gk::FRAMEBUFFER, gk::COLOR_ATTACHMENT0, gk::TEXTURE_2D, tex, 0);
    GLF.FramebufferRenderbuffer(gk::FRAMEBUFFER, gk::DEPTH_ATTACHMENT, gk::RENDERBUFFER, depth);
    bool ok = GLF.CheckFramebufferStatus(gk::FRAMEBUFFER) == gk::FRAMEBUFFER_COMPLETE;
    if (!ok && g_depthFmt != gk::DEPTH_COMPONENT16) {
        g_depthFmt = gk::DEPTH_COMPONENT16;
        GLF.RenderbufferStorage(gk::RENDERBUFFER, g_depthFmt, w, h);
        ok = GLF.CheckFramebufferStatus(gk::FRAMEBUFFER) == gk::FRAMEBUFFER_COMPLETE;
    }
    GLF.BindFramebuffer(gk::FRAMEBUFFER, (unsigned)g_defFbo);
    return ok;
}

// ------------------------------------------------------------ статическая геометрия уровня
static void glBuildStatic() {
    std::vector<GV> V, Gd;
    V.reserve(G.box.size() * 30);
    for (const Box& b : G.box) {
        if (!b.vis) continue;
        V3 w[8];
        for (int i = 0; i < 8; i++) w[i] = V3((i & 1) ? b.mx.x : b.mn.x, (i & 2) ? b.mx.y : b.mn.y, (i & 4) ? b.mx.z : b.mn.z);
        float ox = floorf(b.mn.x / 32) * 32, oy = floorf(b.mn.y / 32) * 32, oz = floorf(b.mn.z / 32) * 32;
        bool flat = (b.mx.y - b.mn.y) < 0.08f;
        for (int f = 0; f < 6; f++) {
            if (f == 3 && (b.mn.y <= G.groundY + 0.01f || flat)) continue;
            if (flat && f != 2) continue;
            V3 q[4] = {w[kFace[f][0]], w[kFace[f][1]], w[kFace[f][2]], w[kFace[f][3]]};
            float U[4], Vv[4];
            for (int k = 0; k < 4; k++) {
                if (f < 2) { U[k] = q[k].z - oz; Vv[k] = q[k].y - oy; } else if (f < 4) { U[k] = q[k].x - ox; Vv[k] = q[k].z - oz; } else { U[k] = q[k].x - ox; Vv[k] = q[k].y - oy; }
            }
            float ao = (f == 2 || f == 3) ? -1000.0f : b.mn.y;
            float tex = (float)b.tex;
            if ((b.tex == TX_ROAD || b.tex == TX_DIRT) && f == 2) {
                bool alongZ = (b.mx.z - b.mn.z) >= (b.mx.x - b.mn.x);
                float hw = alongZ ? (b.mx.x - b.mn.x) * 0.5f : (b.mx.z - b.mn.z) * 0.5f, cx = (b.mn.x + b.mx.x) * 0.5f, cz = (b.mn.z + b.mx.z) * 0.5f;
                for (int k = 0; k < 4; k++) { U[k] = alongZ ? q[k].x - cx : q[k].z - cz; Vv[k] = alongZ ? q[k].z - oz : q[k].x - ox; }
                tex = (b.tex == TX_ROAD ? 11.0f : 14.0f) + clampf(hw, 0.5f, 18.5f) / 20.0f;
            }
            glQuadLit(V, q, kFaceN[f], glCol(b.col, 1), U, Vv, tex, ao, b.glow ? 1.0f : 0.0f);
        }
    }
    // сетки окружения: горы, воронки, гарь (камень — процедурная текстура)
    for (const MTri& t : G.mtri) {
        V3 n = norm(cross(t.b - t.a, t.c - t.a));
        uint32_t c = glCol(t.col, 1);
        float tex = n.y < 0.97f ? (float)TX_ROCK : 0.0f;
        V3 v3[3] = {t.a, t.b, t.c};
        for (int k = 0; k < 3; k++) glPush(V, v3[k], n, c, v3[k].x + v3[k].z * 0.7f - floorf((v3[k].x + v3[k].z * 0.7f) / 64) * 64, v3[k].y - floorf(v3[k].y / 64) * 64, tex, -1000, 0);
    }
    // повёрнутый реквизит: ежи, проволока, колья
    for (const Prop& p : G.props) {
        V3 ax[3] = {p.B.r * p.h.x, p.B.u * p.h.y, p.B.f * p.h.z}, w[8];
        for (int i = 0; i < 8; i++) w[i] = p.c + ((i & 1) ? ax[0] : -ax[0]) + ((i & 2) ? ax[1] : -ax[1]) + ((i & 4) ? ax[2] : -ax[2]);
        V3 nrm[6] = {p.B.r, -p.B.r, p.B.u, -p.B.u, p.B.f, -p.B.f};
        for (int f = 0; f < 6; f++) { V3 q[4] = {w[kFace[f][0]], w[kFace[f][1]], w[kFace[f][2]], w[kFace[f][3]]}; glQuadLit(V, q, nrm[f], glCol(p.col, 1), nullptr, nullptr, 0, -1000, 0); }
    }
    for (const Prism& pr : G.prisms) {
        V3 c = (pr.mn + pr.mx) * 0.5f;
        float y0 = pr.mn.y, y1 = pr.mx.y;
        V3 a0, a1, b0, b1, r0, r1;
        if (pr.alongX) { a0 = V3(pr.mn.x, y0, pr.mn.z); a1 = V3(pr.mx.x, y0, pr.mn.z); b0 = V3(pr.mn.x, y0, pr.mx.z); b1 = V3(pr.mx.x, y0, pr.mx.z); r0 = V3(pr.mn.x, y1, c.z); r1 = V3(pr.mx.x, y1, c.z); }
        else { a0 = V3(pr.mn.x, y0, pr.mn.z); a1 = V3(pr.mn.x, y0, pr.mx.z); b0 = V3(pr.mx.x, y0, pr.mn.z); b1 = V3(pr.mx.x, y0, pr.mx.z); r0 = V3(c.x, y1, pr.mn.z); r1 = V3(c.x, y1, pr.mx.z); }
        V3 sl[2][4] = {{a0, a1, r1, r0}, {b0, b1, r1, r0}};
        for (int s = 0; s < 2; s++) {
            V3* q = sl[s];
            V3 n = norm(cross(q[1] - q[0], q[3] - q[0])); if (n.y < 0) n = -n;
            float U[4], Vv[4];
            for (int k = 0; k < 4; k++) { U[k] = (pr.alongX ? q[k].x : q[k].z) - floorf((pr.alongX ? pr.mn.x : pr.mn.z) / 32) * 32; Vv[k] = len(q[k] - V3(q[k].x, y0, q[k].z)) + (k >= 2 ? fabsf(pr.alongX ? (c.z - pr.mn.z) : (c.x - pr.mn.x)) : 0.0f); }
            glQuadLit(V, q, n, glCol(pr.col, 1), U, Vv, TX_ROOF, -1000, 0);
        }
        V3 gab[2][3] = {{a0, b0, r0}, {a1, b1, r1}};
        for (int s = 0; s < 2; s++) {
            V3 n = pr.alongX ? V3(s ? 1.0f : -1.0f, 0, 0) : V3(0, 0, s ? 1.0f : -1.0f);
            uint32_t gc = glCol(pr.gable, 1);
            for (int k = 0; k < 3; k++) { V3 p = gab[s][k]; glPush(V, p, n, gc, pr.alongX ? p.z - floorf(p.z / 32) * 32 : p.x - floorf(p.x / 32) * 32, p.y - floorf(p.y / 32) * 32, TX_PLASTER, -1000, 0); }
        }
    }
    // трава: перекрещенные карточки с текстурой пучка (альфа-тест), отдельный буфер — без теней
    std::vector<GV> Gr;
    auto tuft = [&](V3 base, float h, uint32_t col, uint32_t seed, int cards) {
        uint32_t q = hash2((int)seed, 517);
        float a0 = (q & 1023) / 1023.0f * 3.1416f, w = h * (1.3f + ((q >> 10) & 15) / 15.0f * 0.7f);
        uint32_t cb = glCol(shadeC(col, 0.8f), 1), ct = glCol(col, 1);
        float v0 = (GRASS_ROW + GRASS_SZ - 0.5f) / ATL_H, v1 = (GRASS_ROW + 0.5f) / ATL_H;
        for (int c = 0; c < cards; c++) {
            float a = a0 + c * 3.1416f / cards;
            V3 s(cosf(a) * w * 0.5f, 0, sinf(a) * w * 0.5f);
            bool flip = ((q >> (20 + c)) & 1) != 0;
            float u0 = flip ? 1.0f : 0.0f, u1 = flip ? 0.0f : 1.0f;
            V3 n = norm(V3(-s.z * 0.3f, 1.0f, s.x * 0.3f));
            V3 b0 = base - s, b1 = base + s, t0 = base - s * 1.1f + V3(0, h, 0), t1 = base + s * 1.1f + V3(0, h, 0);
            glPush(Gr, b0, n, cb, u0, v0, 21, 0, 5); glPush(Gr, b1, n, cb, u1, v0, 21, 0, 5); glPush(Gr, t1, n, ct, u1, v1, 21, 1, 5);
            glPush(Gr, b0, n, cb, u0, v0, 21, 0, 5); glPush(Gr, t1, n, ct, u1, v1, 21, 1, 5); glPush(Gr, t0, n, ct, u0, v1, 21, 1, 5);
        }
    };
    for (auto& g : G.grass)                 // высокая трава (укрытие)
        for (size_t k = 0; k < g.tufts.size(); k++) tuft(g.tufts[k], 0.75f + (k % 5) * 0.12f, shadeC(g.col, 0.9f + (k % 7) * 0.05f), (uint32_t)k * 7 + 1, 3);
    // земля: плитки 16 м по уровню, дальше — крупные
    float gy = G.groundY;
    if (G.hasGround || G.sea) {
        float x0 = floorf((G.bmin.x - 48) / 16) * 16, x1 = ceilf((G.bmax.x + 48) / 16) * 16;
        float z0 = floorf((G.bmin.z - 48) / 16) * 16, z1 = ceilf((G.bmax.z + 48) / 16) * 16;
        float fl = G.sea ? 4.0f : 0.0f, tx = G.sea ? 0.0f : (float)TX_GROUND;
        for (float z = z0; z < z1; z += 16)
            for (float x = x0; x < x1; x += 16) {
                V3 q[4] = {V3(x, gy, z), V3(x + 16, gy, z), V3(x + 16, gy, z + 16), V3(x, gy, z + 16)};
                float bx0 = floorf(x / 256) * 256, bz0 = floorf(z / 256) * 256;
                float U[4] = {x - bx0, x + 16 - bx0, x + 16 - bx0, x - bx0}, Vv[4] = {z - bz0, z - bz0, z + 16 - bz0, z + 16 - bz0};
                uint32_t cc[4];
                for (int k = 0; k < 4; k++) { uint32_t h = hash2((int)floorf(q[k].x / 16), (int)floorf(q[k].z / 16)); cc[k] = glCol(mixC(G.gcol1, G.gcol2, (h & 255) / 255.0f), 1); }
                static const int idx[6] = {0, 1, 2, 0, 2, 3};
                for (int k = 0; k < 6; k++) { int i = idx[k]; glPush(Gd, q[i], V3(0, 1, 0), cc[i], U[i], Vv[i], tx, -1000, fl); }
            }
        float R = 4000;
        uint32_t c = glCol(mixC(G.gcol1, G.gcol2, 0.5f), 1);
        V3 outer[4][4] = {{V3(-R, gy, -R), V3(R, gy, -R), V3(R, gy, z0), V3(-R, gy, z0)}, {V3(-R, gy, z1), V3(R, gy, z1), V3(R, gy, R), V3(-R, gy, R)},
                          {V3(-R, gy, z0), V3(x0, gy, z0), V3(x0, gy, z1), V3(-R, gy, z1)}, {V3(x1, gy, z0), V3(R, gy, z0), V3(R, gy, z1), V3(x1, gy, z1)}};
        for (auto& q : outer) glQuadLit(Gd, q, V3(0, 1, 0), c, nullptr, nullptr, 0, -1000, fl);
    }
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboStatic);
    GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(V.size() * sizeof(GV)), V.empty() ? nullptr : V.data(), gk::STATIC_DRAW);
    g_nStatic = (int)V.size();
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboGround);
    GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(Gd.size() * sizeof(GV)), Gd.empty() ? nullptr : Gd.data(), gk::STATIC_DRAW);
    g_nGround = (int)Gd.size();
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboGrass);
    GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(Gr.size() * sizeof(GV)), Gr.empty() ? nullptr : Gr.data(), gk::STATIC_DRAW);
    g_nGrass = (int)Gr.size();
    g_builtSerial = g_levelSerial;
}

// ------------------------------------------------------------ проходы
static void glBindGV(bool full) {
    const int st = (int)sizeof(GV);
    GLF.EnableVertexAttribArray(0); GLF.VertexAttribPointer(0, 3, gk::FLOAT, 0, st, (const void*)0);
    if (full) {
        GLF.EnableVertexAttribArray(1); GLF.VertexAttribPointer(1, 3, gk::FLOAT, 0, st, (const void*)12);
        GLF.EnableVertexAttribArray(2); GLF.VertexAttribPointer(2, 4, gk::UNSIGNED_BYTE, 1, st, (const void*)24);
        GLF.EnableVertexAttribArray(3); GLF.VertexAttribPointer(3, 2, gk::FLOAT, 0, st, (const void*)28);
    } else { GLF.DisableVertexAttribArray(1); GLF.DisableVertexAttribArray(2); GLF.DisableVertexAttribArray(3); }
    GLF.EnableVertexAttribArray(4); GLF.VertexAttribPointer(4, 3, gk::FLOAT, 0, st, (const void*)36);
}
static void glDrawVBO(unsigned vbo, int n, bool full) {
    if (n <= 0) return;
    GLF.BindBuffer(gk::ARRAY_BUFFER, vbo);
    glBindGV(full);
    GLF.DrawArrays(gk::TRIANGLES, 0, n);
}
static void glDrawDyn(const std::vector<GV>& B, bool full) {
    if (B.empty()) return;
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboDyn);
    GLF.BufferData(gk::ARRAY_BUFFER, (ptrdiff_t)(B.size() * sizeof(GV)), B.data(), gk::STREAM_DRAW);
    glBindGV(full);
    GLF.DrawArrays(gk::TRIANGLES, 0, (int)B.size());
}
static void glCamMatrix(float* m, float sx, float sy, float n, float f) {
    V3 p = g_cam.pos, R = g_cam.R, U = g_cam.U, F = g_cam.F;
    float a = (f + n) / (f - n), b = -2 * f * n / (f - n);
    float rows[4][4] = {{sx * R.x, sx * R.y, sx * R.z, -sx * dot(R, p)},
                        {sy * U.x, sy * U.y, sy * U.z, -sy * dot(U, p)},
                        {a * F.x, a * F.y, a * F.z, -a * dot(F, p) + b},
                        {F.x, F.y, F.z, -dot(F, p)}};
    for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) m[c * 4 + r] = rows[r][c];
}
static void glShadowMatrix(V3 center, float half, float* m) {
    V3 Fl = -g_env.sun;
    V3 up = fabsf(Fl.y) > 0.95f ? V3(1, 0, 0) : V3(0, 1, 0);
    V3 Rl = norm(cross(up, Fl)), Ul = cross(Fl, Rl);
    float texel = 2 * half / std::max(1, g_sfboSize);
    float cr = floorf(dot(Rl, center) / texel) * texel, cu = floorf(dot(Ul, center) / texel) * texel, cf = dot(Fl, center);
    float depth = 140;
    float rows[4][4] = {{Rl.x / half, Rl.y / half, Rl.z / half, -cr / half},
                        {Ul.x / half, Ul.y / half, Ul.z / half, -cu / half},
                        {Fl.x / depth, Fl.y / depth, Fl.z / depth, -cf / depth},
                        {0, 0, 0, 1}};
    for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) m[c * 4 + r] = rows[r][c];
}
static inline void cv3(uint32_t c, float* o) { o[0] = ((c >> 16) & 255) / 255.0f; o[1] = ((c >> 8) & 255) / 255.0f; o[2] = (c & 255) / 255.0f; }
static void glSetMain(const float* vp) {
    GLProg& P = g_pMain;
    GLF.UseProgram(P.id);
    GLF.UniformMatrix4fv(P.uVP, 1, 0, vp);
    GLF.UniformMatrix4fv(P.uSVP, 1, 0, g_shadowVP);
    GLF.Uniform3f(P.uSun, g_env.sun.x, g_env.sun.y, g_env.sun.z);
    float sc = g_env.sunI * 1.35f;
    float sky[3], fog[3];
    cv3(g_env.skyTop, sky); cv3(g_env.fog, fog);
    GLF.Uniform3f(P.uSunCol, sc * 1.05f, sc * 1.0f, sc * 0.92f);
    float am = g_env.amb * 0.78f;
    GLF.Uniform3f(P.uSkyAmb, am * (0.8f + sky[0] * 0.4f), am * (0.85f + sky[1] * 0.3f), am * (0.9f + sky[2] * 0.3f));
    GLF.Uniform3f(P.uGndAmb, am * 0.78f, am * 0.74f, am * 0.7f);
    GLF.Uniform3f(P.uFog, fog[0], fog[1], fog[2]);
    GLF.Uniform2f(P.uFogR, g_env.fogS, g_env.fogE);
    GLF.Uniform3f(P.uCam, g_cam.pos.x, g_cam.pos.y, g_cam.pos.z);
    GLF.Uniform1f(P.uTime, G.time);
    if (P.uWind >= 0) GLF.Uniform4f(P.uWind, fmodf(G.time, 1000.0f), G.groundY, g_cam.pos.x, g_cam.pos.z);
    GLF.Uniform1f(P.uShadowOn, g_glShadows ? 1.0f : 0.0f);
    GLF.Uniform1f(P.uShadowTexel, 0.7f / std::max(1, g_sfboSize));
    GLF.ActiveTexture(gk::TEXTURE0); GLF.BindTexture(gk::TEXTURE_2D, g_texNoise); GLF.Uniform1i(P.uNoise, 0);
    GLF.ActiveTexture(gk::TEXTURE0 + 1); GLF.BindTexture(gk::TEXTURE_2D, g_sfboTex); GLF.Uniform1i(P.uShadow, 1);
    if (P.uAtlas >= 0) { GLF.ActiveTexture(gk::TEXTURE0 + 2); GLF.BindTexture(gk::TEXTURE_2D, g_texAtlas); GLF.Uniform1i(P.uAtlas, 2); }
    GLF.ActiveTexture(gk::TEXTURE0);
}
// травяной ковёр: плитки вокруг камеры, маска уровня решает, где растёт и какого цвета
static void glDrawCarpet() {
    if (g_lawnDen <= 0 || g_gmW <= 0 || g_gmask.empty() || !g_nTileD || !G.hasGround) return;
    GLProg& P = g_pMain;
    GLF.ActiveTexture(gk::TEXTURE0 + 3); GLF.BindTexture(gk::TEXTURE_2D, g_texGMask);
    if (g_gmUploaded != g_gmSerial) {
        GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, g_gmW, g_gmH, 0, gk::RGBA, gk::UNSIGNED_BYTE, g_gmask.data());
        glTexParams(true, false);
        g_gmUploaded = g_gmSerial;
    }
    GLF.Uniform1i(P.uGMask, 3);
    GLF.Uniform4f(P.uGRect, g_gmX0, g_gmZ0, 1.0f / g_gmW, 1.0f / g_gmH);
    auto cl = [](uint32_t c, float* o) { o[0] = ((c >> 16) & 255) / 255.0f; o[1] = ((c >> 8) & 255) / 255.0f; o[2] = (c & 255) / 255.0f; };
    float ga[3], gb[3]; cl(g_lawnA, ga); cl(g_lawnB, gb);
    GLF.Uniform3f(P.uGrassA, ga[0], ga[1], ga[2]); GLF.Uniform3f(P.uGrassB, gb[0], gb[1], gb[2]);
    GLF.ActiveTexture(gk::TEXTURE0);
    int q = clampi(S.quality, 0, 2), R = q == 2 ? 2 : 1;
    const float T = 16.0f;
    int cx = (int)floorf(g_cam.pos.x / T), cz = (int)floorf(g_cam.pos.z / T);
    for (int dz = -R; dz <= R; dz++)
        for (int dx = -R; dx <= R; dx++) {
            float ox = (cx + dx) * T, oz = (cz + dz) * T;
            V3 c(ox + T * 0.5f, G.groundY, oz + T * 0.5f);
            if ((dx || dz) && dot(c - g_cam.pos, g_cam.F) < -T * 0.75f) continue;
            bool inner = std::max(abs(dx), abs(dz)) <= 1;
            GLF.Uniform4f(P.uTileOff, ox, oz, G.groundY, (hash2(cx + dx, cz + dz) & 1) ? 1.0f : -1.0f);
            if (inner && q >= 1) glDrawVBO(g_vboTileD, g_nTileD, true);
            glDrawVBO(g_vboTileS, g_nTileS, true);
        }
}
static float g_glScale = 1.0f;
static void glScene() {
    g_bOpq.clear(); g_bShd.clear(); g_bBld.clear(); g_bAdd.clear(); g_bVmO.clear(); g_bVmA.clear();
    if (g_builtSerial != g_levelSerial) glBuildStatic();
    drawDynamic();
    int q = clampi(S.quality, 0, 2);
    g_glShadows = q >= 1;
    g_glScale = q == 0 ? 0.5f : (q == 1 ? 0.75f : 1.0f);
    int w = std::max(64, (int)(g_dst.w * g_glScale)), h = std::max(36, (int)(g_dst.h * g_glScale));
    if (w != g_fboW || h != g_fboH || !g_fbo) { glEnsureFbo(g_fbo, g_fboTex, g_fboDepth, w, h, true); g_fboW = w; g_fboH = h; }
    GLF.Disable(gk::CULL_FACE);
    GLF.Disable(gk::BLEND);
    GLF.Enable(gk::DEPTH_TEST); GLF.DepthFunc(gk::LEQUAL); GLF.DepthMask(1);
    // тени
    if (g_glShadows) {
        int ss = q == 2 ? 2048 : 1024;
        if (ss != g_sfboSize || !g_sfbo) { glEnsureFbo(g_sfbo, g_sfboTex, g_sfboDepth, ss, ss, false); g_sfboSize = ss; }
        V3 center = g_cam.pos + V3(g_cam.F.x, 0, g_cam.F.z) * 18.0f;
        glShadowMatrix(center, q == 2 ? 42.0f : 36.0f, g_shadowVP);
        GLF.BindFramebuffer(gk::FRAMEBUFFER, g_sfbo);
        GLF.Viewport(0, 0, ss, ss);
        GLF.ClearColor(1, 1, 1, 1); GLF.Clear(gk::COLOR_BUFFER_BIT | gk::DEPTH_BUFFER_BIT);
        GLF.UseProgram(g_pDepth.id);
        GLF.UniformMatrix4fv(g_pDepth.uSVP, 1, 0, g_shadowVP);
        glDrawVBO(g_vboStatic, g_nStatic, false);
        glDrawDyn(g_bOpq, false);
    }
    GLF.BindFramebuffer(gk::FRAMEBUFFER, g_fbo);
    GLF.Viewport(0, 0, w, h);
    GLF.ClearColor(0, 0, 0, 1); GLF.Clear(gk::COLOR_BUFFER_BIT | gk::DEPTH_BUFFER_BIT);
    // небо
    {
        float sx = (RW * 0.5f) / g_cam.focal, sy = (RH * 0.5f) / g_cam.focal;
        float verts[4][5];
        float xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int i = 0; i < 4; i++) {
            V3 d = g_cam.F + g_cam.R * (xy[i][0] * sx) + g_cam.U * (xy[i][1] * sy);
            verts[i][0] = xy[i][0]; verts[i][1] = xy[i][1]; verts[i][2] = d.x; verts[i][3] = d.y; verts[i][4] = d.z;
        }
        float tri[6][5];
        int id[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; k++) memcpy(tri[k], verts[id[k]], sizeof(tri[k]));
        GLF.DepthMask(0); GLF.Disable(gk::DEPTH_TEST);
        GLF.UseProgram(g_pSky.id);
        float t3[3];
        cv3(g_env.skyTop, t3); GLF.Uniform3f(g_pSky.uTop, t3[0], t3[1], t3[2]);
        cv3(g_env.skyHor, t3); GLF.Uniform3f(g_pSky.uHor, t3[0], t3[1], t3[2]);
        cv3(g_env.fog, t3); GLF.Uniform3f(g_pSky.uFog, t3[0], t3[1], t3[2]);
        GLF.Uniform3f(g_pSky.uSun, g_env.sun.x, g_env.sun.y, g_env.sun.z);
        {   // низкое солнце — теплее и краснее
            float k = clampf(g_env.sun.y / 0.5f, 0, 1); k = k * k * (3 - 2 * k);
            GLF.Uniform3f(g_pSky.uSunCol, g_env.sunI, g_env.sunI * lerpf(0.62f, 0.92f, k), g_env.sunI * lerpf(0.36f, 0.8f, k));
        }
        GLF.Uniform1f(g_pSky.uFlash, g_env.flash * 0.6f);
        GLF.Uniform1f(g_pSky.uTime, fmodf(G.time, 3000.0f));
        GLF.Uniform1f(g_pSky.uCloud, g_env.cloud);
        GLF.ActiveTexture(gk::TEXTURE0); GLF.BindTexture(gk::TEXTURE_2D, g_texNoise); GLF.Uniform1i(g_pSky.uNoise, 0);
        GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboQuad);
        GLF.BufferData(gk::ARRAY_BUFFER, sizeof(tri), tri, gk::STREAM_DRAW);
        for (unsigned a = 2; a < 5; a++) GLF.DisableVertexAttribArray(a);
        GLF.EnableVertexAttribArray(0); GLF.VertexAttribPointer(0, 2, gk::FLOAT, 0, 20, (const void*)0);
        GLF.EnableVertexAttribArray(1); GLF.VertexAttribPointer(1, 3, gk::FLOAT, 0, 20, (const void*)8);
        GLF.DrawArrays(gk::TRIANGLES, 0, 6);
        GLF.Enable(gk::DEPTH_TEST); GLF.DepthMask(1);
    }
    float vp[16];
    glCamMatrix(vp, g_cam.focal / (RW * 0.5f), g_cam.focal / (RH * 0.5f), 0.1f, std::max(200.0f, g_cam.farZ * 1.3f));
    glSetMain(vp);
    GLF.Enable(gk::POLYGON_OFFSET_FILL); GLF.PolygonOffset(1.0f, 2.0f);
    glDrawVBO(g_vboGround, g_nGround, true);
    GLF.Disable(gk::POLYGON_OFFSET_FILL);
    glDrawVBO(g_vboStatic, g_nStatic, true);
    glDrawVBO(g_vboGrass, g_nGrass, true);
    glDrawCarpet();
    glDrawDyn(g_bOpq, true);
    GLF.Enable(gk::BLEND); GLF.DepthMask(0);
    if (!g_bShd.empty()) {
        for (auto& v : g_bShd) { v.c = 0xFF808080u; v.fl = 2; }
        GLF.BlendFunc(gk::DST_COLOR, gk::ZERO);
        GLF.Enable(gk::POLYGON_OFFSET_FILL); GLF.PolygonOffset(-1.0f, -2.0f);
        glDrawDyn(g_bShd, true);
        GLF.Disable(gk::POLYGON_OFFSET_FILL);
    }
    GLF.BlendFunc(gk::SRC_ALPHA, gk::ONE_MINUS_SRC_ALPHA);
    glDrawDyn(g_bBld, true);
    GLF.BlendFunc(gk::SRC_ALPHA, gk::ONE);
    glDrawDyn(g_bAdd, true);
    GLF.DepthMask(1); GLF.Disable(gk::BLEND);
}
// оружие в руках: свой проход с ближней плоскостью отсечения
static void glFlushVM() {
    if (!g_useGL) return;
    GLF.BindFramebuffer(gk::FRAMEBUFFER, g_fbo);
    GLF.Viewport(0, 0, g_fboW, g_fboH);
    if (g_bVmO.empty() && g_bVmA.empty()) return;
    GLF.Clear(gk::DEPTH_BUFFER_BIT);
    float vp[16];
    glCamMatrix(vp, g_vmFocal / (RW * 0.5f), g_vmFocal / (RH * 0.5f), 0.015f, 20.0f);
    glSetMain(vp);
    glDrawDyn(g_bVmO, true);
    GLF.Enable(gk::BLEND); GLF.DepthMask(0); GLF.BlendFunc(gk::SRC_ALPHA, gk::ONE);
    glDrawDyn(g_bVmA, true);
    GLF.DepthMask(1); GLF.Disable(gk::BLEND);
    g_bVmO.clear(); g_bVmA.clear();
}
static void glQuadDraw(unsigned prog, float u1, float v1) {
    float q[6][4] = {{-1, -1, 0, 0}, {1, -1, u1, 0}, {1, 1, u1, v1}, {-1, -1, 0, 0}, {1, 1, u1, v1}, {-1, 1, 0, v1}};
    (void)prog;
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboQuad);
    GLF.BufferData(gk::ARRAY_BUFFER, sizeof(q), q, gk::STREAM_DRAW);
    for (unsigned a = 2; a < 5; a++) GLF.DisableVertexAttribArray(a);
    GLF.EnableVertexAttribArray(0); GLF.VertexAttribPointer(0, 2, gk::FLOAT, 0, 16, (const void*)0);
    GLF.EnableVertexAttribArray(1); GLF.VertexAttribPointer(1, 2, gk::FLOAT, 0, 16, (const void*)8);
    GLF.DrawArrays(gk::TRIANGLES, 0, 6);
}
// вывод на экран: картинка сцены с постэффектами + интерфейс
static void glPresent(bool scene, int winW, int winH) {
    GLF.BindFramebuffer(gk::FRAMEBUFFER, (unsigned)g_defFbo);
    GLF.Viewport(0, 0, winW, winH);
    GLF.ClearColor(0, 0, 0, 1);
    GLF.Clear(gk::COLOR_BUFFER_BIT | gk::DEPTH_BUFFER_BIT);
    GLF.Disable(gk::DEPTH_TEST); GLF.DepthMask(0);
    GLF.Viewport(g_dst.x, winH - g_dst.y - g_dst.h, g_dst.w, g_dst.h);
    if (scene && g_fbo) {
        GLF.UseProgram(g_pPost.id);
        GLF.ActiveTexture(gk::TEXTURE0); GLF.BindTexture(gk::TEXTURE_2D, g_fboTex); GLF.Uniform1i(g_pPost.uTex, 0);
        GLF.Uniform4f(g_pPost.uFx, g_post[0], g_post[1], g_post[2], g_post[3]);
        GLF.Uniform4f(g_pPost.uTint, g_post[4], g_post[5], g_post[6], g_post[7]);
        GLF.Uniform1f(g_pPost.uAspect, (float)UIW / UIH);
        GLF.Disable(gk::BLEND);
        glQuadDraw(g_pPost.id, 1, 1);
    }
    GLF.UseProgram(g_pUI.id);
    GLF.ActiveTexture(gk::TEXTURE0); GLF.BindTexture(gk::TEXTURE_2D, g_glTexUI);
    static int uiTexW = 640;
    if (uiTexW != UIW) {   // ширина интерфейса поменялась (поворот, другой экран)
        GLF.TexImage2D(gk::TEXTURE_2D, 0, (int)gk::RGBA, UIW, UIH, 0, gk::RGBA, gk::UNSIGNED_BYTE, nullptr);
        uiTexW = UIW;
    }
    GLF.TexSubImage2D(gk::TEXTURE_2D, 0, 0, 0, UIW, UIH, gk::RGBA, gk::UNSIGNED_BYTE, g_ui);
    GLF.Uniform1i(g_pUI.uTex, 0);
    GLF.Enable(gk::BLEND); GLF.BlendFunc(gk::SRC_ALPHA, gk::ONE_MINUS_SRC_ALPHA);
    // UI хранится сверху вниз — переворачиваем по v
    float q[6][4] = {{-1, -1, 0, 1}, {1, -1, 1, 1}, {1, 1, 1, 0}, {-1, -1, 0, 1}, {1, 1, 1, 0}, {-1, 1, 0, 0}};
    GLF.BindBuffer(gk::ARRAY_BUFFER, g_vboQuad);
    GLF.BufferData(gk::ARRAY_BUFFER, sizeof(q), q, gk::STREAM_DRAW);
    for (unsigned a = 2; a < 5; a++) GLF.DisableVertexAttribArray(a);
    GLF.EnableVertexAttribArray(0); GLF.VertexAttribPointer(0, 2, gk::FLOAT, 0, 16, (const void*)0);
    GLF.EnableVertexAttribArray(1); GLF.VertexAttribPointer(1, 2, gk::FLOAT, 0, 16, (const void*)8);
    GLF.DrawArrays(gk::TRIANGLES, 0, 6);
    GLF.Disable(gk::BLEND); GLF.DepthMask(1);
}

// =====================================================================
//  ПОСТРОЕНИЕ УРОВНЕЙ: вспомогательные функции
// =====================================================================
static void saveAll();
static int g_ch = 0;  // текущая глава кампании
static void setEnv(V3 sun, float amb, float sunI, uint32_t fog, float fs, float fe, uint32_t top, uint32_t hor, float farZ) {
    g_env.sun = norm(sun); g_env.amb = amb; g_baseAmb = amb; g_env.sunI = sunI; g_env.fog = fog; g_env.fogS = fs; g_env.fogE = fe;
    g_env.skyTop = top; g_env.skyHor = hor; g_env.flash = 0; g_env.cloud = 0.45f; g_env.vmLight = clampf(amb + sunI * 0.6f, 0.35f, 1.1f);
    G.farZ = farZ;
}
static void setGround(uint32_t c1, uint32_t c2, float y) { G.hasGround = true; G.groundY = y; G.gcol1 = c1; G.gcol2 = c2; }
static void bounds(float x0, float z0, float x1, float z1) {
    addBox(x0 - 1, -20, z0, x0, 40, z1, 0, MT_INVIS); addBox(x1, -20, z0, x1 + 1, 40, z1, 0, MT_INVIS);
    addBox(x0, -20, z0 - 1, x1, 40, z0, 0, MT_INVIS); addBox(x0, -20, z1, x1, 40, z1 + 1, 0, MT_INVIS);
    G.bmin = V3(x0, -20, z0); G.bmax = V3(x1, 40, z1);
}
struct Gap { float a, b; bool win; };
static void wallGeneric(bool alongX, float fixed, float s0, float s1, float y0, float h, float th, uint32_t col, int mat,
                        std::initializer_list<float> doors, std::initializer_list<float> wins) {
    std::vector<Gap> gaps;
    for (float d : doors) { Gap g; g.a = d - 0.75f; g.b = d + 0.75f; g.win = false; gaps.push_back(g); }
    for (float w : wins) { Gap g; g.a = w - 0.8f; g.b = w + 0.8f; g.win = true; gaps.push_back(g); }
    std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) { return a.a < b.a; });
    auto seg = [&](float a, float b, float ya, float yb) {
        if (b - a < 0.01f || yb - ya < 0.01f) return;
        if (alongX) addBox(a, ya, fixed - th * 0.5f, b, yb, fixed + th * 0.5f, col, mat);
        else addBox(fixed - th * 0.5f, ya, a, fixed + th * 0.5f, yb, b, col, mat);
    };
    float cur = s0;
    for (auto& g : gaps) {
        if (g.a > cur) seg(cur, g.a, y0, y0 + h);
        if (g.win) { seg(g.a, g.b, y0, y0 + 1.0f); seg(g.a, g.b, y0 + 2.1f, y0 + h); }
        else seg(g.a, g.b, y0 + 2.3f, y0 + h);
        cur = std::max(cur, g.b);
    }
    if (s1 > cur) seg(cur, s1, y0, y0 + h);
}
static void wallX(float z, float x0, float x1, float h, float th, uint32_t col, std::initializer_list<float> doors = {}, std::initializer_list<float> wins = {}, int mat = MT_CONCRETE, float y0 = 0) {
    wallGeneric(true, z, x0, x1, y0, h, th, col, mat, doors, wins);
}
static void wallZ(float x, float z0, float z1, float h, float th, uint32_t col, std::initializer_list<float> doors = {}, std::initializer_list<float> wins = {}, int mat = MT_CONCRETE, float y0 = 0) {
    wallGeneric(false, x, z0, z1, y0, h, th, col, mat, doors, wins);
}
static void sandbags(float x0, float z0, float x1, float z1, float h = 0.95f) { int i = addBox(x0, 0, z0, x1, h, z1, RGBc(150, 135, 100), MT_CONCRETE, true); G.box[i].tex = TX_SANDBAG; }
static void crate(float x, float z, float s, float h, float y = 0) { addBox(x - s, y, z - s, x + s, y + h, z + s, RGBc(120, 95, 60), MT_WOOD, true); }
static void barrel(float x, float z, uint32_t col) { addBox(x - 0.3f, 0, z - 0.3f, x + 0.3f, 0.95f, z + 0.3f, col, MT_METAL, true); }
static void carProp(float x, float z, bool alongZ, uint32_t col, float y = 0) {
    float hx = alongZ ? 0.9f : 2.1f, hz = alongZ ? 2.1f : 0.9f;
    addBox(x - hx, y + 0.3f, z - hz, x + hx, y + 1.0f, z + hz, col, MT_METAL, true);
    float cx = alongZ ? 0.8f : 1.1f, cz = alongZ ? 1.1f : 0.8f;
    addBox(x - cx, y + 1.0f, z - cz, x + cx, y + 1.55f, z + cz, shadeC(col, 0.8f), MT_METAL);
    addDeco(x - hx + 0.1f, y, z - hz + 0.1f, x + hx - 0.1f, y + 0.3f, z + hz - 0.1f, RGBc(25, 25, 25));
}
static void container(float x, float z, bool alongZ, uint32_t col, float y = 0) {
    float hx = alongZ ? 1.22f : 3.0f, hz = alongZ ? 3.0f : 1.22f;
    addBox(x - hx, y, z - hz, x + hx, y + 2.6f, z + hz, col, MT_METAL);
    for (int k = -2; k <= 2; k++) {
        if (alongZ) addDeco(x - hx - 0.03f, y + 0.1f, z + k * 1.1f - 0.05f, x + hx + 0.03f, y + 2.5f, z + k * 1.1f + 0.05f, shadeC(col, 0.75f));
        else addDeco(x + k * 1.1f - 0.05f, y + 0.1f, z - hz - 0.03f, x + k * 1.1f + 0.05f, y + 2.5f, z + hz + 0.03f, shadeC(col, 0.75f));
    }
}
static void surface(float x0, float z0, float x1, float z1, float h, uint32_t col, int tex) { addDeco(x0, 0, z0, x1, h, z1, col); G.box.back().tex = (uint8_t)tex; }
static void lamp(float x, float y, float z, uint32_t col) { addDeco(x - 0.15f, y - 0.08f, z - 0.15f, x + 0.15f, y, z + 0.15f, col, true); }
static void grassPatch(float x0, float z0, float x1, float z1, int count, uint32_t col) {
    Grass g; g.mn = V3(x0, 0, z0); g.mx = V3(x1, 1.2f, z1); g.col = col;
    for (int i = 0; i < count; i++) g.tufts.push_back(V3(frange(x0, x1), G.groundY, frange(z0, z1)));
    G.grass.push_back(g);
}
static void addRoof(float x0, float z0, float x1, float z1, float y, float rh, uint32_t col, uint32_t gable) {
    Prism p; p.mn = V3(x0, y, z0); p.mx = V3(x1, y + rh, z1); p.alongX = (x1 - x0) >= (z1 - z0); p.col = col; p.gable = gable;
    G.prisms.push_back(p);
}
// окно на фасаде: n — наружная нормаль (по оси X или Z)
static void addWindow(V3 c, V3 n, float w, float h, uint32_t frame, uint32_t shutter, bool door) {
    V3 t(fabsf(n.z), 0, fabsf(n.x));   // вдоль фасада
    auto box = [&](V3 cc, float hw, float hh, float depth, uint32_t col) {
        V3 a = cc - t * hw - V3(0, hh, 0), b = cc + t * hw + V3(0, hh, 0);
        V3 o = n * depth;
        addDeco(std::min(a.x, b.x + o.x) - (n.x != 0 ? 0 : 0), a.y, std::min(a.z, b.z + o.z), std::max(a.x, b.x + o.x), b.y, std::max(a.z, b.z + o.z), col);
    };
    box(c + n * 0.01f, w * 0.5f + 0.08f, h * 0.5f + 0.08f, 0.05f, frame);
    box(c + n * 0.02f, w * 0.5f, h * 0.5f, 0.05f, door ? RGBc(78, 56, 40) : RGBc(38, 46, 56));
    if (!door) {
        box(c + n * 0.03f, 0.03f, h * 0.5f, 0.05f, frame);                         // импост
        box(c + n * 0.02f - V3(0, h * 0.5f + 0.1f, 0), w * 0.5f + 0.15f, 0.04f, 0.16f, frame); // подоконник
        if (shutter) {
            box(c + n * 0.03f - t * (w * 0.5f + 0.32f), 0.28f, h * 0.5f, 0.04f, shutter);
            box(c + n * 0.03f + t * (w * 0.5f + 0.32f), 0.28f, h * 0.5f, 0.04f, shutter);
        }
    } else box(c + n * 0.02f + V3(0, h * 0.5f + 0.18f, 0), w * 0.5f + 0.2f, 0.07f, 0.12f, frame); // козырёк
}
// дом: штукатурка/кирпич, цоколь, межэтажные пояса, карниз, окна со ставнями, дверь, скатная крыша
static void house(float x0, float z0, float x1, float z1, float h, uint32_t col, bool roof = true) {
    uint32_t hh = hash2((int)(x0 * 7 + z1), (int)(z0 * 3 + x1));
    bool brick = (hh & 7) == 0;
    uint32_t wall = brick ? RGBc(150, 82, 62) : col;
    int bi = addBox(x0, 0, z0, x1, h, z1, wall);
    G.box[bi].tex = brick ? TX_BRICK : TX_PLASTER;
    uint32_t trim = mixC(wall, RGBc(235, 230, 220), 0.55f);
    g_defTex = TX_CONCRETE;
    addDeco(x0 - 0.06f, 0, z0 - 0.06f, x1 + 0.06f, 0.55f, z1 + 0.06f, shadeC(wall, 0.62f));         // цоколь
    g_defTex = -1;
    addDeco(x0 - 0.18f, h - 0.3f, z0 - 0.18f, x1 + 0.18f, h, z1 + 0.18f, trim);                     // карниз
    int floors = std::max(1, (int)((h - 0.6f) / 3.0f));
    for (int f = 1; f < floors; f++) addDeco(x0 - 0.07f, f * 3.0f - 0.08f, z0 - 0.07f, x1 + 0.07f, f * 3.0f + 0.08f, z1 + 0.07f, trim);
    static const uint32_t shut[4] = {0xFF3E6A4Au, 0xFF2F5A86u, 0xFF8A4A2Eu, 0};
    uint32_t sh = shut[(hh >> 4) & 3];
    float L[4] = {x1 - x0, x1 - x0, z1 - z0, z1 - z0};
    V3 N[4] = {V3(0, 0, -1), V3(0, 0, 1), V3(-1, 0, 0), V3(1, 0, 0)};
    for (int s = 0; s < 4; s++) {
        if (L[s] < 3.0f) continue;
        int n = std::min(8, std::max(1, (int)((L[s] - 1.0f) / 2.6f)));
        float step = L[s] / n;
        for (int f = 0; f < std::min(floors, 3); f++) {
            float y = f * 3.0f + 1.65f;
            if (y + 0.8f > h - 0.3f) break;
            for (int k = 0; k < n; k++) {
                float t = (k + 0.5f) * step;
                V3 c;
                if (s == 0) c = V3(x0 + t, y, z0); else if (s == 1) c = V3(x0 + t, y, z1);
                else if (s == 2) c = V3(x0, y, z0 + t); else c = V3(x1, y, z0 + t);
                bool door = (f == 0 && k == n / 2 && ((hh >> (8 + s)) & 1));
                if (door) { c.y = 1.1f; addWindow(c, N[s], 1.1f, 2.2f, trim, 0, true); }
                else addWindow(c, N[s], 0.9f, 1.3f, trim, sh, false);
            }
        }
    }
    if (roof) {
        float w = std::min(x1 - x0, z1 - z0);
        uint32_t rc = ((hh >> 12) & 1) ? RGBc(160, 72, 50) : RGBc(92, 88, 86);
        addRoof(x0 - 0.35f, z0 - 0.35f, x1 + 0.35f, z1 + 0.35f, h, std::min(3.5f, w * 0.38f), rc, wall);
        float cx = x0 + (x1 - x0) * 0.3f, cz = z0 + (z1 - z0) * 0.35f;
        g_defTex = TX_BRICK;
        addDeco(cx - 0.3f, h, cz - 0.3f, cx + 0.3f, h + std::min(3.5f, w * 0.38f) + 0.6f, cz + 0.3f, RGBc(130, 70, 55));  // труба
        g_defTex = -1;
    } else {
        addDeco(x0, h, z0, x1, h + 0.5f, z0 + 0.25f, trim); addDeco(x0, h, z1 - 0.25f, x1, h + 0.5f, z1, trim);   // парапет
        addDeco(x0, h, z0, x0 + 0.25f, h + 0.5f, z1, trim); addDeco(x1 - 0.25f, h, z0, x1, h + 0.5f, z1, trim);
    }
}

// ------------------------------------------------------------ миникарта: растр уровня (1 клетка = 1 м)
// 0 — земля, 1 — дорога/пол, 2 — низкое укрытие, 3 — здание/стена, 4 — вода
static std::vector<uint8_t> g_mm;
static int g_mmW = 0, g_mmH = 0; static float g_mmX0 = 0, g_mmZ0 = 0;
static void buildMinimap() {
    float x0 = G.bmin.x, z0 = G.bmin.z, x1 = G.bmax.x, z1 = G.bmax.z;
    if (!(x1 > x0 && z1 > z0) || (x1 - x0) * (z1 - z0) > 4.0e6f) { g_mmW = g_mmH = 0; g_mm.clear(); return; }
    g_mmX0 = x0; g_mmZ0 = z0; g_mmW = (int)ceilf(x1 - x0); g_mmH = (int)ceilf(z1 - z0);
    g_mm.assign((size_t)g_mmW * g_mmH, 0);
    float gy = G.hasGround ? G.groundY : 0.0f;
    for (const Box& b : G.box) {
        if (!b.vis || b.mat == MT_INVIS) continue;
        float h = b.mx.y - b.mn.y;
        if (b.mn.y > gy + 2.0f) continue;                    // балконы, перекрытия, кроны
        if (!b.solid && h > 0.12f) continue;                 // декор (горы, рамы окон)
        uint8_t t;
        if (h < 0.45f) {
            int r = (b.col >> 16) & 255, g = (b.col >> 8) & 255, bb = b.col & 255;
            t = (bb > r + 25 && bb > g) ? 4 : 1;
        } else t = (b.mx.y - gy < 1.7f) ? 2 : 3;
        int ix0 = std::max(0, (int)floorf(b.mn.x - x0)), ix1 = std::min(g_mmW - 1, (int)ceilf(b.mx.x - x0) - 1);
        int iz0 = std::max(0, (int)floorf(b.mn.z - z0)), iz1 = std::min(g_mmH - 1, (int)ceilf(b.mx.z - z0) - 1);
        for (int z = iz0; z <= iz1; z++)
            for (int x = ix0; x <= ix1; x++) { uint8_t& c = g_mm[(size_t)z * g_mmW + x]; if (t > c || (t == 4 && c <= 1)) c = t; }
    }
}
static inline int mmAt(float x, float z) {
    int ix = (int)floorf(x - g_mmX0), iz = (int)floorf(z - g_mmZ0);
    if (ix < 0 || iz < 0 || ix >= g_mmW || iz >= g_mmH) return -1;
    return g_mm[(size_t)iz * g_mmW + ix];
}
static int spawnTarget(V3 p, bool civ, float yaw) {
    Actor a; a.kind = civ ? AK_CIVTARGET : AK_TARGET; a.team = 1; a.pos = p; a.yaw = yaw; a.hp = 1; a.maxhp = 1; a.uni = U_CIV;
    G.act.push_back(a);
    return (int)G.act.size() - 1;
}
static int spawnVip(V3 p, int uni, float yaw) {
    Actor a; a.kind = AK_VIP; a.team = 1; a.pos = p; a.yaw = yaw; a.uni = uni; a.hp = 10000; a.maxhp = 10000;
    G.act.push_back(a);
    return (int)G.act.size() - 1;
}
static int spawnVeh(int kind, V3 p, float yaw, int team) {
    Vehicle v; v.kind = kind; v.pos = p; v.yaw = yaw; v.team = team;
    if (kind == VK_HELI) { v.hp = 1300; v.maxhp = 1300; v.speed = 14; }
    if (kind == VK_JET) { v.speed = 140; v.hp = 1e9f; }
    if (kind == VK_TECH) { v.hp = 350; v.maxhp = 350; }
    if (kind == VK_TEL) { v.hp = 1e9f; v.speed = 0; }
    if (kind == VK_TANK) { v.hp = 1400; v.maxhp = 1400; v.speed = 5; v.fireCD = 4; }
    G.veh.push_back(v);
    return (int)G.veh.size() - 1;
}
// ---- окружение: сетки гор и воронок, реквизит, дымы войны
static inline float hnoise(float x, float y, int seed) {   // гладкий шум на хэше решётки
    int ix = (int)floorf(x), iy = (int)floorf(y); float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    auto h = [&](int a, int b) { return (hash2(a * 7919 + seed, b * 104729 + seed * 13) & 1023) / 1023.0f; };
    float a = h(ix, iy) + (h(ix + 1, iy) - h(ix, iy)) * fx, b = h(ix, iy + 1) + (h(ix + 1, iy + 1) - h(ix, iy + 1)) * fx;
    return a + (b - a) * fy;
}
static void meshTri(V3 a, V3 b, V3 c, uint32_t col, bool up = true) {
    V3 n = cross(b - a, c - a);
    if (up && n.y < 0) std::swap(b, c);
    MTri t; t.a = a; t.b = b; t.c = c; t.col = col; G.mtri.push_back(t);
}
static void meshGroupEnd(int t0, V3 c, float r) { MGroup g; g.c = c; g.r = r; g.t0 = t0; g.t1 = (int)G.mtri.size(); G.mgrp.push_back(g); }
// гора: неровный конус из треугольников с осыпями, скальными гранями и снежной шапкой
static void mountain(float x, float z, float w, float h, uint32_t col) {
    const int R = 8, S = 22;
    int seed = (int)(x * 13 + z * 7);
    V3 P[R + 1][S];
    for (int i = 0; i <= R; i++) {
        float r = 1.0f - i / (float)R;            // 1 — подножие, 0 — вершина
        for (int j = 0; j < S; j++) {
            float a = j * 2 * PI_F / S + hnoise(i * 0.7f, j * 0.5f, seed) * 0.25f;
            float rr = r * (0.82f + 0.36f * hnoise(j * 0.45f, 3.1f, seed + 5));
            float prof = powf(std::max(0.0f, 1 - r), 1.35f);
            float bump = 0.78f + 0.44f * hnoise(j * 0.6f, i * 0.8f, seed + 11);
            float y = i == 0 ? -0.6f : h * prof * bump;
            if (i == R) y = h * (0.92f + 0.12f * hnoise(j * 0.3f, 9.0f, seed));
            P[i][j] = V3(x + cosf(a) * rr * w, y, z + sinf(a) * rr * w * 0.8f);
        }
    }
    uint32_t grassC = mixC(G.gcol1, RGBc(70, 78, 50), 0.4f), rockC = col, darkC = shadeC(col, 0.78f), snowC = RGBc(232, 234, 240);
    bool snowy = G.snow || h > 55;
    int t0 = (int)G.mtri.size();
    auto colAt = [&](V3 a, V3 b, V3 c, int i, int j) {
        V3 n = norm(cross(b - a, c - a)); if (n.y < 0) n = -n;
        float hy = (a.y + b.y + c.y) / (3 * std::max(1.0f, h));
        float v = hnoise(i * 1.3f, j * 1.7f, seed + 3);
        uint32_t base = mixC(darkC, rockC, v);
        if (hy < 0.18f && n.y > 0.55f) base = mixC(grassC, base, hy / 0.18f * 0.6f);
        if (snowy && hy > 0.62f + v * 0.12f && n.y > 0.45f) base = mixC(base, snowC, clampf((hy - 0.62f) * 4.0f + n.y - 0.45f, 0, 1));
        return base;
    };
    for (int i = 0; i < R; i++)
        for (int j = 0; j < S; j++) {
            int k = (j + 1) % S;
            V3 a = P[i][j], b = P[i][k], c = P[i + 1][k], d = P[i + 1][j];
            meshTri(a, b, c, colAt(a, b, c, i, j)); meshTri(a, c, d, colAt(a, c, d, i, j + 100));
        }
    V3 top(x, h * 1.02f, z);
    for (int j = 0; j < S; j++) meshTri(P[R][j], P[R][(j + 1) % S], top, colAt(P[R][j], P[R][(j + 1) % S], top, R, j));
    meshGroupEnd(t0, V3(x, h * 0.4f, z), w * 1.25f + h * 0.5f);
}
// воронка: вал из выброшенной земли, обожжённое дно
static void crater(float x, float z, float r) {
    const int S = 14; int seed = (int)(x * 31 + z * 17);
    uint32_t earth = mixC(G.gcol1, RGBc(92, 76, 58), 0.65f), burnt = RGBc(38, 34, 30), rimC = mixC(earth, RGBc(110, 96, 74), 0.4f);
    int t0 = (int)G.mtri.size();
    V3 ring[4][S];
    float rad[4] = {0.45f, 0.95f, 1.2f, 1.75f}, hy[4] = {0.03f, 0.2f, 0.26f, 0.015f};
    for (int q = 0; q < 4; q++) for (int j = 0; j < S; j++) {
        float a = j * 2 * PI_F / S, rr = r * rad[q] * (0.85f + 0.3f * hnoise(j * 0.8f, q * 1.3f, seed));
        ring[q][j] = V3(x + cosf(a) * rr, G.groundY + hy[q] * r * (0.8f + 0.4f * hnoise(j * 1.1f, q + 5.0f, seed)), z + sinf(a) * rr);
    }
    V3 c(x, G.groundY + 0.02f, z);
    for (int j = 0; j < S; j++) {
        int k = (j + 1) % S;
        meshTri(c, ring[0][j], ring[0][k], burnt);
        uint32_t cols[3] = {mixC(burnt, earth, 0.4f), rimC, mixC(rimC, G.gcol1, 0.5f)};
        for (int q = 0; q < 3; q++) { meshTri(ring[q][j], ring[q][k], ring[q + 1][k], cols[q]); meshTri(ring[q][j], ring[q + 1][k], ring[q + 1][j], cols[q]); }
    }
    meshGroupEnd(t0, c, r * 2);
}
// пятно гари
static void scorch(float x, float z, float r) {
    const int S = 10; int seed = (int)(x * 3 + z * 5); int t0 = (int)G.mtri.size();
    V3 c(x, G.groundY + 0.045f, z);
    for (int j = 0; j < S; j++) {
        float a0 = j * 2 * PI_F / S, a1 = (j + 1) * 2 * PI_F / S;
        float r0 = r * (0.6f + 0.6f * hnoise(j * 0.9f, 0, seed)), r1 = r * (0.6f + 0.6f * hnoise((j + 1) % S * 0.9f, 0, seed));
        meshTri(c, c + V3(cosf(a0) * r0, 0, sinf(a0) * r0), c + V3(cosf(a1) * r1, 0, sinf(a1) * r1), mixC(RGBc(30, 28, 26), G.gcol1, 0.25f));
    }
    meshGroupEnd(t0, c, r * 1.3f);
}
static void prop(V3 c, Basis B, V3 h, uint32_t col) { Prop p; p.c = c; p.B = B; p.h = h; p.col = col; G.props.push_back(p); }
// противотанковый ёж из трёх балок
static void hedgehog(float x, float z, float yaw) {
    uint32_t steel = RGBc(70, 58, 48);
    V3 c(x, G.groundY + 0.62f, z);
    for (int k = 0; k < 3; k++) {
        Basis B = basisYPR(yaw + k * 2.094f, 0.62f, 0);
        prop(c, B, V3(0.07f, 0.07f, 0.95f), shadeC(steel, 0.9f + k * 0.06f));
    }
    int b = addBox(x - 0.75f, G.groundY, z - 0.75f, x + 0.75f, G.groundY + 1.1f, z + 0.75f, 0, MT_METAL); G.box[b].vis = false;
}
// заграждение из колючей проволоки между точками a и b
static void wireFence(V3 a, V3 b) {
    V3 d = b - a; d.y = 0; float L = len(d); if (L < 0.5f) return;
    V3 f = d / L; float yaw = atan2f(f.x, f.z);
    int n = std::max(1, (int)(L / 3.0f));
    for (int i = 0; i <= n; i++) {
        V3 p = a + f * (L * i / n);
        prop(p + V3(0, 0.6f, 0), basisYPR(yaw, 0, (i & 1) ? 0.12f : -0.1f), V3(0.04f, 0.6f, 0.04f), RGBc(72, 60, 46));
    }
    Basis B = basisYPR(yaw, 0, 0);
    for (int s = 0; s < 3; s++) prop((a + b) * 0.5f + V3(0, 0.3f + s * 0.38f, 0), B, V3(0.008f, 0.008f, L * 0.5f), RGBc(58, 58, 56));
    for (int i = 0; i < (int)(L / 0.5f); i++) {   // спираль: наклонные витки
        V3 p = a + f * (0.25f + i * 0.5f);
        prop(p + V3(0, 0.45f, 0), basisYPR(yaw + 1.57f, 0, 0.9f + (i & 1) * 0.3f), V3(0.008f, 0.42f, 0.008f), RGBc(64, 64, 60));
    }
}
// трава на открытой земле: куртины кустиков, не на дорогах, постройках и в воронках
static void lawn(float density, uint32_t a, uint32_t b) { g_lawnDen = density; g_lawnA = a; g_lawnB = b; }
static void scatterLawn() {
    G.lawn.clear(); g_gmask.clear(); g_gmW = g_gmH = 0; g_gmSerial++;
    if (g_lawnDen <= 0 || g_mmW <= 0 || !G.hasGround) return;
    // маска ковра: пусто на постройках, дорогах и в воронках; проплешины и сухие пятна по шуму
    g_gmW = g_mmW; g_gmH = g_mmH; g_gmX0 = g_mmX0; g_gmZ0 = g_mmZ0;
    g_gmask.assign((size_t)g_gmW * g_gmH * 4, 0);
    for (int z = 0; z < g_gmH; z++)
        for (int x = 0; x < g_gmW; x++) {
            float wx = g_gmX0 + x + 0.5f, wz = g_gmZ0 + z + 0.5f;
            uint8_t* d = &g_gmask[((size_t)z * g_gmW + x) * 4];
            float dens = 0;
            if (g_mm[(size_t)z * g_gmW + x] == 0) {
                float n = hnoise(wx / 13.0f, wz / 13.0f, 71) * 0.65f + hnoise(wx / 4.0f, wz / 4.0f, 72) * 0.35f;
                dens = clampf(g_lawnDen * 2.3f * (0.5f + 0.9f * n), 0, 1);
                for (const MGroup& g : G.mgrp) if (g.c.y < 2.0f && dist2D(V3(wx, 0, wz), g.c) < g.r * 0.6f) { dens = 0; break; }
            }
            d[0] = (uint8_t)(dens * 255); d[1] = (uint8_t)(clampf(hnoise(wx / 21.0f + 3, wz / 21.0f, 73) * 1.3f - 0.15f, 0, 1) * 255); d[2] = 0; d[3] = 255;
        }
    for (int it = 0; it < 2; it++) {   // размытие краёв: трава редеет постепенно
        std::vector<uint8_t> src(g_gmask);
        for (int z = 0; z < g_gmH; z++)
            for (int x = 0; x < g_gmW; x++) {
                int sum = 0, n = 0;
                for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, zz = z + dz; if (xx < 0 || zz < 0 || xx >= g_gmW || zz >= g_gmH) continue;
                    sum += src[((size_t)zz * g_gmW + xx) * 4]; n++;
                }
                uint8_t& d = g_gmask[((size_t)z * g_gmW + x) * 4];
                d = g_mm[(size_t)z * g_gmW + x] == 0 ? (uint8_t)(sum / n) : 0;
            }
    }
    float area = (float)g_mmW * g_mmH;
    int clumps = std::min(2600, (int)(area * g_lawnDen / 5));
    float gy = G.groundY;
    for (int c = 0; c < clumps && G.lawn.size() < 16000; c++) {
        uint32_t h = hash2(c, 4242);
        float cx = g_mmX0 + (h & 4095) / 4095.0f * g_mmW, cz = g_mmZ0 + ((h >> 12) & 4095) / 4095.0f * g_mmH;
        int n = 4 + (int)((h >> 24) & 7);
        float cr = 0.7f + ((h >> 27) & 3) * 0.4f;
        for (int k = 0; k < n; k++) {
            uint32_t q = hash2(c * 16 + k, 777);
            float a = (q & 1023) / 1023.0f * 6.2832f, r = sqrtf(((q >> 10) & 1023) / 1023.0f) * cr;
            float x = cx + cosf(a) * r, z = cz + sinf(a) * r;
            if (mmAt(x, z) != 0) continue;
            bool hole = false;
            for (const MGroup& g : G.mgrp) if (g.c.y < 2.0f && dist2D(V3(x, 0, z), g.c) < g.r * 0.75f) { hole = true; break; }
            if (hole) continue;
            Tuft t; t.p = V3(x, gy, z); t.h = 0.28f + ((q >> 20) & 15) / 15.0f * 0.34f;
            t.col = mixC(g_lawnA, g_lawnB, ((q >> 24) & 255) / 255.0f);
            G.lawn.push_back(t);
        }
    }
}
// столб дыма (fire — с огнём у основания)
static void plume(V3 p, float size, bool fire) { Plume q; q.p = p; q.size = size; q.acc = frand(); q.fire = fire; G.plumes.push_back(q); }
// сгоревшая машина с дымом
static void burntCar(float x, float z, bool alongZ) {
    carProp(x, z, alongZ, RGBc(42, 38, 34));
    scorch(x, z, 3.0f);
    plume(V3(x, G.groundY + 1.4f, z), 1.0f, true);
}
// подбитый танк: остов без башенного огня, со шлейфом дыма
static void wreckTank(float x, float z, float yaw) {
    int v = spawnVeh(VK_TANK, V3(x, G.groundY, z), yaw, 1);
    G.veh[v].alive = false; G.veh[v].hp = 0; G.veh[v].burn = 0; G.veh[v].tag = -5; G.veh[v].orbitA = frange(-0.8f, 0.8f); G.veh[v].active = false;
    float c = cosf(yaw), s = sinf(yaw), hx = fabsf(c) * 1.9f + fabsf(s) * 3.4f, hz = fabsf(s) * 1.9f + fabsf(c) * 3.4f;
    int b = addBox(x - hx, G.groundY, z - hz, x + hx, G.groundY + 1.9f, z + hz, 0, MT_METAL, true); G.box[b].vis = false;
    scorch(x, z, 4.5f);
    plume(V3(x, G.groundY + 2.2f, z), 1.4f, true);
}
// набор «следов боя» в прямоугольнике: воронки, гарь, ежи (не ближе clear к точкам из списка)
static void battleScars(float x0, float z0, float x1, float z1, int nCrater, int nHedge, int seed, std::initializer_list<V3> keepOut, float clear) {
    auto ok = [&](float x, float z) { for (const V3& k : keepOut) if (dist2D(V3(x, 0, z), k) < clear) return false; return true; };
    for (int i = 0; i < nCrater; i++) {
        uint32_t h = hash2(i, seed); float x = x0 + (h & 1023) / 1023.0f * (x1 - x0), z = z0 + ((h >> 10) & 1023) / 1023.0f * (z1 - z0);
        if (!ok(x, z)) continue;
        if (i % 3 == 2) scorch(x, z, 1.5f + (h >> 20 & 3) * 0.5f); else crater(x, z, 1.2f + (h >> 20 & 3) * 0.45f);
    }
    for (int i = 0; i < nHedge; i++) {
        uint32_t h = hash2(i + 500, seed); float x = x0 + (h & 1023) / 1023.0f * (x1 - x0), z = z0 + ((h >> 10) & 1023) / 1023.0f * (z1 - z0);
        if (!ok(x, z)) continue;
        hedgehog(x, z, (h >> 22) * 0.01f);
    }
}
// дымы и дальняя канонада
static void warUpdate(float dt) {
    for (auto& q : G.plumes) {
        float d = len(q.p - G.P.pos);
        float rate = d > 120 ? 1.4f : 2.6f;
        q.acc += dt * rate;
        while (q.acc >= 1) {
            q.acc -= 1;
            if (G.parts.size() > 620) break;
            float s = q.size;
            spawnPart(q.p + V3(frange(-0.5f, 0.5f) * s, 0, frange(-0.5f, 0.5f) * s), V3(0.5f + frange(-0.2f, 0.2f), frange(1.6f, 2.4f) * (0.7f + s * 0.3f), 0.25f),
                      frange(9, 14), 1.2f * s, mixC(RGBc(34, 32, 30), RGBc(78, 74, 70), frand() * 0.6f), RM_BLEND, -0.05f, 0.75f * s, 0.04f);
        }
        if (q.fire && d < 140 && frand() < dt * 9) spawnPart(q.p + V3(frange(-0.6f, 0.6f), -0.4f, frange(-0.6f, 0.6f)) * q.size, V3(0, frange(1, 2.2f), 0), frange(0.4f, 0.8f), 0.6f * q.size, RGBc(255, (int)frange(110, 170), 40), RM_ADD, -0.5f, 0.6f);
    }
    if (!G.war) return;
    G.warT -= dt;
    if (G.warT <= 0) {
        G.warT = frange(4, 11);
        float a = frange(0, 2 * PI_F), d = frange(170, 240);
        V3 p = G.P.pos + V3(cosf(a) * d, 0, sinf(a) * d); p.y = G.groundY + 2;
        for (int k = 0; k < 6; k++) spawnPart(p + V3(0, k * 1.5f, 0), V3(frange(-3, 3), frange(2, 8), frange(-3, 3)), frange(0.25f, 0.5f), frange(6, 10), RGBc(255, 190, 110), RM_ADD, 0, 6);
        for (int k = 0; k < 5; k++) spawnPart(p + V3(frange(-4, 4), frange(0, 4), frange(-4, 4)), V3(frange(-1, 1), frange(1, 3), frange(-1, 1)), frange(6, 9), frange(5, 8), RGBc(70, 66, 60), RM_BLEND, -0.05f, 1.5f, 0.1f);
        float vol, pan, dd; spatial(p, vol, pan, dd);
        playSnd(ST_EXPLO, 0.22f, pan, 0.9f, 2.6f);
        g_env.flash = std::max(g_env.flash, 0.06f);
    }
}
static int countAlive(int tag) {
    int n = 0;
    for (auto& a : G.act) if (a.alive && a.team == 1 && a.tag == tag && (a.kind == AK_SOLDIER || a.kind == AK_DOG || a.kind == AK_TARGET)) n++;
    return n;
}
static void setObjective(const char* text, V3 wp, bool wpOn) {
    G.objective = text; G.objT = 7; G.wp = wp; G.wpOn = wpOn;
    playSnd(ST_BEEP, 0.3f, 0, 0, 0.12f, 0, 0, 880);
}
static void showStory(const std::vector<std::string>& lines, float dur, int next, bool freeze, uint32_t bg) {
    G.story = lines; G.storyT = 0; G.storyDur = dur; G.storyNext = next; G.storyFreeze = freeze; G.storyBg = bg;
}
static void checkpoint() {
    G.cpStage = G.stage; PR.cpMission = g_ch; PR.cpStage = G.stage;
    G.fl[20] = 2.5f;
    saveAll();
}
static void refillAmmo() {
    for (int s = 0; s < 2; s++) if (G.P.w[s].def >= 0) G.P.w[s].reserve = std::max(G.P.w[s].reserve, WD[G.P.w[s].def].reserveMax / 2);
}

// =====================================================================
//  КАМПАНИЯ: 4 операции, 12 глав
// =====================================================================
struct MissionInfo {
    const char* title; const char* sub; const char* brief[5]; const char* goal; float mx, my; bool vargas;
    int op, mission, startStage, endStage; const char* outro[3];
};
struct OpInfo { const char* name; const char* hero; const char* tag; };
static const int NOPS = 5, NCH = 13;
static const OpInfo OPS[NOPS] = {
    {"ШТОРМ НАД КАРАЛИЕЙ", "Сержант Андрей Котов, позывной «Кот»", "Первые дни в отряде «Гром»."},
    {"ТЕНЬ", "Молодой Кот и лейтенант Ирина Власова, «Сова»", "Год назад. Охота на Ворона."},
    {"СТАЛЬНОЙ РУБЕЖ", "Сержант Котов и отряд «Гром»", "Передать координаты и прорваться к горам."},
    {"ПОСЛЕДНИЙ ОТСЧЁТ", "Сержант Котов и отряд «Гром»", "Перевал, электростанция, ракеты."},
    {"ПУСТЫННЫЙ ВЕТЕР", "Сержант Котов и отряд «Гром»", "ОАЭ. Последняя боеголовка."},
};
// главы: каждая операция состоит из нескольких глав (подмиссий)
static const MissionInfo MI[NCH] = {
    {"ПОЛИГОН", "База «Гром», учебный центр. 2027 год.",
     {"Сержант Котов, добро пожаловать в отряд «Гром».", "Капитан Соболев проверит вас лично: стрельба, гранаты, стойки.",
      "Затем — «дом-убийца» на время. Лучший результат пойдёт в личное дело.", "Не расслабляйтесь. Обстановка у Каральской Республики накаляется.", nullptr},
     "Пройти курс подготовки", 0.18f, 0.72f, false, 0, 0, 0, -1, {nullptr, nullptr, nullptr}},
    {"АБОРДАЖ", "Каральское море. Ночь, шторм.",
     {"Генерал Варгас захватил власть в Каральской Республике.", "По данным разведки, он скупает ракетные комплексы у «Синдиката Ворона».",
      "Грузовое судно Синдиката идёт к побережью. Спуститесь по тросам на палубу.", "Зачистите палубу и трюм. Нам нужны документы о грузе.", nullptr},
     "Захватить документы на судне", 0.42f, 0.55f, false, 0, 1, 0, 4, {"Над морем — рёв двигателей.", "Это не наши самолёты.", nullptr}},
    {"ТОНУЩЕЕ СУДНО", "Каральское море. Тот же шторм.",
     {"Судно атаковано истребителями и идёт ко дну.", "Трюм заливает, палуба горит. Вертолёт ждёт на носу.", "У вас девяносто секунд.", nullptr, nullptr},
     "Добраться до вертолёта", 0.40f, 0.50f, false, 0, 1, 4, -1, {nullptr, nullptr, nullptr}},
    {"СТАРЫЙ РЫНОК", "Порт Каралия. День.",
     {"Документы с судна вывели нас на связного Варгаса.", "Он прячется в квартале старого рынка. Ополчение Варгаса держит улицы.",
      "Будьте осторожны: стрелки на балконах, узкие улочки.", "Связной нужен живым. Он знает, где ракеты.", nullptr},
     "Захватить связного Варгаса", 0.58f, 0.62f, true, 0, 2, 0, -1, {nullptr, nullptr, nullptr}},
    {"ПОЛЗКОМ", "ГОД НАЗАД. Пригород Велеса. Сумерки.",
     {"Флешбэк. Лейтенант Власова, «Сова», и молодой сержант Котов.", "Цель: Эрик Холстен, «Ворон». Глава Синдиката, бывший офицер разведки.",
      "Он проводит сделку в заброшенной промзоне. Дистанция выстрела — около 900 метров.",
      "Двигайтесь скрытно. Патрули, собаки, вертолёт. Если вас обнаружат — операция сорвана.", nullptr},
     "Выйти на позицию и поразить Ворона", 0.70f, 0.32f, false, 1, 3, 0, 5, {"Попадание. Ворон падает.", "Но через прицел он успел увидеть стрелка.", nullptr}},
    {"ОТХОД", "ГОД НАЗАД. Тот же вечер.",
     {"Выстрел выдал позицию. Сова ранена.", "Вынесите её к точке эвакуации и продержитесь до вертолёта.",
      "С раненой на плечах — только пистолет.", nullptr, nullptr},
     "Эвакуировать Сову", 0.68f, 0.36f, false, 1, 3, 5, -1, {nullptr, nullptr, nullptr}},
    {"РУБЕЖ", "Телецентр, город Сарен. Вечер.",
     {"Связной назвал место: заброшенная электростанция в горах, ущелье Тарса.", "Координаты нужно передать в штаб через телецентр. Связь глушат.",
      "Синдикат знает, где вы. Будут атаковать волнами: пехота, техника, вертолёт, танк.", "Держитесь до окончания передачи. Это приказ.", nullptr},
     "Удержать телецентр до конца передачи", 0.52f, 0.40f, true, 2, 4, 0, -1, {nullptr, nullptr, nullptr}},
    {"ПРИКРЫТИЕ", "Автобаза под Сареном. Утро.",
     {"Чтобы дойти до гор, отряду нужна броня. На автобазе Синдиката стоит танк «Молот».", "Седой с пятью бойцами пойдёт на штурм. Вы с Совой — на высоте над полем.",
      "Ваша задача — прикрыть их атаку: окоп, пулемёты, контратаки, гранатомётчики.", "Если группа погибнет или танк сожгут — операция провалена.", nullptr},
     "Прикрыть штурм автобазы и захват танка", 0.58f, 0.36f, false, 2, 8, 0, -1, {nullptr, nullptr, nullptr}},
    {"СТАЛЬНОЙ КУЛАК", "Долина Ирмы. Закат.",
     {"Координаты переданы. До Тарсы — сорок километров по долине.", "Отряду достался трофейный танк «Молот». Седой — командир, Кот — наводчик.",
      "Зенитки в деревне прикрывают долину. Уничтожьте их.", "Пули танку не страшны. Опасайтесь РПГ и вражеских танков.", nullptr},
     "Пробиться к мосту через Ирму", 0.64f, 0.30f, false, 2, 6, 0, -1, {nullptr, nullptr, nullptr}},
    {"ПЕРЕВАЛ", "Перевал Тарса. Снегопад, сумерки.",
     {"Дорога на электростанцию идёт через перевал.", "Три радара прикрывают ГЭС с воздуха. Пока они работают, авиация не поможет.",
      "Заложите заряды на всех трёх. На гребне — миномётный расчёт.", nullptr, nullptr},
     "Уничтожить радары на перевале", 0.74f, 0.22f, false, 3, 7, 0, -1, {nullptr, nullptr, nullptr}},
    {"ШТУРМ", "Ущелье Тарса. Рассвет.",
     {"Ракеты Варгаса — на площадке за машинным залом старой ГЭС.", "Прорвитесь через КПП, туннель и машзал.",
      "Ворон где-то рядом. Его джип видели у ворот.", nullptr, nullptr},
     "Прорваться через КПП, туннель и машзал", 0.80f, 0.18f, false, 3, 5, 0, 3, {"Машинный зал позади.", "Впереди — стартовая площадка. И таймер пуска.", nullptr}},
    {"ПОСЛЕДНИЙ ОТСЧЁТ", "Ущелье Тарса. Рассвет.",
     {"Ракеты на стартовой площадке. Пуск через три минуты.", "Пульт управления — в диспетчерской.",
      "По перехвату, Ворон лично руководит охраной. Он ждёт вас, Котов.", nullptr, nullptr},
     "Остановить пуск ракет", 0.82f, 0.15f, false, 3, 5, 3, -1, {nullptr, nullptr, nullptr}},
    {"ДЖЕБЕЛЬ-АЛИ", "ОАЭ, порт Джебель-Али под Дубаем. Ночь.",
     {"Пуск сорван, но одной боеголовки на площадке не было.", "Её вывезли морем за неделю до штурма. Контейнер выгружен в Джебель-Али.",
      "На заряде запущен таймер. Номер контейнера неизвестен — Док ведёт сканер.", "Найти, вскрыть, обезвредить. Огни Дубая — в тридцати километрах.", nullptr},
     "Найти и обезвредить боеголовку в порту", 0.96f, 0.94f, false, 4, 9, 0, -1, {nullptr, nullptr, nullptr}},
};
static void setStage(int s);
static void missionFail(const std::string& msg) {
    if (G.failed || G.missionDone) return;
    G.failed = true; G.failMsg = msg; G.endT = 1.6f;
}
static void missionComplete() {
    if (G.failed || G.missionDone) return;
    G.missionDone = true; G.endT = 1.2f;
}
static void placeAllies() {
    V3 F(sinf(G.P.yaw), 0, cosf(G.P.yaw)), R(cosf(G.P.yaw), 0, -sinf(G.P.yaw));
    for (auto& a : G.act) {
        if (a.kind != AK_ALLY || a.scripted) continue;
        static const float off[4][2] = {{-1.8f, -2.2f}, {1.8f, -2.4f}, {0.0f, -4.0f}, {-2.5f, -5.0f}};
        V3 p = G.P.pos + R * off[a.formation & 3][0] + F * off[a.formation & 3][1];
        if (bodyBlocked(p, 0.3f, 1.6f)) p = G.P.pos - F * 1.0f;
        a.pos = p; a.yaw = G.P.yaw;
    }
}
static void allyScript(const char* name, V3 t) { int i = findAlly(name); if (i >= 0) { G.act[i].scripted = true; G.act[i].target = t; } }
static void allyFree(const char* name) { int i = findAlly(name); if (i >= 0) G.act[i].scripted = false; }

// ------------------------------------------------------------ М1 ПОЛИГОН
static void buildM1() {
    setEnv(V3(0.4f, 0.8f, 0.3f), 0.55f, 0.5f, RGBc(185, 200, 215), 60, 230, RGBc(105, 148, 205), RGBc(190, 205, 222), 240);
    setGround(RGBc(152, 140, 100), RGBc(128, 134, 86), 0);
    bounds(-60, -45, 60, 70);
    sandbags(-9, 4.0f, 9, 4.6f, 1.0f);
    for (int i = -3; i <= 3; i++) addDeco(i * 3 - 0.05f, 0, 4.6f, i * 3 + 0.05f, 1.4f, 4.7f, RGBc(210, 205, 190));
    addBox(-20, 0, 52, 20, 4, 58, RGBc(120, 105, 75));
    addBox(-21.5f, 0, 6, -20.5f, 2.5f, 52, RGBc(120, 105, 75));
    addBox(11, 0, 6, 12, 2.5f, 52, RGBc(120, 105, 75));
    uint32_t pc = RGBc(140, 140, 135);
    addBox(22, 0, 16, 30, 0.8f, 16.3f, pc, MT_CONCRETE, true); addBox(22, 0, 23.7f, 30, 0.8f, 24, pc, MT_CONCRETE, true);
    addBox(22, 0, 16.3f, 22.3f, 0.8f, 23.7f, pc, MT_CONCRETE, true); addBox(29.7f, 0, 16.3f, 30, 0.8f, 23.7f, pc, MT_CONCRETE, true);
    sandbags(23, 7, 29, 7.6f, 1.0f);
    uint32_t kc = RGBc(165, 135, 95);
    wallX(4, -44, -24, 2.8f, 0.15f, kc, {-34}, {}, MT_WOOD);
    wallX(28, -44, -24, 2.8f, 0.15f, kc, {-28}, {}, MT_WOOD);
    wallZ(-44, 4, 28, 2.8f, 0.15f, kc, {}, {}, MT_WOOD);
    wallZ(-24, 4, 28, 2.8f, 0.15f, kc, {}, {}, MT_WOOD);
    wallX(12, -44, -24, 2.8f, 0.15f, kc, {-38, -30}, {}, MT_WOOD);
    wallZ(-34, 12, 28, 2.8f, 0.15f, kc, {20}, {}, MT_WOOD);
    wallX(20, -44, -34, 2.8f, 0.15f, kc, {-40}, {}, MT_WOOD);
    crate(-42.5f, 7, 0.5f, 1.0f); crate(-25.6f, 24.5f, 0.5f, 1.0f); crate(-36, 16, 0.45f, 0.9f);
    addDeco(-6, 0, -31, 6, 0.02f, -19, RGBc(80, 80, 80));
    addDeco(-2, 0.02f, -27, -1.4f, 0.04f, -23, RGBc(230, 230, 230)); addDeco(1.4f, 0.02f, -27, 2, 0.04f, -23, RGBc(230, 230, 230));
    addDeco(-1.4f, 0.02f, -25.3f, 1.4f, 0.04f, -24.7f, RGBc(230, 230, 230));
    addBox(36, 0, -26, 44, 2.4f, -20, RGBc(90, 100, 70), MT_CLOTH); addBox(36, 0, -14, 44, 2.4f, -8, RGBc(90, 100, 70), MT_CLOTH);
    int t = spawnVeh(VK_TRUCK, V3(-14, 0, -14), 0.4f, 0); G.veh[t].arrived = true; G.veh[t].solidAdded = true;
    int tk = spawnVeh(VK_TANK, V3(22, 0, -26), 1.2f, 0); G.veh[tk].arrived = true; G.veh[tk].solidAdded = true; G.veh[tk].orbitA = -0.6f;
    addBox(18.6f, 0, -29.6f, 25.4f, 2.0f, -22.4f, 0, MT_INVIS); G.box.back().mat = MT_METAL;
    addBox(-15.5f, 0, -17, -12.5f, 1.9f, -11, 0, MT_INVIS); G.box.back().mat = MT_METAL;
    for (int k = 0; k < 4; k++) addDeco(-50 + (k & 1) * 3.0f, 0, -30 + (k >> 1) * 3.0f, -49.7f + (k & 1) * 3.0f, 8, -29.7f + (k >> 1) * 3.0f, RGBc(90, 80, 70));
    addDeco(-50.5f, 8, -30.5f, -46.5f, 11, -26.5f, RGBc(110, 100, 90));
    for (float x = -57; x <= 57; x += 6) addDeco(x - 0.06f, 0, -44.6f, x + 0.06f, 2.2f, -44.4f, RGBc(90, 90, 90));
    int s = spawnAlly(V3(2, 0, 2), "Седой", U_GROM, 0); G.act[s].scripted = true; G.act[s].target = V3(2, 0, 2);
    int d = spawnAlly(V3(26, 0, 5), "Док", U_DOC, 1); G.act[d].scripted = true; G.act[d].target = V3(26, 0, 5);
    giveWeapon(0, W_AR, 120); giveWeapon(1, W_PISTOL, 45);
    G.P.nade[0] = 3; G.P.nade[1] = 1; G.P.nade[2] = 1;
    g_amb.wind = 0.15f; g_amb.echoAmt = 0.35f;
}
static void stageM1(int s) {
    switch (s) {
    case 0: {
        say("Седой", "Кот, добро пожаловать в «Гром». Посмотрим, чему тебя учили.");
        say("Седой", "Винтовка у тебя. Мишени — от двадцати до сорока пяти метров.");
        say("Седой", "Целься через мушку. Поразить все шесть.");
        setObjective("Поразите 6 мишеней на стрельбище", V3(0, 0, 30), true);
        V3 p[6] = {V3(-6, 0, 20), V3(0, 0, 24), V3(6, 0, 21), V3(-4, 0, 32), V3(4, 0, 37), V3(0, 0, 45)};
        for (auto& q : p) G.act[spawnTarget(q, false, PI_F)].tag = 0;
    } break;
    case 1:
        say("Седой", "Неплохо. Теперь стойки. Присядь.");
        setObjective(g_lastDev == 0 ? "Присядьте: кнопка ПРИСЕД" : (g_lastDev == 1 ? "Присядьте: кнопка B" : "Присядьте: клавиша C"), V3(), false); G.iv[0] = 0; break;
    case 2: {
        say("Док", "Это осколочная. Чека, держишь — пошёл отсчёт. Не передержи.");
        say("Док", "Закинь её в окоп. Мишени не убегут, они деревянные.");
        setObjective("Бросьте осколочную гранату в окоп", V3(26, 0, 20), true);
        V3 p[3] = {V3(24, 0, 19), V3(26.5f, 0, 21.5f), V3(28, 0, 18)};
        for (auto& q : p) G.act[spawnTarget(q, false, PI_F)].tag = 2;
        if (G.P.nade[0] < 2) G.P.nade[0] = 2;
    } break;
    case 3: {
        checkpoint();
        say("Седой", "Дом-убийца. Время пошло, как войдёшь.");
        say("Седой", "Бело-синие мишени — гражданские. Не трогать.");
        setObjective("Пройдите «дом-убийцу» на время", V3(-34, 0, 3), true);
        struct T { float x, z, yaw; bool civ; };
        T ts[] = {{-41, 9, 0.6f, false}, {-28, 9.5f, -0.5f, true}, {-26, 6, -1.4f, false}, {-42, 16, 1.2f, false}, {-37, 18.5f, 0.3f, false},
                  {-42, 26, 1.0f, true}, {-38, 24.5f, 2.6f, false}, {-26, 22, -2.0f, false}, {-30, 26, 3.0f, false}, {-27, 15, -1.0f, true}};
        for (auto& q : ts) { int i = spawnTarget(V3(q.x, 0, q.z), q.civ, q.yaw + PI_F); G.act[i].tag = q.civ ? 9 : 3; }
        refillAmmo(); G.iv[1] = 0;
    } break;
    case 4: {
        say("Майор Грин", "«Гром», это Грин. Тревога. Вылет немедленно.", true);
        say("Седой", "Слышал, Кот? Тренировка окончена. К вертолёту!");
        int h = spawnVeh(VK_HELI, V3(80, 30, -60), -1.5f, 0);
        G.veh[h].path = {V3(20, 15, -35), V3(0, 2.3f, -25)}; G.veh[h].speed = 16; G.iv[2] = h;
        allyFree("Седой"); allyFree("Док");
        setObjective("Бегом к вертолёту", V3(0, 0, -25), true);
        G.timerOn = false;
    } break;
    }
}
static void updateM1(float dt) {
    switch (G.stage) {
    case 0: if (countAlive(0) == 0 && G.stageT > 1) setStage(1); break;
    case 1:
        if (G.iv[0] == 0 && G.P.stance == 1) { G.iv[0] = 1; say("Седой", "Теперь ляг."); setObjective(g_lastDev == 0 ? "Лягте: кнопка ЛЕЧЬ" : (g_lastDev == 1 ? "Лягте: удерживайте B" : "Лягте: клавиша Z"), V3(), false); }
        else if (G.iv[0] == 1 && G.P.stance == 2) { G.iv[0] = 2; say("Седой", "Встать. Бегом к Доку, на гранатную позицию."); setObjective("Бегом к гранатной позиции", V3(26, 0, 6), true); }
        else if (G.iv[0] == 2 && dist2D(G.P.pos, V3(26, 0, 6)) < 3) setStage(2);
        break;
    case 2:
        if (countAlive(2) == 0) { say("Док", "Ба-бах. Окоп чист, а ты — молодец."); setStage(3); }
        else if (G.P.nade[0] == 0 && G.gren.empty()) { G.P.nade[0] = 1; say("Док", "Держи ещё одну. Бесплатно, но последняя."); }
        break;
    case 3: {
        bool inside = G.P.pos.x > -44 && G.P.pos.x < -24 && G.P.pos.z > 4.3f && G.P.pos.z < 28;
        if (G.iv[1] == 0 && inside) { G.iv[1] = 1; G.fl[0] = 0; G.timerOn = true; G.timerLabel = "Время"; G.iv[3] = 1; playSnd(ST_BEEP, 0.4f, 0, 0, 0.3f, 0, 0, 1000); }
        if (G.iv[1] == 1) {
            G.fl[0] += dt; G.timer = G.fl[0];
            if (G.P.pos.z > 28.6f && G.P.pos.x > -31 && G.P.pos.x < -25) {
                G.iv[1] = 2; G.timerOn = false;
                int civ = 0, missed = 0;
                for (auto& a : G.act) { if (a.kind == AK_CIVTARGET && !a.alive) civ++; if (a.kind == AK_TARGET && a.tag == 3 && a.alive) missed++; }
                float total = G.fl[0] + civ * 3.0f + missed * 5.0f;
                bool rec = PR.bestKH <= 0 || total < PR.bestKH;
                if (rec) PR.bestKH = total;
                saveAll();
                showStory({fmt("Время: %.1f с", G.fl[0]), fmt("Штраф: гражданские %d (+3 с), пропущено %d (+5 с)", civ, missed),
                           fmt("Итог: %.1f с%s", total, rec ? "  — НОВЫЙ РЕКОРД" : ""), fmt("Лучший результат: %.1f с", PR.bestKH)}, 5.0f, 4, false, 0);
                if (civ > 0) say("Седой", "Гражданских не трогать, Кот. В бою это будут люди.");
                else say("Седой", "Чисто. Годится.");
            }
        }
    } break;
    case 4: {
        int h = G.iv[2];
        bool landed = h < (int)G.veh.size() && G.veh[h].pi >= (int)G.veh[h].path.size();
        g_amb.heli = 1.0f / (1.0f + len(G.veh[h].pos - G.P.pos) * 0.04f); g_amb.heliRate = 5.5f;
        if (landed && dist2D(G.P.pos, V3(0, 0, -25)) < 5 && G.storyNext != -2)
            showStory({"По тревоге отряд поднимается в воздух.", "Курс — Каральское побережье."}, 4.0f, -2, true, RGBc(10, 14, 20));
    } break;
    }
}

// ------------------------------------------------------------ М2 ШТОРМ
static void buildM2() {
    setEnv(V3(-0.3f, 0.8f, 0.2f), 0.32f, 0.25f, RGBc(16, 20, 28), 6, 70, RGBc(8, 10, 16), RGBc(22, 26, 36), 85);
    G.hasGround = false; G.sea = true; G.groundY = -3; G.gcol1 = RGBc(18, 30, 42); G.gcol2 = RGBc(28, 42, 56);
    G.rain = true; G.lightning = true;
    uint32_t hull = RGBc(80, 40, 36), deck = RGBc(72, 74, 72), rail = RGBc(150, 150, 140);
    addBox(-8.2f, -9, -41, -7.8f, 0.4f, 41, hull, MT_METAL); addBox(7.8f, -9, -41, 8.2f, 0.4f, 41, hull, MT_METAL);
    addBox(-8.2f, -9, 40.8f, 8.2f, 0.4f, 41.2f, hull, MT_METAL); addBox(-8.2f, -9, -41.2f, 8.2f, 0.4f, -40.8f, hull, MT_METAL);
    addBox(-7.8f, -0.3f, -40.8f, 7.8f, 0, 12, deck, MT_METAL); addBox(-7.8f, -0.3f, 19, 7.8f, 0, 40.8f, deck, MT_METAL);
    addBox(-7.8f, -0.3f, 12, -1.5f, 0, 19, deck, MT_METAL); addBox(1.5f, -0.3f, 12, 7.8f, 0, 19, deck, MT_METAL);
    addBox(-7.8f, 0, -30, -7.5f, 1.0f, 40, rail, MT_METAL); addBox(7.5f, 0, -30, 7.8f, 1.0f, 40, rail, MT_METAL);
    addBox(-1.85f, 0, 12, -1.5f, 0.6f, 19, rail, MT_METAL); addBox(1.5f, 0, 12, 1.85f, 0.6f, 19, rail, MT_METAL);
    addBox(-1.85f, 0, 19, 1.85f, 0.6f, 19.3f, rail, MT_METAL);
    uint32_t sup = RGBc(190, 190, 180);
    addBox(-7, 0, -40.5f, 7, 8, -31, sup);
    for (int k = -2; k <= 2; k++) addDeco(k * 2.6f - 0.6f, 5, -31.05f, k * 2.6f + 0.6f, 6.2f, -30.95f, RGBc(200, 180, 110), true);
    addDeco(-0.15f, 8, -36.15f, 0.15f, 16, -35.85f, RGBc(60, 60, 60)); lamp(0, 16.2f, -36, RGBc(255, 60, 40));
    container(-5.5f, -22, true, RGBc(140, 40, 35)); container(4.8f, -17, true, RGBc(40, 70, 120)); container(-5.2f, -6, true, RGBc(50, 100, 60));
    container(5, 2, true, RGBc(150, 110, 40)); container(-5.5f, 26, true, RGBc(100, 100, 110)); container(5, 30, true, RGBc(140, 40, 35));
    container(5, 30, true, RGBc(60, 90, 70), 2.6f);
    crate(-2, -12, 0.6f, 1.1f); crate(2.5f, -6, 0.6f, 1.1f); crate(-1.5f, 4, 0.6f, 1.1f); crate(3, 22, 0.6f, 1.1f); crate(-2.5f, 33, 0.6f, 1.1f);
    addBox(1, 0, 8, 3.5f, 1.0f, 9, RGBc(90, 95, 90), MT_METAL, true); addBox(-4.5f, 0, -27, -2, 1.0f, -26, RGBc(90, 95, 90), MT_METAL, true);
    lamp(-7.4f, 3, -20, RGBc(255, 220, 150)); lamp(7.4f, 3, 0, RGBc(255, 220, 150)); lamp(-7.4f, 3, 20, RGBc(255, 220, 150)); lamp(7.4f, 3, 35, RGBc(255, 220, 150));
    // трюм
    uint32_t hold = RGBc(60, 60, 56);
    addBox(-7.8f, -5.3f, -10, 7.8f, -5, 32, hold, MT_METAL);
    addBox(-7.8f, -5, -10.3f, 7.8f, -0.3f, -10, hold, MT_METAL); addBox(-7.8f, -5, 32, 7.8f, -0.3f, 32.3f, hold, MT_METAL);
    for (int i = 0; i < 14; i++) addBox(-1.4f, -5, 12 + i * 0.5f, 1.4f, -(i + 1) * 0.357f, 12 + (i + 1) * 0.5f, RGBc(95, 95, 90), MT_METAL);
    container(-4.5f, 0, true, RGBc(120, 60, 40), -5); container(4.5f, -4, true, RGBc(50, 70, 100), -5);
    crate(3, 8, 0.6f, 1.2f, -5); crate(-3.5f, 14, 0.6f, 1.2f, -5); crate(-4.5f, 21, 0.6f, 1.2f, -5); crate(4.5f, 19, 0.6f, 1.2f, -5); crate(0, 26, 0.5f, 1.0f, -5);
    addBox(-4.6f, -5, 25.5f, -3.4f, -4.2f, 26.5f, RGBc(90, 70, 50), MT_WOOD, true);
    addDeco(-4.2f, -4.2f, 25.8f, -3.8f, -4.15f, 26.1f, RGBc(230, 230, 220), true);
    addBox(2.4f, -5, 25, 5.4f, -3.6f, 30.5f, RGBc(70, 80, 60), MT_WOOD);
    for (int k = 0; k < 4; k++) addDeco(2.35f, -4.6f, 25.4f + k * 1.3f, 2.4f, -4.0f, 26.0f + k * 1.3f, k & 1 ? RGBc(230, 230, 220) : RGBc(200, 40, 30), true);
    lamp(0, -0.4f, -2, RGBc(255, 210, 140)); lamp(0, -0.4f, 9, RGBc(255, 210, 140)); lamp(-3, -0.4f, 25, RGBc(255, 210, 140));
    G.waterMn = V3(-7.8f, 0, -10); G.waterMx = V3(7.8f, 0, 32);
    bounds(-7.8f, -40.8f, 7.8f, 40.6f);
    int a = spawnAlly(V3(-2, 0, -26), "Седой", U_GROM, 0); G.act[a].hidden = true;
    a = spawnAlly(V3(2, 0, -26), "Док", U_DOC, 1); G.act[a].hidden = true;
    a = spawnAlly(V3(0, 0, -28), "Сова", U_GROM, 2); G.act[a].hidden = true;
    giveWeapon(0, W_SMG, 160); giveWeapon(1, W_SHOTGUN, 24);
    G.P.nade[0] = 2; G.P.nade[1] = 2; G.P.nade[2] = 1;
    g_amb.rain = 0.9f; g_amb.wind = 0.45f; g_amb.echoAmt = 0.12f;
}
static void stageM2(int s) {
    switch (s) {
    case 0:
        showStory({"02:40. Каральское море.", "Спуск по тросам."}, 3.0f, -1, false, 0);
        say("Седой", "Тросы пошли. Без лишнего шума, пока можно.");
        break;
    case 1: {
        for (auto& a : G.act) if (a.kind == AK_ALLY) a.hidden = false;
        placeAllies();
        setObjective("Зачистите палубу", V3(0, 0, 10), false);
        V3 p[8] = {V3(-3, 0, -16), V3(3, 0, -10), V3(-6, 0, -1), V3(2, 0, 6), V3(-3.5f, 0, 10), V3(5, 0, 16), V3(-4, 0, 21), V3(2, 0, 35)};
        for (int i = 0; i < 8; i++) spawnEnemy(p[i], U_SYND, i % 3 == 1 ? 1 : 0, false, 1);
        int e = spawnEnemy(V3(-5.5f, 2.6f, 26), U_SYND, 0, false, 1); G.act[e].stationary = true;
        say("Док", "Ну и погодка. Самое то для круиза.");
    } break;
    case 2:
        checkpoint();
        say("Седой", "Палуба чиста. Спускаемся в трюм. Я первый, Кот — за мной.");
        setObjective("Спуститесь в трюм", V3(0, 0, 12.5f), true);
        break;
    case 3: {
        setObjective("Зачистите трюм", V3(0, -5, 20), false);
        V3 p[6] = {V3(-3, -5, 3), V3(3, -5, 4.5f), V3(0, -5, 23), V3(-5.5f, -5, 17), V3(5.5f, -5, 23.5f), V3(-2, -5, 29)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, i & 1, true, 3);
        G.iv[0] = 0;
    } break;
    case 4: {
        checkpoint();
        say("Майор Грин", "«Гром», воздух! Два борта, не наши! Уходите!", true);
        say("Седой", "Судно тонет! Все наверх, на нос! Бегом!");
        int j = spawnVeh(VK_JET, V3(-260, 45, 80), 0, 1); G.veh[j].path = {V3(260, 40, -60)};
        j = spawnVeh(VK_JET, V3(-280, 50, 60), 0, 1); G.veh[j].path = {V3(250, 45, -80)};
        G.timerOn = true; G.timer = 90; G.timerLabel = "До затопления"; G.iv[3] = 0;
        G.waterOn = true; G.waterY = -5;
        int h = spawnVeh(VK_HELI, V3(0, 20, 120), PI_F, 0); G.veh[h].path = {V3(0, 6, 60), V3(0, 4.5f, 45)}; G.veh[h].speed = 12; G.iv[2] = h;
        setObjective("Бегом к носу судна — вертолёт ждёт", V3(0, 0, 39), true);
        spawnEnemy(V3(-3, 0, 36), U_SYND, 0, true, 4); spawnEnemy(V3(4, 0, 38), U_SYND, 0, true, 4);
        G.fl[0] = 0; G.iv[4] = 0;
    } break;
    }
}
static void updateM2(float dt) {
    // молнии
    G.lightT -= dt;
    if (G.lightT <= 0) { G.lightT = frange(5, 12); g_env.flash = 0.9f; G.fl[9] = frange(0.4f, 1.5f); }
    if (G.fl[9] > 0) { G.fl[9] -= dt; if (G.fl[9] <= 0) playSnd(ST_EXPLO, 0.35f, frange(-0.5f, 0.5f), 0.6f, 2.5f); }
    G.roll = sinf(G.time * 0.6f) * 0.045f + G.tilt;
    switch (G.stage) {
    case 0:
        G.P.pos = V3(0, std::max(0.0f, 12 - G.stageT * 3.2f), -24); G.P.vy = 0; G.P.stance = 0;
        if (G.P.pos.y <= 0.001f) setStage(1);
        break;
    case 1: if (countAlive(1) == 0 && G.stageT > 2) setStage(2); break;
    case 2: if (G.P.pos.z > 12 && G.P.pos.y < -1) setStage(3); break;
    case 3:
        if (G.iv[0] == 0 && countAlive(3) == 0) { G.iv[0] = 1; say("Седой", "Чисто. Ищи документы — стол у дальней переборки."); setObjective("Найдите документы", V3(-4, -5, 26), true); }
        if (G.iv[0] == 1 && dist2D(G.P.pos, V3(-4, -5, 26)) < 2.0f && G.P.pos.y < -3) {
            G.iv[0] = 2; say("Кот", "Документы у меня."); say("Сова", "Кот, ящик справа. Посмотри маркировку.");
            setObjective("Осмотрите ящик", V3(3.9f, -5, 27.5f), true);
        }
        if (G.iv[0] == 2 && dist2D(G.P.pos, V3(3.9f, -5, 27.5f)) < 3.2f && G.P.pos.y < -3) {
            G.iv[0] = 3;
            showStory({"Маркировка на ящике: «БУРЯ-М».", "Оперативно-тактический ракетный комплекс."}, 3.5f, 4, false, 0);
            say("Седой", "Это не контрабанда. Это ракеты.");
        }
        break;
    case 4: {
        G.fl[0] += dt;
        if (G.iv[4] == 0 && G.fl[0] > 3.0f) { G.iv[4] = 1; explode(V3(-4, 0.2f, -14), 1, 6, 120, false); explode(V3(4, 0.2f, 24), 1, 6, 120, false); G.P.shake = 1; }
        if (G.iv[4] == 1 && G.fl[0] > 5.0f) { G.iv[4] = 2; explode(V3(0, 0.2f, -30), 1, 6, 120, false); }
        G.tilt = -std::min(0.16f, G.fl[0] * 0.004f);
        G.waterY = std::min(-0.35f, -5 + G.fl[0] * 0.075f);
        G.timer -= dt;
        V3 fires[3] = {V3(-4, 0, -14), V3(4, 0, 24), V3(0, 0, -30)};
        if (G.iv[4] >= 1)
            for (auto& f : fires) {
                if (frand() < 0.6f) spawnPart(f + V3(frange(-0.8f, 0.8f), 0.3f, frange(-0.8f, 0.8f)), V3(0, 2.5f, 0), 0.6f, 0.5f, RGBc(255, 130, 40), RM_ADD, -1, 0.4f);
                if (dist2D(G.P.pos, f) < 1.3f && fabsf(G.P.pos.y - f.y) < 1) { G.P.hp -= 30 * dt; G.P.sinceHit = 0; if (G.P.hp <= 0) { G.P.hp = 0; G.P.dead = true; } }
            }
        int h = G.iv[2];
        g_amb.heli = 1.0f / (1.0f + len(G.veh[h].pos - G.P.pos) * 0.05f);
        allyScript("Седой", V3(-2, 0, 37)); allyScript("Док", V3(2, 0, 36)); allyScript("Сова", V3(0, 0, 34));
        // из трюма метка ведёт сначала к трапу, на палубе — к носу
        { bool inHold = G.P.pos.y < -1.5f; bool onStairs = inHold && fabsf(G.P.pos.x) < 1.5f && G.P.pos.z < 20.5f && G.P.pos.z > 11.5f && (G.P.pos.z > 18.5f || G.P.pos.y > -4.9f);
          V3 w = inHold ? (onStairs ? V3(0, -0.3f, 11.6f) : V3(0, -5, 19.8f)) : V3(0, 0, 39); int st = inHold ? 1 : 2; if (G.iv[6] != st) { G.iv[6] = st; setObjective(inHold ? "К трапу — наверх, на палубу" : "Бегом к носу судна — вертолёт ждёт", w, true); } else G.wp = w; }
        if (G.P.pos.z > 38.3f && G.P.pos.y > -0.6f && G.storyNext != -2)
            showStory({"Прыжок! Седой ловит Кота за разгрузку.", "Судно Синдиката уходит под воду.", "Документы — у «Грома»."}, 4.5f, -2, true, RGBc(8, 10, 16));
        if (G.timer <= 0) missionFail("Судно затонуло.");
    } break;
    }
}

// ------------------------------------------------------------ М3 СТАРЫЙ РЫНОК
static const uint32_t kPastel[6] = {0xFFD8C49Au, 0xFFE8E2D4u, 0xFFC97C5Du, 0xFF8FB3C9u, 0xFFE0B870u, 0xFFB9A88Cu};
static void buildM3() {
    setEnv(V3(0.5f, 0.75f, -0.3f), 0.55f, 0.55f, RGBc(200, 205, 212), 50, 180, RGBc(90, 140, 210), RGBc(200, 210, 225), 190);
    setGround(RGBc(172, 160, 138), RGBc(150, 140, 120), 0);
    bounds(-34, -2, 48, 112);
    g_enemyUni = U_MILITIA;
    float zs[6] = {0, 12, 24, 36, 48, 56};
    for (int i = 0; i < 5; i++) {
        float h1 = 6.0f + (i % 3) * 1.5f, h2 = 7.0f + ((i + 1) % 3) * 1.5f;
        house(-16, zs[i], -4.2f, zs[i + 1], h1, kPastel[i % 6]);
        house(4.2f, zs[i], 16, zs[i + 1], h2, kPastel[(i + 3) % 6]);
    }
    float bw[2] = {6, 30}, be[2] = {18, 42};
    for (float zc : bw) { addBox(-4.2f, 3.0f, zc - 2, -2.8f, 3.3f, zc + 2, RGBc(200, 190, 170)); addBox(-2.9f, 3.3f, zc - 2, -2.8f, 4.3f, zc + 2, RGBc(40, 40, 40), MT_METAL); }
    for (float zc : be) { addBox(2.8f, 3.0f, zc - 2, 4.2f, 3.3f, zc + 2, RGBc(200, 190, 170)); addBox(2.8f, 3.3f, zc - 2, 2.9f, 4.3f, zc + 2, RGBc(40, 40, 40), MT_METAL); }
    // улица: асфальт с разметкой, бордюры, плиточные тротуары; рынок и переулок — брусчатка
    surface(-2.9f, -2, 2.9f, 56, 0.03f, RGBc(92, 92, 94), TX_ROAD);
    for (int sd = -1; sd <= 1; sd += 2) {
        float a = sd * 2.9f, b = sd * 4.2f;
        surface(std::min(a, b), -2, std::max(a, b), 56, 0.13f, RGBc(176, 170, 160), TX_TILES);
        surface(std::min(a, a + sd * 0.16f), -2, std::max(a, a + sd * 0.16f), 56, 0.15f, RGBc(150, 148, 144), TX_CONCRETE);
    }
    surface(-22, 56, 22, 92, 0.025f, RGBc(150, 138, 120), TX_PAVING);
    surface(-34, 102, 34, 111, 0.025f, RGBc(140, 132, 120), TX_PAVING);
    carProp(-2, 18, true, RGBc(150, 40, 35)); sandbags(-3.8f, 30, 1.0f, 30.8f); carProp(2.2f, 40, true, RGBc(45, 42, 40));
    crate(2.8f, 10, 0.5f, 1.0f); crate(-3, 46, 0.5f, 1.0f); barrel(3.4f, 25, RGBc(40, 90, 140));
    house(-34, 56, -22, 92, 8, kPastel[1]); house(22, 56, 34, 70, 7, kPastel[2]); house(22, 76, 34, 92, 9, kPastel[3]);
    uint32_t hc = kPastel[4];
    wallX(92, -12, -2, 6, 0.3f, hc, {-7}, {-10, -4}); wallX(102, -12, -2, 6, 0.3f, hc, {-7}, {});
    wallZ(-12, 92, 102, 6, 0.3f, hc); wallZ(-2, 92, 102, 6, 0.3f, hc); addBox(-12, 6, 92, -2, 6.3f, 102, shadeC(hc, 0.8f));
    crate(-9, 97, 0.5f, 0.9f);
    house(-1.7f, 92, 3.7f, 102, 9, kPastel[0]);
    wallX(92, 4, 14, 6, 0.3f, hc, {9}, {6, 12}); wallX(102, 4, 14, 6, 0.3f, hc, {9}, {});
    wallZ(4, 92, 102, 6, 0.3f, hc); wallZ(14, 92, 102, 6, 0.3f, hc); addBox(4, 6, 92, 14, 6.3f, 102, shadeC(hc, 0.8f));
    crate(11, 98, 0.5f, 0.9f);
    house(-34, 92, -12.2f, 102, 8, kPastel[5]); house(14.2f, 92, 34, 102, 8, kPastel[2]);
    house(-34, 111, 34, 120, 10, kPastel[3]);
    crate(-14, 106, 0.6f, 1.1f); crate(16, 107, 0.6f, 1.1f); carProp(2, 107, false, RGBc(200, 200, 190));
    uint32_t cy = RGBc(205, 190, 160);
    wallX(64, 34, 46, 4, 0.4f, cy); wallX(84, 34, 46, 4, 0.4f, cy); wallZ(46, 64, 84, 4, 0.4f, cy);
    wallZ(34, 64, 70, 4, 0.4f, cy); wallZ(34, 76, 84, 4, 0.4f, cy);
    crate(40, 70, 0.6f, 1.1f); crate(38, 79, 0.6f, 1.1f); barrel(44, 67, RGBc(140, 50, 40));
    uint32_t cloth[4] = {RGBc(190, 50, 45), RGBc(40, 110, 160), RGBc(220, 180, 60), RGBc(60, 140, 80)};
    int ci = 0;
    for (float z : {64.0f, 72.0f, 80.0f})
        for (float x : {-14.0f, -6.0f, 6.0f, 14.0f}) {
            addBox(x - 1.3f, 0, z - 0.05f, x + 1.3f, 1.0f, z + 0.05f, RGBc(130, 95, 60), MT_WOOD, true);
            addBox(x - 1.5f, 2.2f, z - 1.2f, x + 1.5f, 2.3f, z + 0.2f, cloth[ci++ & 3], MT_CLOTH);
            addDeco(x - 1.45f, 0, z - 1.15f, x - 1.35f, 2.2f, z - 1.05f, RGBc(90, 70, 50)); addDeco(x + 1.35f, 0, z - 1.15f, x + 1.45f, 2.2f, z - 1.05f, RGBc(90, 70, 50));
        }
    addBox(-1.5f, 0, 72.5f, 1.5f, 0.8f, 75.5f, RGBc(185, 180, 170), MT_CONCRETE, true);
    addDeco(-0.3f, 0.8f, 73.7f, 0.3f, 2.0f, 74.3f, RGBc(185, 180, 170));
    for (int k = 0; k < 3; k++) { float x = -18 + k * 18.0f; addDeco(x - 0.15f, 0, 59.6f, x + 0.15f, 5, 59.9f, RGBc(110, 85, 60)); addDeco(x - 1.4f, 5, 58.4f, x + 1.4f, 5.4f, 61.1f, RGBc(60, 120, 50)); }
    spawnAlly(V3(-2, 0, 0), "Седой", U_GROM, 0); spawnAlly(V3(2, 0, 0), "Док", U_DOC, 1); spawnAlly(V3(0, 0, -2), "Сова", U_GROM, 2);
    giveWeapon(0, W_AR, 210); giveWeapon(1, W_PISTOL, 60);
    G.P.nade[0] = 3; G.P.nade[1] = 2; G.P.nade[2] = 2;
    g_amb.wind = 0.1f; g_amb.echoAmt = 0.5f;
}
static void stageM3(int s) {
    switch (s) {
    case 0: {
        say("Майор Грин", "Связной — где-то в квартале рынка. Нужен живым.", true);
        say("Седой", "Двигаемся по улице. Смотрите балконы.");
        setObjective("Пробейтесь к рыночной площади", V3(0, 0, 58), true);
        V3 bal[4] = {V3(-3.6f, 3.3f, 6), V3(3.6f, 3.3f, 18), V3(-3.6f, 3.3f, 30), V3(3.6f, 3.3f, 42)};
        for (auto& p : bal) { int e = spawnEnemy(p, U_MILITIA, 0, false, 0); G.act[e].stationary = true; G.act[e].yaw = PI_F; }
        V3 st[4] = {V3(-1.5f, 0, 21.5f), V3(-2.5f, 0, 31.5f), V3(2.4f, 0, 43.5f), V3(-3, 0, 48)};
        for (int i = 0; i < 4; i++) spawnEnemy(st[i], U_MILITIA, i & 1, false, 0);
    } break;
    case 1: {
        checkpoint();
        say("Сова", "Площадь. Вижу движение у прилавков.");
        setObjective("Зачистите площадь и найдите связного", V3(0, 0, 74), true);
        V3 p[8] = {V3(-14, 0, 65), V3(-6, 0, 65.5f), V3(6, 0, 73), V3(14, 0, 73), V3(-6, 0, 81), V3(6, 0, 81), V3(-15, 0, 86), V3(15, 0, 87)};
        for (int i = 0; i < 8; i++) spawnEnemy(p[i], U_MILITIA, i % 3 == 2 ? 1 : 0, false, 1);
        Actor c; c.kind = AK_CONTACT; c.team = 1; c.uni = U_CIV; c.pos = V3(8, 0, 89); c.hp = 60; c.maxhp = 60; c.name = "Связной"; c.yaw = PI_F;
        c.path = {V3(16, 0, 82), V3(24, 0, 73), V3(33, 0, 73), V3(40, 0, 74), V3(42, 0, 81)};
        G.act.push_back(c); G.iv[5] = (int)G.act.size() - 1;
        G.iv[0] = 0;
    } break;
    case 2: {
        checkpoint();
        setObjective("Догоните связного. Брать живым!", V3(42, 0, 81), true);
        V3 g[3] = {V3(38, 0, 66), V3(44, 0, 71), V3(37, 0, 82)};
        for (int i = 0; i < 3; i++) spawnEnemy(g[i], U_MILITIA, 0, true, 2);
        if (G.iv[5] <= 0 || G.iv[5] >= (int)G.act.size() || G.act[G.iv[5]].kind != AK_CONTACT) {
            Actor c; c.kind = AK_CONTACT; c.team = 1; c.uni = U_CIV; c.pos = V3(26, 0, 73); c.hp = 60; c.maxhp = 60; c.name = "Связной";
            c.path = {V3(33, 0, 73), V3(40, 0, 74), V3(42, 0, 81)};
            G.act.push_back(c); G.iv[5] = (int)G.act.size() - 1;
        }
        Actor& c = G.act[G.iv[5]];
        if (c.state != AS_FLEE) { c.state = AS_FLEE; c.pathI = 0; }
    } break;
    case 3: {
        std::vector<std::string> L = {"Связной захвачен.", "Седой: — Где ракеты?", "Связной: — Я только курьер! Документы, деньги...",
                                      "Седой: — Где. Ракеты.", "Связной: — Старая электростанция в горах. Ущелье Тарса.", "Там стартовая площадка. Больше не знаю!"};
        showStory(L, 9.0f, 4, true, RGBc(20, 18, 16));
    } break;
    case 4: {
        std::vector<std::string> L = {"ПЕРЕХВАТ. Радио Каралии.", "Варгас: «Граждане! Чужие солдаты топчут нашу землю.",
                                      "Они пришли с оружием в наш дом. Пусть знают:", "близок день возмездия. Каждый из них его увидит.»"};
        showStory(L, 8.0f, -2, true, RGBc(40, 12, 10));
    } break;
    }
}
static void updateM3(float dt) {
    (void)dt;
    if (G.stage >= 1 && G.stage <= 2) {
        Actor& c = G.act[G.iv[5]];
        if (!c.alive) { missionFail("Связной убит. Он был нужен живым."); return; }
    }
    switch (G.stage) {
    case 0: if ((G.P.pos.z > 53 || countAlive(0) == 0) && G.stageT > 2) setStage(1); break;
    case 1: {
        int alive = countAlive(1);
        if (G.iv[0] == 0 && (alive <= 3 || G.P.pos.z > 84)) {
            G.iv[0] = 1;
            say("Док", "Они отходят через дома!");
            say("Седой", "Связной уходит на восток! За ним! Живым брать!");
            int k = 0;
            for (auto& a : G.act) {
                if (!a.alive || a.tag != 1 || a.kind != AK_SOLDIER) continue;
                if (k++ & 1) a.path = {V3(-7, 0, 90), V3(-7, 0, 97), V3(-7, 0, 104), V3(-12, 0, 107)};
                else a.path = {V3(9, 0, 90), V3(9, 0, 97), V3(9, 0, 104), V3(14, 0, 107)};
                a.pathI = 0; a.retreat = true;
                if (a.cover >= 0) G.cover[a.cover].occ = -1;
                a.cover = -1;
            }
            Actor& c = G.act[G.iv[5]]; c.state = AS_FLEE; c.pathI = 0;
            setStage(2);
        }
    } break;
    case 2: {
        Actor& c = G.act[G.iv[5]];
        G.wp = c.pos;
        if (dist2D(G.P.pos, c.pos) < 2.4f && c.pathI >= (int)c.path.size()) { c.invuln = true; c.state = AS_IDLE; setStage(3); }
    } break;
    }
}

// ------------------------------------------------------------ М4 ТЕНЬ
static float g_milUp = 0, g_milSide = 0;
static void computeCorrection(V3 eye, V3 tgt) {
    V3 dir = norm(tgt - eye);
    V3 p = eye, v = norm(dir + V3(0, 0.00068f, 0)) * 850.0f;
    float h = 1.0f / 120.0f;
    for (int i = 0; i < 1000 && p.z < tgt.z; i++) { p += v * h; v.y -= 9.81f * h; v.x += g_wind * 0.6f * h; v *= (1.0f - 0.07f * h); }
    float dist = len(tgt - eye);
    V3 line = eye + dir * ((p.z - eye.z) / std::max(0.01f, dir.z));
    g_milUp = (line.y - p.y) / dist * 1000.0f;
    g_milSide = (line.x - p.x) / dist * 1000.0f;
}
static void buildM4() {
    setEnv(V3(-0.6f, 0.35f, 0.5f), 0.42f, 0.35f, RGBc(118, 118, 112), 150, 1900, RGBc(80, 85, 95), RGBc(150, 140, 125), 1500);
    setGround(RGBc(92, 98, 68), RGBc(80, 86, 60), 0);
    bounds(-45, -10, 45, 160);
    G.stealth = true; g_wind = -3.0f; g_enemyUni = U_SYND;
    addDeco(10, 0, -10, 14, 0.02f, 160, RGBc(70, 70, 68));
    uint32_t rc = RGBc(140, 130, 115), rust = RGBc(110, 70, 45);
    wallX(55, -20, -8, 3, 0.3f, rc, {-14}, {-10}); wallZ(-20, 55, 65, 3, 0.3f, rc, {}, {60}); wallX(65, -20, -15, 2, 0.3f, rc);
    wallZ(-8, 55, 60, 2.2f, 0.3f, rc);
    wallX(70, -44, -37, 2.5f, 0.3f, rc, {}, {-40.5f}); wallZ(-44, 70, 78, 2.5f, 0.3f, rc); wallZ(-37, 70, 75, 1.6f, 0.3f, rc);
    house(18, 60, 26, 70, 5, rc); house(22, 96, 30, 104, 4, rc);
    carProp(12, 30, true, rust); carProp(-2, 92, false, rust); carProp(30, 120, true, rust);
    for (int k = 0; k < 10; k++) addBox(-36, 0, k * 15.0f, -35.8f, 1.2f, k * 15.0f + 12, RGBc(100, 85, 65), MT_WOOD);
    for (int k = 0; k < 14; k++) {
        float x = (k % 2 ? 30.0f : -40.0f) + (k * 7 % 5), z = k * 11.0f + 3;
        addBox(x - 0.25f, 0, z - 0.25f, x + 0.25f, 4, z + 0.25f, RGBc(70, 55, 40), MT_WOOD);
        addDeco(x - 1.8f, 3.5f, z - 1.8f, x + 1.8f, 6, z + 1.8f, RGBc(55, 70, 40));
    }
    uint32_t gc = RGBc(110, 120, 70);
    grassPatch(-30, 4, 10, 42, 230, gc); grassPatch(-32, 44, -2, 53, 70, gc); grassPatch(-35, 53, -23, 82, 110, gc); grassPatch(-25, 82, 15, 132, 300, gc); grassPatch(-12, 131, 12, 143, 80, gc);
    addBox(-3, 0, 144, 3, 0.35f, 152, RGBc(100, 90, 70)); addBox(-2.5f, 0.35f, 146, 2.5f, 0.7f, 152, RGBc(100, 90, 70));
    // дальняя площадка сделки
    house(-25, 1060, -10, 1072, 6, RGBc(120, 115, 105)); house(8, 1058, 24, 1070, 7, RGBc(110, 105, 100));
    carProp(-4, 1046, false, RGBc(20, 20, 22)); carProp(6, 1047, false, RGBc(210, 210, 205));
    G.iv[10] = spawnVip(V3(0, 0, 1050), U_RAVEN, PI_F);
    spawnVip(V3(1.3f, 0, 1049.2f), U_MILITIA, 0.2f); spawnVip(V3(-6, 0, 1048), U_SYND, PI_F); spawnVip(V3(7, 0, 1052), U_SYND, PI_F);
    // патрули
    int e = spawnEnemy(V3(12, 0, 8), U_SYND, 0, false, 0); G.act[e].path = {V3(12, 0, 8), V3(12, 0, 46)}; G.act[e].loopPath = true; G.act[e].yaw = 0;
    e = spawnEnemy(V3(12.8f, 0, 5), U_SYND, 0, false, 0); G.act[e].path = {V3(12.8f, 0, 5), V3(12.8f, 0, 43)}; G.act[e].loopPath = true;
    e = spawnEnemy(V3(-8, 0, 60), U_SYND, 0, false, 1); G.act[e].path = {V3(-8, 0, 60), V3(0, 0, 70), V3(-6, 0, 78), V3(-8, 0, 60)}; G.act[e].loopPath = true;
    Actor dog; dog.kind = AK_DOG; dog.team = 1; dog.hp = 45; dog.maxhp = 45; dog.pos = V3(-7, 0, 59); dog.handler = e; dog.tag = 1; dog.speed = 7;
    G.act.push_back(dog);
    e = spawnEnemy(V3(20, 0, 105), U_SYND, 0, false, 2); G.act[e].yaw = -PI_F * 0.5f;
    e = spawnEnemy(V3(19, 0, 113), U_SYND, 0, false, 2); G.act[e].yaw = -PI_F * 0.6f;
    e = spawnEnemy(V3(26, 0, 88), U_SYND, 0, false, 2); G.act[e].path = {V3(26, 0, 88), V3(26, 0, 124), V3(30, 0, 126), V3(26, 0, 88)}; G.act[e].loopPath = true;
    dog.pos = V3(27, 0, 87); dog.handler = e; dog.tag = 2; G.act.push_back(dog);
    int sv = spawnAlly(V3(2, 0, 2), "Сова", U_SOVA, 0); G.act[sv].scripted = true; G.act[sv].target = V3(2, 0, 2); G.act[sv].weapon = W_SNIPER;
    giveWeapon(0, W_SNIPER, 25); giveWeapon(1, W_PISTOL, 45); G.P.w[1].silenced = true;
    G.P.nade[0] = 1; G.P.nade[1] = 0; G.P.nade[2] = 2;
    G.P.ghillie = true; G.P.stance = 2; G.P.eyeH = 0.35f;
    G.timerOn = true; G.timer = 480; G.timerLabel = "До сделки"; G.iv[3] = 0;
    g_amb.wind = 0.6f; g_amb.echoAmt = 0.4f;
}
static void stageM4(int s) {
    int sv = findAlly("Сова");
    switch (s) {
    case 0:
        showStory({"ГОД НАЗАД.", "Пригород Велеса."}, 3.5f, -1, false, 0);
        say("Сова", "Тихо, Кот. Маскхалат — не шапка-невидимка.");
        say("Сова", "Ползком через поле. В траве лёжа нас не видно.");
        setObjective("Ползком к старым руинам", V3(-5, 0, 42), true);
        allyScript("Сова", V3(-7, 0, 41));
        break;
    case 1:
        checkpoint();
        say("Сова", "Патруль с собакой у руин. Ждём... Пусть пройдут.");
        say("Сова", "Собака учует на ходу. Ползи медленно, не вставай.");
        setObjective("Уходите в траву на запад", V3(-29, 0, 56), true);
        allyScript("Сова", V3(-31, 0, 55)); G.iv[13] = 0;
        break;
    case 2: {
        checkpoint();
        int h = spawnVeh(VK_HELI, V3(0, 28, 200), PI_F, 1);
        G.veh[h].path = {V3(-2, 22, 130), V3(-8, 20, 100), V3(-5, 22, 80), V3(10, 30, 20), V3(60, 40, -100)};
        G.veh[h].speed = 6; G.veh[h].spot = true; G.veh[h].burn = 0; G.iv[2] = h;
        say("Сова", "Вертушка! Лежать! Не двигайся!");
        setObjective("Замрите в траве, пока вертолёт не пройдёт", V3(0, 0, 128), true);
        G.fl[0] = 0;
    } break;
    case 3:
        checkpoint();
        say("Сова", "Ушёл. Последний рывок — к насыпи. Позиция там.");
        setObjective("Выйдите на позицию", V3(0, 0.7f, 149), true);
        allyScript("Сова", V3(2, 0.7f, 149));
        break;
    case 4: {
        g_scopeHigh = true;
        Actor& r = G.act[G.iv[10]];
        computeCorrection(G.P.pos + V3(0, 0.35f, 0), actorChest(r));
        say("Сова", "Позиция. Дистанция — девятьсот. Ветер справа, три метра.");
        static std::string corr;
        corr = fmt("Поправка: %.1f деления вверх, %.1f вправо. Задержи дыхание.", g_milUp, g_milSide);
        say("Сова", corr.c_str());
        say("Сова", "Цель — в чёрном пальто. Это Ворон. Работай.");
        setObjective("Поразите Ворона (900 м)", r.pos, true);
        G.timerOn = true; G.timer = 75; G.timerLabel = "Сделка";
        G.iv[6] = G.shots;
        if (G.P.w[0].mag < 3) G.P.w[0].mag = 5;
        if (G.P.slot != 0) { G.P.pendingSlot = 0; G.P.switchT = 0.6f; }
    } break;
    case 5: {
        g_scopeHigh = false;
        G.stealth = false; G.detected = true;
        G.act[G.iv[10]].hidden = true;
        if (sv >= 0) { Actor& a = G.act[sv]; a.state = AS_DOWN; a.scripted = true; a.target = a.pos; }
        say("Сова", "Попадание! Он упал... Его утаскивают в машину.");
        say("Сова", "Нас засекли! А-а... Я ранена. Нога...");
        setObjective("Вынесите Сову", sv >= 0 ? G.act[sv].pos : V3(), true);
        V3 p[6] = {V3(-10, 0, 158), V3(10, 0, 157), V3(0, 0, 159), V3(40, 0, 120), V3(40, 0, 110), V3(-40, 0, 135)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, i & 1, true, 5);
        Actor dog; dog.kind = AK_DOG; dog.team = 1; dog.hp = 45; dog.maxhp = 45; dog.alerted = true; dog.tag = 5; dog.speed = 7;
        dog.pos = V3(30, 0, 140); G.act.push_back(dog); dog.pos = V3(-30, 0, 150); G.act.push_back(dog);
        for (auto& a : G.act) if (a.alive && a.team == 1 && (a.kind == AK_SOLDIER || a.kind == AK_DOG)) { a.alerted = true; a.lastKnown = G.P.pos; a.lastSeen = 0; }
        G.timerOn = false; G.iv[7] = 0;
    } break;
    case 6: {
        checkpoint();
        say("Сова", "Клади меня тут... Я прикрою. Пистолет при мне.");
        say("Майор Грин", "«Тень», вертушка будет через минуту. Держитесь.", true);
        setObjective("Продержитесь до эвакуации", V3(-20, 0, 25), true);
        G.timerOn = true; G.timer = 70; G.timerLabel = "Эвакуация"; G.fl[1] = 0; G.iv[8] = 0;
        if (sv >= 0) { Actor& a = G.act[sv]; a.state = AS_IDLE; a.scripted = true; a.pos = G.P.pos + V3(1, 0, 0); a.target = a.pos; a.weapon = W_PISTOL; a.stance = 1; a.hidden = false; }
        G.P.carry = -1;
    } break;
    }
}
static void updateM4(float dt) {
    int sv = findAlly("Сова");
    if (G.stage <= 3 && G.detected) { missionFail("Нас обнаружили. Операция сорвана."); return; }
    if (G.stage <= 4 && G.timerOn) { G.timer -= dt; if (G.timer <= 0) { missionFail(G.stage == 4 ? "Ворон уехал. Окно закрыто." : "Не успели. Сделка сорвалась без нас."); return; } }
    switch (G.stage) {
    case 0: if (dist2D(G.P.pos, V3(-5, 0, 42)) < 5) setStage(1); break;
    case 1:
        if (G.iv[13] == 0 && dist2D(G.P.pos, V3(-29, 0, 56)) < 4) {
            G.iv[13] = 1; say("Сова", "Хорошо. Вдоль руин, по траве. На север.");
            setObjective("Ползком по траве на север", V3(-24, 0, 84), true); allyScript("Сова", V3(-26, 0, 85));
        }
        if (G.iv[13] == 1 && dist2D(G.P.pos, V3(-24, 0, 84)) < 5) setStage(2);
        break;
    case 2: {
        int h = G.iv[2];
        Vehicle& v = G.veh[h];
        g_amb.heli = 1.0f / (1.0f + len(v.pos - G.P.pos) * 0.03f); g_amb.heliRate = 5.2f;
        if (v.pi >= 4) { G.fl[0] += dt; }
        if (v.pi >= 4 && G.fl[0] > 1 && G.iv[9] == 0) { G.iv[9] = 1; say("Сова", "Пронесло. Двигаемся."); allyScript("Сова", V3(-1, 0, 127)); }
        if (G.iv[9] == 1 && dist2D(G.P.pos, V3(0, 0, 128)) < 6) { v.active = false; setStage(3); }
    } break;
    case 3: g_amb.heli = 0; if (dist2D(G.P.pos, V3(0, 0, 149)) < 2.5f && G.P.pos.y > 0.5f) setStage(4); break;
    case 4: {
        Actor& r = G.act[G.iv[10]];
        r.yaw = PI_F + sinf(G.time * 0.4f) * 0.5f;
        if (r.hp < r.maxhp) { G.P.adsT = 0; setStage(5); break; }
        if (G.shots - G.iv[6] >= 4 && G.bul.empty()) missionFail("Промах. Ворон ушёл.");
        else if (G.bul.empty() && G.shots > G.iv[6] && G.iv[11] != G.shots) { G.iv[11] = G.shots; if (G.shots - G.iv[6] < 4) say("Сова", "Мимо. Спокойно. Ещё раз.", false, true); }
    } break;
    case 5: {
        if (sv < 0) break;
        Actor& a = G.act[sv];
        if (G.P.carry < 0) {
            G.wp = a.pos;
            float d = dist2D(G.P.pos, a.pos);
            G.useHint = d < 2.0f ? "Поднять Сову" : "";
            if (d < 2.0f && (G.P.useT > 0.5f || IN.use)) {
                G.P.carry = sv; G.iv[22] = 1; a.state = AS_CARRIED; G.useHint = "";
                say("Сова", "Держу пистолет... Неси.");
                setObjective("Донесите Сову до точки эвакуации", V3(-20, 0, 25), true);
            }
        } else if (dist2D(G.P.pos, V3(-20, 0, 25)) < 6) setStage(6);
        G.iv[7]++;
    } break;
    case 6: {
        G.timer -= dt; G.fl[1] += dt;
        if (G.fl[1] > G.iv[8] * 17.0f + 6 && G.iv[8] < 4) {
            G.iv[8]++;
            for (int k = 0; k < 3; k++) spawnEnemy(V3(frange(-30, 10), 0, frange(70, 80)), U_SYND, k & 1, true, 6);
        }
        if (G.iv[12] == 0 && G.timer < 18) {
            G.iv[12] = 1;
            int h = spawnVeh(VK_HELI, V3(-80, 30, -60), 0.8f, 0); G.veh[h].path = {V3(-40, 15, 0), V3(-22, 3, 22)}; G.veh[h].speed = 16; G.iv[2] = h;
        }
        if (G.iv[12] == 1) { Vehicle& v = G.veh[G.iv[2]]; g_amb.heli = 1.0f / (1.0f + len(v.pos - G.P.pos) * 0.04f); }
        if (G.timer <= 0) {
            G.timer = 0;
            G.useHint = "";
            if (dist2D(G.P.pos, V3(-21, 0, 23)) < 7 && G.storyNext != -2)
                showStory({"Эвакуация.", "Ворон выжил. Он видел, кто стрелял.", "Он запомнил лицо Кота."}, 5.0f, -2, true, RGBc(16, 16, 18));
            else G.wp = V3(-21, 0, 23);
        }
    } break;
    }
}

// ------------------------------------------------------------ М5 РУБЕЖ
static void buildM5() {
    setEnv(V3(0.3f, 0.6f, -0.5f), 0.48f, 0.45f, RGBc(150, 150, 156), 40, 170, RGBc(110, 116, 130), RGBc(172, 172, 178), 175);
    setGround(RGBc(88, 88, 86), RGBc(98, 98, 94), 0);
    bounds(-60, -30, 60, 95);
    uint32_t bc = RGBc(170, 170, 165);
    wallX(0, -15, 15, 4, 0.4f, bc, {0}, {-10, -5, 5, 10});
    wallX(20, -15, 15, 4, 0.4f, bc, {0}, {-10, -5, 5, 10});
    wallZ(-15, 0, 20, 4, 0.4f, bc, {}, {5, 15});
    wallZ(15, 0, 20, 4, 0.4f, bc, {10}, {4, 16});
    addBox(-15.2f, 4, -0.2f, 15.2f, 4.4f, 20.2f, shadeC(bc, 0.8f));
    addDeco(-0.15f, 4.4f, 9.85f, 0.15f, 16, 10.15f, RGBc(160, 160, 160)); lamp(0, 16.2f, 10, RGBc(255, 50, 40));
    addBox(-1, 0, 9.4f, 1, 1.0f, 10.6f, RGBc(60, 64, 70), MT_METAL, true);
    addDeco(-0.55f, 1.0f, 10.45f, 0.55f, 1.32f, 10.55f, RGBc(60, 140, 200), true);
    uint32_t wc = RGBc(120, 95, 70);
    addBox(-10, 0, 15, -7, 0.85f, 16, wc, MT_WOOD, true); addBox(7, 0, 15, 10, 0.85f, 16, wc, MT_WOOD, true);
    addBox(-10, 0, 5, -7, 0.85f, 6, wc, MT_WOOD, true); addBox(7, 0, 5, 10, 0.85f, 6, wc, MT_WOOD, true);
    addBox(-14.5f, 0, 8, -13.5f, 2.2f, 12, RGBc(40, 42, 46), MT_METAL);
    addBox(3, 0, 2, 4.2f, 0.7f, 3, RGBc(70, 85, 55), MT_WOOD, true); addDeco(3.3f, 0.7f, 2.3f, 3.9f, 0.72f, 2.7f, RGBc(230, 200, 60), true);
    for (int k = -1; k <= 1; k += 2) for (int j = 0; j < 2; j++) lamp(k * 7.0f, 3.9f, 5 + j * 10.0f, RGBc(230, 230, 210));
    uint32_t st = RGBc(150, 148, 140);
    addBox(-12, 0, 30, -6, 0.8f, 31, st, MT_CONCRETE, true); addBox(6, 0, 30, 12, 0.8f, 31, st, MT_CONCRETE, true);
    addBox(-3, 0, 40, 3, 0.8f, 41, st, MT_CONCRETE, true); addBox(-14, 0, 48, -8, 0.8f, 49, st, MT_CONCRETE, true); addBox(8, 0, 48, 14, 0.8f, 49, st, MT_CONCRETE, true);
    carProp(-18, 38, true, RGBc(140, 30, 30)); carProp(18, 34, true, RGBc(50, 60, 90)); carProp(-6, 55, false, RGBc(200, 200, 190));
    addDeco(22, 0, 44, 27, 2.6f, 44.2f, RGBc(90, 140, 160)); addDeco(22, 2.6f, 43, 27, 2.8f, 45.2f, RGBc(60, 60, 60));
    addDeco(-60, 0, 60, 60, 0.02f, 68, RGBc(55, 55, 55));
    house(-50, 72, -20, 88, 12, kPastel[1]); house(-14, 74, 14, 90, 14, kPastel[5]); house(20, 72, 50, 88, 10, kPastel[3]);
    addBox(24, 0, -5, 25, 1.0f, 25, RGBc(60, 90, 50), MT_WOOD, true); addBox(-25, 0, -5, -24, 1.0f, 25, RGBc(60, 90, 50), MT_WOOD, true);
    carProp(-8, -12, false, RGBc(80, 80, 85)); carProp(8, -14, false, RGBc(160, 150, 120));
    int a = spawnAlly(V3(-5, 0, 18.4f), "Седой", U_GROM, 0); G.act[a].scripted = true; G.act[a].target = V3(-5, 0, 18.4f);
    a = spawnAlly(V3(5, 0, 18.4f), "Док", U_DOC, 1); G.act[a].scripted = true; G.act[a].target = V3(5, 0, 18.4f);
    a = spawnAlly(V3(0, 0, 11.5f), "Сова", U_GROM, 2); G.act[a].scripted = true; G.act[a].target = V3(0, 0, 11.5f);
    giveWeapon(0, W_AR, 300); giveWeapon(1, W_SHOTGUN, 40);
    G.P.nade[0] = 4; G.P.nade[1] = 2; G.P.nade[2] = 2;
    g_amb.wind = 0.2f; g_amb.echoAmt = 0.45f;
    G.progOn = true; G.progLabel = "Передача координат"; G.progress = 0;
}
static void m5Wave(int w) {
    switch (w) {
    case 0: for (int k = 0; k < 6; k++) spawnEnemy(V3(-17 + k * 1.3f, 0, 74 + (k & 1) * 2.0f), U_SYND, k % 3 == 2, true, 10); break;
    case 1: { int t = spawnVeh(VK_TRUCK, V3(-58, 0, 64), PI_F * 0.5f, 1); G.veh[t].path = {V3(-20, 0, 64), V3(-2, 0, 58), V3(0, 0, 50)}; G.veh[t].cargo = 6; G.veh[t].tag = 11; } break;
    case 2:
        for (int k = 0; k < 4; k++) spawnEnemy(V3(50, 0, 8 + k * 2.0f), U_SYND, k & 1, true, 12);
        for (int k = 0; k < 4; k++) spawnEnemy(V3(-50, 0, 8 + k * 2.0f), U_SYND, k & 1, true, 12);
        break;
    case 3: { int h = spawnVeh(VK_HELI, V3(120, 40, 120), -2.3f, 1); G.veh[h].path = {V3(40, 25, 60), V3(20, 6, 36)}; G.veh[h].cargo = 4; G.veh[h].orbitR = 38; G.veh[h].tag = 13; G.iv[2] = h; } break;
    case 4: {
        int t = spawnVeh(VK_TECH, V3(58, 0, 64), -PI_F * 0.5f, 1); G.veh[t].path = {V3(25, 0, 64), V3(16, 0, 52)}; G.veh[t].cargo = 2; G.veh[t].tag = 14;
        t = spawnVeh(VK_TRUCK, V3(-58, 0, 64), PI_F * 0.5f, 1); G.veh[t].path = {V3(-24, 0, 64), V3(-14, 0, 56)}; G.veh[t].cargo = 6; G.veh[t].tag = 14;
    } break;
    case 5: for (int k = 0; k < 8; k++) spawnEnemy(V3(-12 + k * 3.0f, 0, 76 + (k & 1) * 3.0f), U_SYND, k % 3 == 1, true, 15); break;
    case 6: { int t = spawnVeh(VK_TANK, V3(-60, 0, 64), PI_F * 0.5f, 1); G.veh[t].path = {V3(-24, 0, 64), V3(-6, 0, 60), V3(-2, 0, 57)}; G.veh[t].tag = 16; } break;
    }
}
static const float kM5Wave[7] = {3, 40, 78, 128, 158, 176, 196};
static void stageM5(int s) {
    switch (s) {
    case 0:
        say("Седой", "Занимаем телецентр. Сова — на передатчик. Все к окнам.");
        say("Сова", "Передача пошла. Нужно минуты четыре.");
        say("Док", "Четыре минуты. Успею рассказать анекдот про трёх снайперов.");
        setObjective("Удерживайте телецентр", V3(0, 0, 10), false);
        G.fl[1] = 0; G.iv[0] = 0; G.iv[1] = 0;
        break;
    case 1: {
        int d = findAlly("Док");
        if (d >= 0) { Actor& a = G.act[d]; a.state = AS_DOWN; a.scripted = true; a.pos = V3(0.5f, 0, 23); a.target = a.pos; }
        say("Сова", "Док ранен! Он у северного входа!");
        say("Седой", "Кот! Тащи Дока внутрь! Прикрываем!");
        setObjective("Затащите Дока в здание", V3(0.5f, 0, 23), true);
        G.fl[2] = 0;
    } break;
    case 2: {
        checkpoint();
        int d = findAlly("Док");
        if (d >= 0) { Actor& a = G.act[d]; a.state = AS_IDLE; a.scripted = true; a.pos = V3(-3, 0, 6); a.target = a.pos; a.stance = 1; a.hidden = false; }
        G.P.carry = -1; G.iv[22] = 0;
        say("Док", "Жить буду. Но танцевать — вряд ли.");
        setObjective("Удерживайте телецентр", V3(0, 0, 10), false);
        if (G.fl[1] < 110) { G.fl[1] = 110; G.progress = std::max(G.progress, 0.45f); G.iv[0] = 3; }
    } break;
    case 3:
        showStory({"Передача завершена.", "Седой: — Штаб принял координаты.", "Седой: — Но Грин передаёт: ракеты уже на стартовых позициях."}, 6.0f, -2, true, RGBc(14, 16, 22));
        break;
    }
}
static void updateM5(float dt) {
    if (G.stage >= 3) return;
    G.fl[1] += dt;
    while (G.iv[0] < 7 && G.fl[1] >= kM5Wave[G.iv[0]]) {
        m5Wave(G.iv[0] == 5 ? 6 : (G.iv[0] == 6 ? 5 : G.iv[0]));
        if (G.iv[0] == 5) { say("Сова", "Танк! С севера, по дороге!"); say("Седой", "Пули его не возьмут. Гранаты под гусеницы! В ящике есть ещё."); }
        if (G.iv[0] == 1) say("Сова", "Грузовик с севера!");
        if (G.iv[0] == 2) say("Седой", "Обходят с флангов! Восток и запад!");
        if (G.iv[0] == 3) say("Док", "Вертушка! Держитесь от окон!");
        if (G.iv[0] == 4) say("Сова", "Пикап с пулемётом! Гранатой его!");
        G.iv[0]++;
    }
    // боеприпасы в ящике
    if (dist2D(G.P.pos, V3(3.6f, 0, 2.5f)) < 1.8f) {
        if (G.fl[6] <= 0) { refillAmmo(); G.P.nade[0] = std::max(G.P.nade[0], 2); G.fl[6] = 10; playSnd(ST_CLICK, 0.4f, 0, 0, 0.1f, 0.05f); G.fl[21] = 2; }
    }
    G.fl[6] -= dt;
    bool threat = false;
    for (auto& a : G.act) if (a.alive && a.team == 1 && a.kind == AK_SOLDIER && dist2D(a.pos, V3(0, 0, 10)) < 5) threat = true;
    if (!threat) G.progress = std::min(1.0f, G.progress + dt / 240.0f);
    G.progLabel = threat ? "Противник у передатчика!" : "Передача координат";
    for (auto& v : G.veh) if (v.kind == VK_HELI && v.active) g_amb.heli = std::max(g_amb.heli * 0.98f, 1.0f / (1.0f + len(v.pos - G.P.pos) * 0.04f));
    if (G.stage == 0 && G.fl[1] > 100 && G.iv[1] == 0) { G.iv[1] = 1; allyScript("Док", V3(0.5f, 0, 22.5f)); say("Док", "Прикрою северный вход снаружи!"); }
    if (G.stage == 0 && G.fl[1] > 106) setStage(1);
    if (G.stage == 1) {
        int d = findAlly("Док");
        Actor& a = G.act[d];
        G.fl[2] += dt;
        if (G.P.carry < 0) {
            float dd = dist2D(G.P.pos, a.pos);
            G.useHint = dd < 2.0f ? "Тащить Дока" : "";
            if (dd < 2.0f && (G.P.useT > 0.5f || IN.use)) { G.P.carry = d; G.iv[22] = 2; a.state = AS_CARRIED; G.useHint = ""; setObjective("Затащите Дока в здание", V3(0, 0, 12), true); }
            if (G.fl[2] > 50) missionFail("Док погиб.");
        } else if (G.P.pos.z < 17 && G.P.pos.z > 1 && fabsf(G.P.pos.x) < 14) setStage(2);
    }
    if (G.progress >= 1 && G.stage < 3) setStage(3);
}

// ------------------------------------------------------------ М6 ПОСЛЕДНИЙ ОТСЧЁТ
static void buildM6() {
    setEnv(V3(0.7f, 0.25f, 0.6f), 0.45f, 0.6f, RGBc(196, 150, 122), 60, 230, RGBc(92, 112, 160), RGBc(238, 172, 124), 240);
    setGround(RGBc(112, 102, 86), RGBc(98, 90, 76), 0);
    bounds(-28, -10, 28, 262);
    uint32_t rock = RGBc(115, 100, 85), conc = RGBc(150, 148, 140), tun = RGBc(95, 95, 92), hall = RGBc(130, 125, 115);
    addBox(-40, 0, -10, -28, 22, 262, rock); addBox(28, 0, -10, 40, 22, 262, rock);
    mountain(-90, 100, 45, 70, RGBc(120, 105, 95)); mountain(95, 180, 55, 80, RGBc(110, 100, 92)); mountain(0, 340, 80, 95, RGBc(125, 112, 100));
    mountain(-70, 260, 40, 60, RGBc(118, 104, 94));
    for (auto& b : G.box) if (b.col == rock) b.tex = TX_ROCK;
    addDeco(-4, 0, -10, 4, 0.02f, 40, RGBc(60, 60, 60));
    sandbags(-3.5f, -2.2f, 3.5f, -1.6f); carProp(-7, -4, true, RGBc(70, 80, 60));
    { int tk = spawnVeh(VK_TANK, V3(11, 0, 3), 0.25f, 1); G.veh[tk].alive = false; G.veh[tk].arrived = true; G.veh[tk].solidAdded = true; G.veh[tk].orbitA = 0.9f; G.veh[tk].burn = 0;
      addBox(8.6f, 0, -0.6f, 13.4f, 2.0f, 6.6f, 0, MT_INVIS); G.box.back().mat = MT_METAL; }
    wallZ(-18, 0, 36, 3, 0.5f, conc); wallZ(18, 0, 36, 3, 0.5f, conc);
    uint32_t bc = RGBc(190, 185, 170);
    wallX(18, 6, 10, 2.8f, 0.2f, bc, {}, {8}); wallX(22, 6, 10, 2.8f, 0.2f, bc, {8}, {});
    wallZ(6, 18, 22, 2.8f, 0.2f, bc, {}, {20}); wallZ(10, 18, 22, 2.8f, 0.2f, bc);
    addBox(5.8f, 2.8f, 17.8f, 10.2f, 3.0f, 22.2f, RGBc(80, 80, 80));
    addBox(-4, 0.9f, 19.9f, 2, 1.0f, 20.1f, RGBc(220, 60, 50), MT_METAL);
    sandbags(-6, 14, -2, 14.8f); sandbags(2, 26, 6, 26.8f);
    addBox(-12, 0, 10, -10, 1.1f, 11, conc, MT_CONCRETE, true); addBox(10, 0, 28, 12, 1.1f, 29, conc, MT_CONCRETE, true); addBox(-14, 0, 24, -12, 1.1f, 25, conc, MT_CONCRETE, true);
    crate(13, 10, 0.6f, 1.2f); barrel(-9, 30, RGBc(60, 80, 50)); crate(-15, 16, 0.6f, 1.2f);
    // туннель
    addBox(-28, 0, 38, -5, 22, 40, rock); addBox(5, 0, 38, 28, 22, 40, rock); addBox(-5, 5, 38, 5, 22, 40, rock);
    addBox(-28, 0, 40, -6, 22, 101.7f, rock); addBox(6, 0, 40, 28, 22, 101.7f, rock);
    addBox(-6, 0, 40, -5, 6, 101.7f, tun); addBox(5, 0, 40, 6, 6, 101.7f, tun); addBox(-6, 5, 40, 6, 6, 101.7f, tun);
    for (float z = 44; z < 100; z += 8) lamp(0, 5, z, RGBc(255, 230, 170));
    crate(-3, 50, 0.6f, 1.2f); barrel(3, 56, RGBc(150, 40, 30)); addBox(1.5f, 0, 64, 4.5f, 1.6f, 67, RGBc(200, 160, 40), MT_METAL, true);
    crate(-3.5f, 72, 0.6f, 1.2f); sandbags(-2, 84, 2, 84.8f); barrel(-3.5f, 90, RGBc(150, 40, 30)); crate(3.2f, 79, 0.6f, 1.2f);
    // машзал
    addBox(-20, 0, 101.7f, -5, 12, 102.3f, hall); addBox(5, 0, 101.7f, 20, 12, 102.3f, hall); addBox(-5, 5, 101.7f, 5, 12, 102.3f, hall);
    addBox(-20.6f, 0, 102, -20, 12, 160, hall); addBox(20, 0, 102, 20.6f, 12, 160, hall);
    addBox(-28, 0, 101.7f, -20.6f, 22, 160, rock); addBox(20.6f, 0, 101.7f, 28, 22, 160, rock);
    addBox(-20.6f, 12, 102, 20.6f, 12.5f, 160, shadeC(hall, 0.7f));
    addBox(-20.6f, 0, 159.7f, -3, 12, 160.3f, hall); addBox(3, 0, 159.7f, 20.6f, 12, 160.3f, hall); addBox(-3, 4, 159.7f, 3, 12, 160.3f, hall);
    for (float z : {116.0f, 131.0f, 146.0f}) {
        addBox(-14, 0, z - 3, -6, 4.5f, z + 3, RGBc(70, 90, 100), MT_METAL); addBox(-13, 4.5f, z - 2, -7, 5.8f, z + 2, RGBc(80, 100, 110), MT_METAL);
        addBox(7, 0, z - 2, 11, 2.5f, z + 2, RGBc(150, 120, 50), MT_METAL);
        addBox(-3, 0, z + 4, 3, 0.9f, z + 4.6f, RGBc(90, 95, 100), MT_METAL, true);
    }
    addBox(15, 3.7f, 108, 19.6f, 4.0f, 156, RGBc(80, 80, 75), MT_METAL); addBox(15, 4, 108, 15.1f, 5, 156, RGBc(160, 150, 40), MT_METAL);
    for (float z = 110; z < 158; z += 10) lamp(0, 11.9f, z, RGBc(240, 240, 220));
    // стартовая площадка
    addDeco(-26, 0, 160.3f, 26, 0.02f, 236, RGBc(130, 128, 122));
    int t = spawnVeh(VK_TEL, V3(-12, 0, 195), 0.0f, 1); G.veh[t].arrived = true; G.veh[t].solidAdded = true;
    addBox(-13.3f, 0, 190.5f, -10.7f, 1.9f, 199.5f, 0, MT_INVIS); G.box.back().mat = MT_METAL;
    t = spawnVeh(VK_TEL, V3(12, 0, 207), 0.0f, 1); G.veh[t].arrived = true; G.veh[t].solidAdded = true;
    addBox(10.7f, 0, 202.5f, 13.3f, 1.9f, 211.5f, 0, MT_INVIS); G.box.back().mat = MT_METAL;
    container(-4, 180, false, RGBc(70, 90, 110)); container(6, 222, false, RGBc(140, 60, 40));
    sandbags(-8, 212, -4, 212.8f); sandbags(3, 186, 7, 186.8f);
    addBox(16, 0, 170, 22, 3, 176, RGBc(200, 200, 195), MT_METAL); crate(-18, 220, 0.7f, 1.3f); barrel(10, 230, RGBc(60, 80, 50)); barrel(-6, 198, RGBc(60, 80, 50));
    // диспетчерская
    uint32_t cr = RGBc(175, 170, 160);
    wallX(236, -12, 12, 4.5f, 0.4f, cr, {0}, {-8, -4, 4, 8}); wallX(254, -12, 12, 4.5f, 0.4f, cr, {}, {-6, 6});
    wallZ(-12, 236, 254, 4.5f, 0.4f, cr, {}, {245}); wallZ(12, 236, 254, 4.5f, 0.4f, cr, {245}, {});
    addBox(-12.2f, 4.5f, 235.8f, 12.2f, 4.9f, 254.2f, shadeC(cr, 0.7f));
    addBox(-10, 0, 251.5f, -2, 1.0f, 253, RGBc(70, 72, 78), MT_METAL, true); addBox(2, 0, 251.5f, 10, 1.0f, 253, RGBc(70, 72, 78), MT_METAL, true);
    addBox(-1.2f, 0, 250.4f, 1.2f, 1.1f, 252.4f, RGBc(80, 80, 90), MT_METAL, true);
    addDeco(-1.0f, 1.1f, 251.9f, 1.0f, 1.7f, 252.0f, RGBc(220, 50, 40), true);
    addBox(-7, 0, 243, -3, 0.9f, 244, RGBc(120, 95, 70), MT_WOOD, true); addBox(3, 0, 241, 7, 0.9f, 242, RGBc(120, 95, 70), MT_WOOD, true);
    lamp(-6, 4.4f, 245, RGBc(240, 240, 220)); lamp(6, 4.4f, 245, RGBc(240, 240, 220));
    spawnAlly(V3(-2, 0, 0), "Седой", U_GROM, 0); spawnAlly(V3(2, 0, 0), "Док", U_DOC, 1); spawnAlly(V3(0, 0, -2), "Сова", U_GROM, 2);
    for (auto& b : G.box) if (b.col == rock) b.tex = TX_ROCK;
    giveWeapon(0, W_AR, 270); giveWeapon(1, W_SHOTGUN, 40);
    G.P.nade[0] = 3; G.P.nade[1] = 2; G.P.nade[2] = 1;
    g_amb.wind = 0.35f; g_amb.echoAmt = 0.55f;
}
static void stageM6(int s) {
    switch (s) {
    case 0: {
        say("Майор Грин", "Электростанция в ущелье Тарса. Ракеты — на площадке за машзалом.", true);
        setObjective("Прорвитесь через КПП", V3(0, 0, 40), true);
        V3 p[7] = {V3(-4, 0, 15.6f), V3(4, 0, 27.6f), V3(-13, 0, 25.8f), V3(11, 0, 29.8f), V3(8, 0, 20), V3(13, 0, 11.4f), V3(-11, 0, 11.8f)};
        for (int i = 0; i < 7; i++) spawnEnemy(p[i], U_SYND, i % 3 == 2, false, 0);
        int j = spawnVeh(VK_JEEP, V3(0, 0, 30), 0, 1); G.veh[j].path = {V3(0, 0, 60), V3(0, 0, 100), V3(0, 0, 130), V3(0, 0, 159), V3(0, 0, 250)}; G.veh[j].speed = 9; G.veh[j].hp = 1e9f; G.iv[4] = j;
        G.iv[0] = 0;
    } break;
    case 1: {
        checkpoint();
        setObjective("Пройдите туннель", V3(0, 0, 104), true);
        say("Седой", "Туннель. Держимся стен, работаем парами.");
        V3 p[6] = {V3(-3, 0, 51.5f), V3(3, 0, 57.5f), V3(3, 0, 68), V3(-3.5f, 0, 73.6f), V3(0, 0, 86), V3(-3, 0, 92)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, i & 1, false, 1);
    } break;
    case 2: {
        checkpoint();
        setObjective("Пробейтесь через машинный зал", V3(0, 0, 160), true);
        say("Сова", "Машзал. Мостки справа — там стрелки.");
        V3 p[6] = {V3(-4, 0, 121.5f), V3(4, 0, 125), V3(0, 0, 136), V3(-4, 0, 141.4f), V3(4, 0, 151), V3(-3, 0, 153)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, i % 3 == 1, false, 2);
        int e = spawnEnemy(V3(17.5f, 4.0f, 125), U_SYND, 0, false, 2); G.act[e].stationary = true;
        e = spawnEnemy(V3(17.5f, 4.0f, 145), U_SYND, 0, false, 2); G.act[e].stationary = true;
    } break;
    case 3: {
        checkpoint();
        say("Майор Грин", "Пуск через три минуты! Пульт — в диспетчерской, северный край площадки!", true);
        setObjective("Прорвитесь к диспетчерской", V3(0, 0, 236), true);
        G.timerOn = true; G.timer = 180; G.timerLabel = "До пуска"; G.iv[3] = 0;
        V3 p[6] = {V3(-4, 0, 182.4f), V3(5, 0, 188), V3(-6, 0, 214), V3(7, 0, 224.6f), V3(-17, 0, 222), V3(11, 0, 232)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, i % 3 == 2, true, 3);
    } break;
    case 4: {
        if (G.timer < 75 || !G.timerOn) { G.timerOn = true; G.timer = std::max(G.timer, 75.0f); G.timerLabel = "До пуска"; }
        Actor r; r.kind = AK_SOLDIER; r.team = 1; r.uni = U_RAVEN; r.name = "Ворон"; r.pos = V3(0, 0, 248); r.hp = 520; r.maxhp = 520;
        r.alerted = true; r.accMul = 0.6f; r.dmgMul = 1.3f; r.tag = 99; r.lastKnown = G.P.pos; r.lastSeen = 2; r.yaw = PI_F; r.grenades = 2;
        G.act.push_back(r); G.iv[10] = (int)G.act.size() - 1;
        V3 g[3] = {V3(-5, 0, 245), V3(5, 0, 243.2f), V3(-9, 0, 249)};
        for (int i = 0; i < 3; i++) spawnEnemy(g[i], U_SYND, 0, true, 4);
        say("Ворон", "Котов. Я узнал тебя ещё год назад. Через прицел.", true);
        say("Ворон", "Ты промахнулся на пару сантиметров. Я — не промахнусь.", true);
        setObjective("Отключите пульт управления", V3(0, 0, 251), true);
        G.iv[5] = 0;
    } break;
    case 5:
        G.timerOn = false;
        showStory({"ПУСК ОТМЕНЁН.", "Седой: — Пульт мёртв. Ракеты на земле.", "Док: — Кто-нибудь, скажите это Варгасу. Я хочу видеть его лицо."}, 6.0f, -2, true, RGBc(16, 14, 12));
        break;
    }
}
static void updateM6(float dt) {
    if (G.stage >= 3 && G.stage <= 4 && G.timerOn) { G.timer -= dt; if (G.timer <= 0) { missionFail("Ракеты запущены."); return; } }
    switch (G.stage) {
    case 0: {
        Vehicle& j = G.veh[G.iv[4]];
        if (G.iv[0] == 0 && G.stageT > 2.5f) { G.iv[0] = 1; say("Сова", "Чёрный джип уходит к туннелю! Это Ворон!"); say("Седой", "За ним! Не отставать!"); }
        if (j.pi >= (int)j.path.size()) j.active = false;
        if (G.P.pos.z > 38) setStage(1);
    } break;
    case 1: if (G.P.pos.z > 103) setStage(2); break;
    case 2: if (G.P.pos.z > 159) setStage(3); break;
    case 3: if (G.P.pos.z > 236.5f) setStage(4); break;
    case 4: {
        Actor& r = G.act[G.iv[10]];
        if (G.iv[5] == 0 && r.alive && r.hp < r.maxhp * 0.4f) {
            G.iv[5] = 1; r.invuln = true; r.retreat = true; r.path = {V3(10, 0, 245), V3(14, 0, 245), V3(22, 0, 244), V3(25, 0, 230)}; r.pathI = 0;
            if (r.cover >= 0) G.cover[r.cover].occ = -1;
            r.cover = -1;
            say("Ворон", "Ещё увидимся, Кот. Обязательно.", true);
            say("Седой", "Он уходит! Пульт, Кот! Пульт важнее!");
        }
        if (G.iv[5] == 1 && (r.pathI >= (int)r.path.size() || dist2D(r.pos, V3(25, 0, 230)) < 2)) { r.hidden = true; G.iv[5] = 2; }
        if (!r.alive && G.iv[5] == 0) G.iv[5] = 2;
        bool ravenGone = G.iv[5] == 2 || r.hidden || !r.alive;
        float d = dist2D(G.P.pos, V3(0, 0, 250.4f - 0.6f));
        if (d < 1.9f) {
            if (!ravenGone) { G.useHint = "Ворон держит пульт под огнём!"; G.P.useT = 0; }
            else G.useHint = "Отключить пульт (удерж.)";
        } else G.useHint = "";
        G.progOn = G.P.useT > 0 && ravenGone; G.progLabel = "Отключение пульта"; G.progress = clampf(G.P.useT / 3.0f, 0, 1);
        if (ravenGone && G.P.useT >= 3.0f) { G.progOn = false; G.useHint = ""; setStage(5); }
    } break;
    }
}


// ------------------------------------------------------------ М7 СТАЛЬНОЙ КУЛАК (танк)
static std::string g_objBuf;
static void buildM7() {
    setEnv(V3(0.6f, 0.3f, -0.5f), 0.45f, 0.55f, RGBc(176, 146, 124), 60, 260, RGBc(92, 104, 146), RGBc(232, 164, 114), 280);
    setGround(RGBc(122, 110, 80), RGBc(106, 98, 72), 0);
    bounds(-70, -20, 70, 420);
    g_enemyUni = U_MILITIA;
    uint32_t rock = RGBc(128, 112, 92);
    addDeco(-5, 0, -20, 5, 0.02f, 420, RGBc(72, 68, 62));
    for (int z = -20; z <= 420; z += 70) { mountain(-115, (float)z, 45, 60.0f + (z % 140) * 0.2f, rock); mountain(118, (float)z + 35, 48, 70, shadeC(rock, 0.95f)); }
    for (int k = 0; k < 30; k++) {
        uint32_t h = hash2(k, 71);
        float x = ((h & 255) / 255.0f) * 110 - 55, z = (((h >> 8) & 1023) / 1023.0f) * 400;
        if (fabsf(x) < 9 || (z > 85 && z < 170 && fabsf(x) < 30) || z > 355) continue;
        float s = 1.0f + ((h >> 18) & 3) * 0.7f, hh = 1.2f + ((h >> 20) & 3) * 0.6f;
        addBox(x - s, 0, z - s * 0.8f, x + s, hh, z + s * 0.8f, shadeC(rock, 0.85f + ((h >> 24) & 3) * 0.05f), MT_CONCRETE, true);
        G.box.back().tex = TX_ROCK;
    }
    // деревня
    for (int k = 0; k < 6; k++) {
        float z = 92 + k * 12.0f;
        bool L = (k & 1) == 0;
        float x0 = L ? -26.0f : 12.0f;
        house(x0, z, x0 + 12, z + 8, 4.5f + (k % 3), kPastel[k % 6]);
        if (k % 2 == 1) house(-26, z + 2, -16, z + 7, 3.5f, kPastel[(k + 2) % 6]);
    }
    wallX(118, -10, -6, 1.2f, 0.4f, RGBc(150, 140, 120)); wallX(142, 6, 10, 1.2f, 0.4f, RGBc(150, 140, 120));
    carProp(-8, 108, true, RGBc(60, 55, 50)); carProp(9, 150, false, RGBc(140, 40, 35));
    // река и мост
    addDeco(-70, 0.01f, 368, 70, 0.03f, 380, RGBc(50, 82, 112));
    addDeco(-5, 0.03f, 366, 5, 0.06f, 382, RGBc(112, 106, 96));
    addBox(-5.4f, 0, 366, -5, 1.0f, 382, RGBc(80, 80, 76), MT_METAL); addBox(5, 0, 366, 5.4f, 1.0f, 382, RGBc(80, 80, 76), MT_METAL);
    addBox(-70, 0, 368, -5.4f, 3, 380, 0, MT_INVIS); addBox(5.4f, 0, 368, 70, 3, 380, 0, MT_INVIS);
    G.P.tank = true; G.P.shell = G.P.loaded = 1; G.P.shells[0] = 16; G.P.shells[1] = 30;
    G.P.nade[0] = G.P.nade[1] = G.P.nade[2] = 0;
    g_amb.wind = 0.3f; g_amb.echoAmt = 0.5f;
}
static void stageM7(int s) {
    switch (s) {
    case 0: {
        say("Седой", "«Молот» на ходу. Кот, ты наводчик. Зенитки в деревне — главная цель.", true);
        say("Седой", "По зениткам и пехоте — фугасным. Пулемёт работает, пока орудие заряжается.", true);
        say("Седой", "Бронебойные береги для танков. Смена снаряда — кнопкой «Снаряд».", true);
        setObjective("Уничтожьте зенитные установки (0/3)", V3(0, 0, 120), true);
        V3 aa[3] = {V3(-18, 0, 105), V3(20, 0, 128), V3(-10, 0, 156)};
        for (int i = 0; i < 3; i++) { int v = spawnVeh(VK_TECH, aa[i], frange(-1, 1), 1); G.veh[v].arrived = true; G.veh[v].tag = 1; }
        V3 inf[10] = {V3(-6, 0, 98), V3(7, 0, 104), V3(-11, 0, 117), V3(4, 0, 121), V3(-4, 0, 133), V3(9, 0, 138),
                      V3(-8, 0, 146), V3(6, 0, 152), V3(-14, 0, 160), V3(12, 0, 163)};
        for (int i = 0; i < 10; i++) spawnEnemy(inf[i], U_MILITIA, i % 3 == 0 ? 2 : 0, false, 0);
        G.iv[5] = 3;
    } break;
    case 1: {
        checkpoint();
        say("Майор Грин", "«Молот», две коробки с севера! Вражеские танки!", true);
        say("Седой", "Заряжай бронебойный! Фугасом его не взять. Три попадания — и он наш.", true);
        G.P.shells[0] = std::max(G.P.shells[0], 10);
        setObjective("Уничтожьте танки противника", V3(0, 0, 225), true);
        int t = spawnVeh(VK_TANK, V3(-12, 0, 300), PI_F, 1); G.veh[t].path = {V3(-10, 0, 250), V3(-8, 0, 215)}; G.veh[t].hp = G.veh[t].maxhp = 1000; G.veh[t].tag = 2;
        t = spawnVeh(VK_TANK, V3(14, 0, 320), PI_F, 1); G.veh[t].path = {V3(12, 0, 270), V3(10, 0, 232)}; G.veh[t].hp = G.veh[t].maxhp = 1000; G.veh[t].tag = 2;
        V3 inf[4] = {V3(-16, 0, 205), V3(15, 0, 212), V3(-4, 0, 240), V3(6, 0, 250)};
        for (int i = 0; i < 4; i++) spawnEnemy(inf[i], U_MILITIA, i < 2 ? 2 : 0, false, 2);
    } break;
    case 2: {
        checkpoint();
        say("Седой", "Чисто. Гони к мосту, пока не подтянули резервы!", true);
        setObjective("Прорвитесь к мосту", V3(0, 0, 374), true);
        int t = spawnVeh(VK_TECH, V3(-60, 0, 300), PI_F * 0.5f, 1); G.veh[t].path = {V3(-25, 0, 300), V3(-13, 0, 302)}; G.veh[t].cargo = 3; G.veh[t].tag = 3;
        t = spawnVeh(VK_TECH, V3(60, 0, 325), -PI_F * 0.5f, 1); G.veh[t].path = {V3(25, 0, 325), V3(14, 0, 322)}; G.veh[t].cargo = 3; G.veh[t].tag = 3;
        V3 inf[6] = {V3(-8, 0, 330), V3(8, 0, 336), V3(-12, 0, 345), V3(11, 0, 350), V3(-3, 0, 358), V3(4, 0, 362)};
        for (int i = 0; i < 6; i++) spawnEnemy(inf[i], U_MILITIA, i % 2 == 0 ? 2 : 0, false, 3);
    } break;
    case 3:
        showStory({"Мост наш. Дорога на Тарсу открыта.", "Седой: — Глуши мотор. Дальше — пешком, через перевал."}, 5.0f, -2, true, RGBc(24, 16, 12));
        break;
    }
}
static void updateM7(float dt) {
    (void)dt;
    switch (G.stage) {
    case 0: {
        int n = 0;
        for (auto& v : G.veh) if (v.tag == 1 && v.alive) n++;
        if (n != G.iv[5]) { G.iv[5] = n; g_objBuf = fmt("Уничтожьте зенитные установки (%d/3)", 3 - n); setObjective(g_objBuf.c_str(), V3(0, 0, 130), true); if (n > 0) say("Седой", "Зенитка готова! Следующая!", true, true); }
        if (n == 0) setStage(1);
    } break;
    case 1: {
        int n = 0;
        for (auto& v : G.veh) if (v.tag == 2 && v.alive) n++;
        if (n == 0) setStage(2);
    } break;
    case 2: if (G.P.pos.z > 372) setStage(3); break;
    }
}

// ------------------------------------------------------------ М8 ПЕРЕВАЛ
static const V3 kRadar[3] = {V3(-30, 0, 60), V3(28, 0, 95), V3(-12, 0, 138)};
static const V3 kRally(-42, 0, 22);
static void buildM8() {
    setEnv(V3(-0.4f, 0.45f, 0.6f), 0.52f, 0.4f, RGBc(178, 182, 192), 25, 140, RGBc(118, 128, 150), RGBc(192, 196, 206), 150);
    setGround(RGBc(216, 219, 226), RGBc(198, 202, 212), 0);
    bounds(-50, -10, 50, 170);
    G.snow = true; g_enemyUni = U_SYND;
    uint32_t rock = RGBc(110, 112, 118), pine = RGBc(46, 70, 52), snowc = RGBc(232, 234, 240);
    mountain(-90, 80, 40, 70, rock); mountain(95, 60, 45, 80, rock); mountain(0, 230, 70, 90, rock); mountain(-60, 210, 40, 60, rock);
    for (int k = 0; k < 34; k++) {
        uint32_t h = hash2(k, 13);
        float x = ((h & 255) / 255.0f) * 92 - 46, z = (((h >> 8) & 1023) / 1023.0f) * 165;
        bool nearRadar = false;
        for (auto& r : kRadar) if (dist2D(V3(x, 0, z), r) < 6) nearRadar = true;
        if (nearRadar || dist2D(V3(x, 0, z), kRally) < 5 || (x > 15 && x < 25 && z > 146) || fabsf(x - 7.5f) < 5.5f) continue;
        if (k % 3 == 0) {
            float s = 1.2f + (h >> 20 & 3) * 0.5f;
            addBox(x - s, 0, z - s, x + s, 1.0f + (h >> 22 & 3) * 0.5f, z + s, rock, MT_CONCRETE, true);
            G.box.back().tex = TX_ROCK;
            addDeco(x - s, 1.0f + (h >> 22 & 3) * 0.5f, z - s, x + s, 1.1f + (h >> 22 & 3) * 0.5f, z + s, snowc);
        } else {
            addBox(x - 0.2f, 0, z - 0.2f, x + 0.2f, 2.5f, z + 0.2f, RGBc(70, 52, 38), MT_WOOD);
            for (int j = 0; j < 3; j++) {
                float w = 1.6f - j * 0.45f, y = 1.5f + j * 1.4f;
                addDeco(x - w, y, z - w, x + w, y + 1.3f, z + w, pine);
                addDeco(x - w * 0.8f, y + 1.3f, z - w * 0.8f, x + w * 0.8f, y + 1.45f, z + w * 0.8f, snowc);
            }
        }
    }
    // грунтовка через перевал: колеи в снегу, отвалы по краям, вешки
    surface(4.5f, -10, 10.5f, 170, 0.03f, RGBc(188, 184, 180), TX_DIRT);
    for (int sd = -1; sd <= 1; sd += 2) {
        float e = 7.5f + sd * 3.0f;
        for (int k = 0; k < 18; k++) {
            float z = -4 + k * 10.0f + (sd > 0 ? 5 : 0);
            addDeco(e + sd * 0.5f - 0.04f, 0.3f, z - 0.04f, e + sd * 0.5f + 0.04f, 1.5f, z + 0.04f, RGBc(40, 40, 40));
            addDeco(e + sd * 0.5f - 0.045f, 1.2f, z - 0.045f, e + sd * 0.5f + 0.045f, 1.45f, z + 0.045f, RGBc(210, 60, 40));
        }
    }
    sandbags(-4, 6, 4, 6.6f); addBox(-8, 0, 3, -6, 2.4f, 8, RGBc(120, 110, 95));
    for (auto& r : kRadar) {
        addBox(r.x - 1.5f, 0, r.z - 1.5f, r.x + 1.5f, 2.4f, r.z + 1.5f, RGBc(90, 96, 92), MT_METAL);
        addDeco(r.x - 0.15f, 2.4f, r.z - 0.15f, r.x + 0.15f, 7, r.z + 0.15f, RGBc(150, 150, 150));
        addDeco(r.x - 1.6f, 7, r.z - 0.12f, r.x + 1.6f, 8.6f, r.z + 0.12f, RGBc(205, 205, 200));
        lamp(r.x, 8.8f, r.z, RGBc(255, 60, 40));
        sandbags(r.x - 3, r.z + 3, r.x + 3, r.z + 3.6f);
    }
    sandbags(17, 149, 23, 149.6f); sandbags(17, 154.4f, 23, 155); sandbags(17, 149.6f, 17.6f, 154.4f); sandbags(22.4f, 149.6f, 23, 154.4f);
    addDeco(19.8f, 0, 151.8f, 20.2f, 1.2f, 152.2f, RGBc(40, 42, 40));
    lamp(kRally.x, 0.3f, kRally.z, RGBc(255, 90, 60));
    spawnAlly(V3(-2, 0, -2), "Седой", U_GROM, 0); spawnAlly(V3(2, 0, -2), "Док", U_DOC, 1); spawnAlly(V3(0, 0, -4), "Сова", U_GROM, 2);
    giveWeapon(0, W_AR, 240); giveWeapon(1, W_SNIPER, 20);
    G.P.nade[0] = 3; G.P.nade[1] = 1; G.P.nade[2] = 3;
    g_amb.wind = 0.8f; g_amb.echoAmt = 0.55f;
}
static int nextRadar() {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < 3; i++) if (!G.iv[i]) { float d = dist2D(G.P.pos, kRadar[i]); if (d < bd) { bd = d; best = i; } }
    return best;
}
static void stageM8(int s) {
    switch (s) {
    case 0: {
        say("Седой", "Перевал Тарса. Три радара прикрывают ГЭС с воздуха. Минируем все три.");
        say("Док", "Снег. Обожаю снег. Особенно когда в него стреляют.");
        int r = nextRadar();
        setObjective("Заложите заряды на радарах (0/3)", kRadar[r] - V3(0, 0, 2.2f), true);
        V3 p[12] = {V3(-26, 0, 66), V3(-34, 0, 56), V3(-24, 0, 52), V3(24, 0, 100), V3(33, 0, 92), V3(22, 0, 88),
                    V3(-8, 0, 143), V3(-16, 0, 133), V3(-6, 0, 128), V3(0, 0, 80), V3(10, 0, 112), V3(-20, 0, 98)};
        for (int i = 0; i < 12; i++) spawnEnemy(p[i], U_SYND, i % 4 == 3 ? 1 : 0, false, 0);
        G.fl[3] = 10; G.iv[4] = 1; G.iv[5] = 0;
    } break;
    case 1:
        checkpoint();
        say("Сова", "Миномёт на гребне, северо-восток! Он нас накроет!");
        setObjective("Уничтожьте миномётный расчёт", V3(20, 0, 152), true);
        for (int i = 0; i < 4; i++) spawnEnemy(V3(18.5f + (i & 1) * 3.0f, 0, 150.5f + (i >> 1) * 3.0f), U_SYND, 0, false, 1);
        G.iv[4] = 1;
        break;
    case 2:
        checkpoint();
        G.iv[4] = 0;
        say("Седой", "Заряды на таймере! Двадцать секунд — все к точке сбора!");
        setObjective("Отходите к точке сбора", kRally, true);
        G.timerOn = true; G.timer = 20; G.timerLabel = "Подрыв"; G.iv[3] = 0; G.iv[6] = 0;
        break;
    case 3:
        showStory({"Радары уничтожены.", "Небо над Тарсой открыто.", "Седой: — Утром идём на станцию."}, 5.0f, -2, true, RGBc(20, 22, 30));
        break;
    }
}
static void updateM8(float dt) {
    // миномётный обстрел
    if (G.iv[4] && G.stage <= 1) {
        G.fl[3] -= dt;
        if (G.fl[3] <= 0 && G.fl[5] <= 0) {
            float a = frange(0, 2 * PI_F), d = frange(5, 14);
            G.fl[6] = G.P.pos.x + cosf(a) * d; G.fl[8] = G.P.pos.z + sinf(a) * d;
            G.fl[5] = 1.4f; G.fl[3] = frange(7, 11);
            playSnd(ST_WHIZ, 0.55f, 0, 0, 1.3f);
            G.warn = "МИНОМЁТ! В УКРЫТИЕ!"; G.warnT = 2.0f;
        }
        if (G.fl[5] > 0) { G.fl[5] -= dt; if (G.fl[5] <= 0) explode(V3(G.fl[6], groundAt(V3(G.fl[6], 5, G.fl[8]), 0.1f), G.fl[8]), 1, 6.5f, 140, false); }
    }
    switch (G.stage) {
    case 0: {
        int r = nextRadar();
        G.useHint = "";
        if (r >= 0) {
            V3 pp = kRadar[r] - V3(0, 0, 2.2f);
            for (int i = 0; i < 3; i++) if (!G.iv[i] && dist2D(G.P.pos, kRadar[i]) < 4.4f) { r = i; pp = kRadar[i] - V3(0, 0, 2.2f); G.useHint = "Заложить заряд (удерж.)"; }
            G.wp = pp;
            if (!G.useHint.empty() && G.P.useT >= 2.5f) {
                G.iv[r] = 1; G.iv[5]++; G.useHint = ""; G.P.useT = 0;
                playSnd(ST_BEEP, 0.4f, 0, 0, 0.2f, 0, 0, 1200);
                say("Кот", G.iv[5] < 3 ? "Заряд установлен." : "Последний заряд установлен.");
                g_objBuf = fmt("Заложите заряды на радарах (%d/3)", G.iv[5]);
                int nr = nextRadar();
                if (nr >= 0) setObjective(g_objBuf.c_str(), kRadar[nr] - V3(0, 0, 2.2f), true);
            }
        }
        if (G.iv[5] >= 3) setStage(1);
    } break;
    case 1: if (countAlive(1) == 0) { say("Сова", "Расчёт уничтожен. Обстрела больше не будет."); setStage(2); } break;
    case 2:
        G.timer -= dt;
        if (G.iv[6] == 0 && G.timer <= 0) {
            G.iv[6] = 1; G.timerOn = false;
            for (auto& r : kRadar) explode(r + V3(0, 1.5f, 0), 0, 10, 320, true);
            G.P.shake = 1;
        }
        if (G.iv[6] == 1 && dist2D(G.P.pos, kRally) < 7) setStage(3);
        break;
    }
}
// ------------------------------------------------------------ М9 ПРИКРЫТИЕ: защитить штурм автобазы с высоты
static const float kRidgeY = 3.2f;
static const V3 kMolot(0, 0, 132);
static const char* kAssault[5] = {"Барс", "Лис", "Гриф", "Шмель", "Тур"};
static void buildM9() {
    setEnv(V3(-0.55f, 0.5f, 0.45f), 0.5f, 0.62f, RGBc(196, 200, 204), 70, 260, RGBc(112, 142, 192), RGBc(220, 212, 196), 280);
    setGround(RGBc(104, 112, 70), RGBc(92, 100, 62), 0);
    bounds(-62, -12, 62, 200);
    g_enemyUni = U_SYND;
    uint32_t rock = RGBc(118, 112, 100), grassC = RGBc(88, 104, 58), wallC = RGBc(150, 146, 134);
    mountain(-120, 60, 50, 55, rock); mountain(125, 90, 55, 60, shadeC(rock, 0.95f)); mountain(0, 240, 80, 70, rock);
    // высота: плато, парапет из мешков, спуск террасами к полю
    int r0 = addBox(-16, 0, -6, 16, kRidgeY, 12, rock, MT_CONCRETE); G.box[r0].tex = TX_ROCK;
    int r1 = addBox(-22, 0, -4, -16, 2.0f, 10, rock, MT_CONCRETE); G.box[r1].tex = TX_ROCK;
    int r2 = addBox(16, 0, -4, 22, 2.2f, 9, rock, MT_CONCRETE); G.box[r2].tex = TX_ROCK;
    for (int k = 0; k < 8; k++) {
        float y = kRidgeY - 0.4f * (k + 1);
        int b = addBox(-4, 0, 12 + k * 1.2f, 4, y, 13.2f + k * 1.2f, shadeC(rock, 0.97f), MT_CONCRETE); G.box[b].tex = TX_ROCK;
    }
    for (int s = -1; s <= 1; s += 2) {
        int b = addBox(s < 0 ? -13.0f : 4.6f, kRidgeY, 11.2f, s < 0 ? -4.6f : 13.0f, kRidgeY + 0.9f, 11.9f, RGBc(150, 135, 100), MT_CONCRETE, true);
        G.box[b].tex = TX_SANDBAG;
    }
    crate(-8, 4, 0.5f, 0.9f, kRidgeY); crate(9, 3, 0.45f, 0.8f, kRidgeY);
    grassPatch(-60, 16, -8, 40, 160, grassC); grassPatch(8, 16, 60, 40, 160, grassC);
    // поле: камни, обломки, низкая стена
    float rx[4] = {-11, -3.5f, 4, 11};
    for (int i = 0; i < 4; i++) { int b = addBox(rx[i] - 1.1f, 0, 43.5f, rx[i] + 1.1f, 1.15f, 44.7f, shadeC(rock, 0.9f), MT_CONCRETE, true); G.box[b].tex = TX_ROCK; }
    carProp(-20, 50, true, RGBc(70, 64, 58)); carProp(22, 36, false, RGBc(96, 40, 34));
    wallX(58.5f, -15, 15, 1.1f, 0.5f, wallC, {-2.5f, 7.5f});
    for (int k = 0; k < 10; k++) {
        uint32_t h = hash2(k, 91);
        float x = ((h & 255) / 255.0f) * 100 - 50, z = 28 + (((h >> 8) & 255) / 255.0f) * 52;
        if (fabsf(x) < 16) continue;
        int b = addBox(x - 1, 0, z - 0.8f, x + 1, 0.9f + ((h >> 16) & 3) * 0.2f, z + 0.8f, shadeC(rock, 0.88f), MT_CONCRETE, true); G.box[b].tex = TX_ROCK;
    }
    // передовой окоп противника
    sandbags(-19, 72.5f, -6, 73.1f); sandbags(-3, 72.5f, 3, 73.1f); sandbags(6, 72.5f, 19, 73.1f);
    addDeco(-20, 0, 73.2f, 20, 0.05f, 76, RGBc(84, 76, 58));
    // автобаза: ограда с воротами, ангары, гаражи, вышка
    for (float wz : {92.0f, 150.0f}) {   // ограда с широкими воротами и столбами
        wallX(wz, -30, -4, 2.6f, 0.4f, wallC); wallX(wz, 4, 30, 2.6f, 0.4f, wallC);
        addBox(-4.5f, 0, wz - 0.4f, -3.7f, 3.2f, wz + 0.4f, shadeC(wallC, 0.85f), MT_CONCRETE); addBox(3.7f, 0, wz - 0.4f, 4.5f, 3.2f, wz + 0.4f, shadeC(wallC, 0.85f), MT_CONCRETE);
    }
    addDeco(-11, 0, 91.0f, -4.6f, 2.4f, 91.2f, RGBc(70, 74, 66));   // распахнутая створка ворот
    wallZ(-30, 92, 150, 2.6f, 0.4f, wallC, {121.0f});
    wallZ(30, 92, 150, 2.6f, 0.4f, wallC, {108.0f});
    house(-25, 100, -9, 116, 6.5f, RGBc(150, 156, 150), true);
    house(10, 100, 26, 110, 4.0f, kPastel[1], true);
    house(-25, 132, -13, 144, 4.5f, kPastel[0], true);
    house(14, 128, 26, 144, 5.0f, RGBc(140, 146, 140), true);
    container(-6, 112, true, RGBc(46, 82, 120)); container(7, 120, false, RGBc(150, 70, 40));
    for (int k = 0; k < 3; k++) { int b = addBox(-27, 0, 120 + k * 3.2f, -24.5f, 2.6f, 122.4f + k * 3.2f, RGBc(170, 170, 160), MT_METAL, true); (void)b; }
    crate(5, 140, 0.6f, 1.2f); crate(-6, 138, 0.5f, 1.0f); crate(-4, 100, 0.5f, 1.0f); crate(6, 98, 0.6f, 1.1f);
    addBox(14, 0, 94, 18, 5.0f, 98, RGBc(110, 104, 92), MT_CONCRETE);                       // вышка
    addDeco(13.6f, 5.0f, 93.6f, 18.4f, 5.15f, 98.4f, RGBc(80, 74, 66));
    addDeco(-4, 0.01f, 92, 4, 0.03f, 150, RGBc(72, 70, 66));                                 // плац
    lamp(-4.6f, 3.2f, 91.6f, RGBc(255, 210, 150)); lamp(4.6f, 3.2f, 91.6f, RGBc(255, 210, 150));
    // «Молот» в укрытии
    int t = spawnVeh(VK_TANK, kMolot, PI_F, 0);
    G.veh[t].hp = 1e9f; G.veh[t].maxhp = 1e9f; G.veh[t].tag = -7; G.veh[t].orbitA = 0;
    // союзники: Сова — наблюдатель на высоте, Седой ведёт штурмовую группу
    int sv = spawnAlly(V3(-2.5f, kRidgeY, 7), "Сова", U_SOVA, 0); G.act[sv].scripted = true; G.act[sv].target = V3(-2.5f, kRidgeY, 9.8f);
    int sd = spawnAlly(V3(0, 0, 25), "Седой", U_GROM, 1); G.act[sd].scripted = true; G.act[sd].target = G.act[sd].pos; G.act[sd].dmgMul = 0.4f;
    for (int i = 0; i < 5; i++) {
        int a = spawnAlly(V3(-8.0f + i * 4.0f, 0, 24), kAssault[i], i == 2 ? U_DOC : U_GROM, i);
        Actor& A = G.act[a]; A.invuln = false; A.tag = 77; A.hp = A.maxhp = 120; A.scripted = true; A.target = A.pos; A.dmgMul = 0.3f; A.accMul = 0.7f;
    }
    giveWeapon(0, W_AR, 270); giveWeapon(1, W_SNIPER, 30);
    G.P.nade[0] = 3; G.P.nade[1] = 0; G.P.nade[2] = 2;
    g_amb.wind = 0.35f; g_amb.echoAmt = 0.45f;
}
// маршрут группы: точки через ворота и т. п.; линия line, номер бойца i (Седой — 5)
static void m9Route(int line) {
    for (auto& a : G.act) {
        if (a.kind != AK_ALLY || !a.alive || a.tag != 77) { if (!(a.kind == AK_ALLY && !strcmp(a.name, "Седой"))) continue; }
        int i = a.tag == 77 ? a.formation : 5;
        float x = -10.0f + i * 4.0f;
        a.path.clear(); a.pathI = 0;
        switch (line) {
        case 0: a.path.push_back(V3(x * 0.9f, 0, 33)); { float rxs[6] = {-11, -3.5f, 4, 11, -7, 7}; a.path.push_back(V3(rxs[i] + (i & 1 ? 0.6f : -0.6f), 0, 42.3f)); } break;
        case 1: a.path.push_back(V3(x * 1.1f, 0, 50)); a.path.push_back(V3(x * 1.1f + (fabsf(x * 1.1f + 2.5f) < 1.5f ? 1.6f : 0.0f), 0, 57.2f)); break;
        case 2: a.path.push_back(V3(x * 0.5f, 0, 68)); a.path.push_back(V3(x * 0.6f, 0, 74.6f)); a.path.push_back(V3(x * 0.25f, 0, 89.5f)); break;
        case 3: {
            a.path.push_back(V3(x * 0.15f, 0, 89)); a.path.push_back(V3(x * 0.15f, 0, 96));
            V3 inside[6] = {V3(-6, 0, 124), V3(-5, 0, 145), V3(5, 0, 117), V3(8, 0, 126), V3(-7, 0, 140), V3(5, 0, 145)};
            a.path.push_back(inside[i]);
        } break;
        }
        a.target = a.path[0];
    }
}
static void m9Place(int line) {   // старт с контрольной точки: группа уже на рубеже
    m9Route(line);
    for (auto& a : G.act) if (a.kind == AK_ALLY && !a.path.empty() && a.alive) { a.pos = a.path.back(); a.pathI = (int)a.path.size() - 1; a.target = a.pos; }
}
static int m9GroupAlive() { int n = 0; for (auto& a : G.act) if (a.kind == AK_ALLY && a.tag == 77 && a.alive) n++; return n; }
static void m9Objective(const char* base) {
    g_objBuf = fmt("%s  (группа: %d/5)", base, m9GroupAlive());
    setObjective(g_objBuf.c_str(), G.wp, G.wpOn);
}
static void stageM9(int s) {
    switch (s) {
    case 0: {
        say("Седой", "«Молот» на автобазе. Мы идём за ним. Кот, ты — наши глаза на высоте.", true);
        say("Сова", "Окоп перед воротами, шесть стрелков. На вышке — пулемёт.");
        say("Седой", "Пока они живы, мы не пройдём. Работай.", true);
        G.wp = V3(0, 0, 74); G.wpOn = true; m9Objective("Прикройте штурм: уничтожьте окоп и вышку");
        V3 p[6] = {V3(-15, 0, 74.4f), V3(-10, 0, 74.4f), V3(-1, 0, 74.4f), V3(9, 0, 74.4f), V3(15, 0, 74.4f), V3(-6, 0, 84)};
        for (int i = 0; i < 6; i++) spawnEnemy(p[i], U_SYND, 0, true, 1);
        int mg = spawnEnemy(V3(16, 5.0f, 96), U_SYND, 0, true, 1); G.act[mg].stationary = true; G.act[mg].accMul = 1.3f;
        m9Route(0);
        G.iv[0] = 5; G.iv[1] = 0; G.iv[2] = 0;
    } break;
    case 1: {
        checkpoint();
        say("Седой", "Окоп чист! Перебежками — к стене!", true);
        say("Сова", "Пикап с пулемётом справа! Заходит группе во фланг!");
        say("Седой", "Кот, гранатой или в упор по кабине!", true);
        G.wp = V3(25, 0, 50); G.wpOn = true; m9Objective("Уничтожьте пулемётный пикап и десант");
        int v = spawnVeh(VK_TECH, V3(60, 0, 64), -PI_F * 0.5f, 1);
        G.veh[v].path = {V3(42, 0, 56), V3(25, 0, 50)}; G.veh[v].cargo = 3; G.veh[v].tag = 2;
        m9Route(1);
    } break;
    case 2: {
        checkpoint();
        say("Седой", "Входим на базу!", true);
        say("Сова", "Кот, грузовики с запада! Контратака, человек восемь!");
        G.wp = V3(-36, 0, 80); G.wpOn = true; m9Objective("Отразите контратаку с запада");
        int a = spawnVeh(VK_TRUCK, V3(-62, 0, 40), PI_F * 0.5f, 1);
        G.veh[a].path = {V3(-48, 0, 46), V3(-38, 0, 62)}; G.veh[a].cargo = 4; G.veh[a].tag = 3;
        int b = spawnVeh(VK_TRUCK, V3(-62, 0, 116), PI_F * 0.5f, 1);
        G.veh[b].path = {V3(-50, 0, 108), V3(-40, 0, 98)}; G.veh[b].cargo = 4; G.veh[b].tag = 3;
        m9Route(2);
    } break;
    case 3: {
        checkpoint();
        say("Седой", "Мы у «Молота»! Заводим. Ему нужна минута прогрева.", true);
        say("Сова", "С севера гранатомётчики. Если они доберутся до танка — всё зря.");
        say("Седой", "Кот, спускайся. Сверху тебе их не достать.", true);
        G.wp = V3(0, 0, 150); G.wpOn = true; m9Objective("Защитите «Молот» от гранатомётчиков");
        G.progOn = true; G.progress = 0; G.iv[3] = 0; G.iv[4] = 0; G.fl[2] = 0; G.fl[3] = 0;
        m9Route(3);
    } break;
    case 4:
        G.progOn = false;
        showStory({"Двигатель «Молота» ревёт на всю долину.", "Седой: — Все на броню. Кот — в башню, наводчиком.", "Впереди — долина Ирмы."}, 5.0f, -2, true, RGBc(16, 18, 22));
        break;
    }
}
static void m9SpawnRpg(int n, int wave) {
    V3 p[8] = {V3(-24, 0, 190), V3(-6, 0, 194), V3(10, 0, 190), V3(24, 0, 188), V3(52, 0, 150), V3(54, 0, 112), V3(-16, 0, 196), V3(18, 0, 196)};
    for (int k = 0; k < n; k++) {
        int e = spawnEnemy(p[(wave * 3 + k) % 8], U_SYND, 2, true, 4);
        Actor& a = G.act[e];
        a.path = {V3(p[(wave * 3 + k) % 8].x * 0.3f, 0, 158), V3(0, 0, 153)}; a.callT = -6.0f;
        if (p[(wave * 3 + k) % 8].x > 40) a.path = {V3(38, 0, 108), V3(32, 0, 108)};
        a.retreat = true; a.pathI = 0;   // выдвигаются по маршруту к базе, затем действуют сами
        a.lastKnown = kMolot + V3(0, 1.5f, 0); a.lastSeen = 0; a.grenCD = 99;
    }
    say("Сова", wave == 0 ? "Вижу РПГ у северных ворот!" : (wave == 1 ? "Ещё гранатомётчики! Север и восток!" : "Последняя группа, Кот!"));
}
static void updateM9(float dt) {
    // движение группы по маршруту
    for (auto& a : G.act) {
        if (a.kind != AK_ALLY || !a.alive || a.path.empty()) continue;
        if (dist2D(a.pos, a.target) < 1.3f && a.pathI + 1 < (int)a.path.size()) { a.pathI++; a.target = a.path[a.pathI]; }
    }
    int alive = m9GroupAlive();
    if (alive != G.iv[0]) {
        G.iv[0] = alive;
        static const char* kObj[4] = {"Прикройте штурм: уничтожьте окоп и вышку", "Уничтожьте пулемётный пикап и десант", "Отразите контратаку с запада", "Защитите «Молот» от гранатомётчиков"};
        if (G.stage <= 3) m9Objective(kObj[G.stage]);
    }
    if (alive < 2) { missionFail("Штурмовая группа уничтожена. Высоту не удержали."); return; }
    // затаившиеся остатки противника выходят на группу, чтобы этап не застревал
    G.fl[4] += dt;
    if (G.stage <= 2 && G.stageT > (G.stage == 0 ? 45.0f : 8.0f) && G.fl[4] > 4) {
        G.fl[4] = 0;
        V3 c(0, 0, 0); int n = 0;
        for (auto& a : G.act) if (a.kind == AK_ALLY && a.tag == 77 && a.alive) { c += a.pos; n++; }
        if (n) for (auto& a : G.act) if (a.alive && a.team == 1 && a.tag == G.stage + 1 && !a.stationary && !a.sees) { a.alerted = true; if (G.stageT > 45) a.role = 1; a.lastKnown = c / (float)n; a.lastSeen = 0; if (G.stageT > 70) { a.retreat = true; a.path = {c / (float)n + V3(frange(-4, 4), 0, 8)}; a.pathI = 0; } }
    }
    switch (G.stage) {
    case 0: if (countAlive(1) == 0) setStage(1); break;
    case 1: {
        bool techAlive = false; for (auto& v : G.veh) if (v.tag == 2 && v.alive) techAlive = true;
        if (!techAlive && G.iv[1] == 0) { G.iv[1] = 1; say("Седой", "Пикап горит! Добивай пехоту.", true); }
        if (!techAlive && countAlive(2) == 0 && G.stageT > 6) setStage(2);
    } break;
    case 2: {
        bool unloaded = true; for (auto& v : G.veh) if (v.tag == 3 && v.alive && !v.unloaded) unloaded = false;
        if (unloaded && countAlive(3) == 0 && G.stageT > 10) setStage(3);
    } break;
    case 3: {
        G.progress = std::min(1.0f, G.progress + dt / 55.0f);
        G.fl[2] += dt;
        static const float kWaveT[3] = {2, 20, 38};
        static const int kWaveN[3] = {2, 3, 2};
        while (G.iv[3] < 3 && G.fl[2] >= kWaveT[G.iv[3]]) { m9SpawnRpg(kWaveN[G.iv[3]], G.iv[3]); G.iv[3]++; }
        // гранатомётчики целятся в танк: с дистанции 45 м и при прямой видимости — выстрел через 5 с
        V3 tgt = kMolot + V3(0, 2.25f, 0);
        for (auto& a : G.act) {
            if (a.tag != 4 || !a.alive || a.team != 1) continue;
            V3 eye = actorEye(a);
            float d = len(tgt - eye);
            if (d < 46 && losClear(eye, tgt)) {
                a.callT += dt; a.yaw = yawTo(a.pos, kMolot);
                if (a.callT > 6.0f) {
                    a.callT = -10.0f;
                    V3 dir = norm(tgt - V3(0, 0.6f, 0) - eye);
                    throwGrenade(eye + dir * 0.7f, dir * 55.0f, 3, 4.0f, 1, false);
                    sndShot(3, eye, false);
                    G.iv[4]++;
                    G.warn = fmt("«МОЛОТ» ПОДБИТ! (%d/4)", G.iv[4]); G.warnT = 2.5f;
                    say("Седой", G.iv[4] < 4 ? "Попадание в «Молот»! Убери гранатомётчиков!" : "Нас подожгли!", true);
                }
            } else a.callT = std::max(0.0f, a.callT - dt);
        }
        if (G.iv[4] >= 4) { missionFail("«Молот» уничтожен гранатомётчиками."); return; }
        if (G.progress >= 1 && G.iv[3] >= 3 && countAlive(4) == 0) setStage(4);
        else if (G.progress >= 1 && G.iv[5] == 0) { G.iv[5] = 1; say("Седой", "Двигатель готов! Добей последних, и уходим.", true); }
    } break;
    }
}
// ------------------------------------------------------------ М10 ДЖЕБЕЛЬ-АЛИ (ОАЭ): найти и обезвредить боеголовку
// новое: поиск по сканеру (сигнал «горячо-холодно»), оборона при вскрытии, обезвреживание проводов по правилам
struct Defuse { int wire[4]; int indicator; bool cut[4]; int step; int order[4]; float flash; int wrong; };
static Defuse g_df;
static const char* kWireName[4] = {"КРАСНЫЙ", "СИНИЙ", "ЖЁЛТЫЙ", "ЗЕЛЁНЫЙ"};
static const uint32_t kWireCol[4] = {0xFFD23C30u, 0xFF3C78DCu, 0xFFE6C83Cu, 0xFF3CB45Au};
static const V3 kM10Cand[6] = {V3(-20, 0, 29.2f), V3(20, 0, 54.0f), V3(-28, 0, 66.4f), V3(28, 0, 85.0f), V3(-12, 0, 97.4f), V3(36, 0, 116.0f)};
static const V3 kM10Pad(34, 0, 140);
static int g_m10Target = 0;
static V3 m10Door() { V3 c = kM10Cand[g_m10Target]; return c + V3(c.x < 0 ? 1.9f : -1.9f, 0, 0); }
// упрощённый силуэт сверхвысокой башни вдали (подсвеченный)
static void skylineSpire(float x, float z, float s) {
    uint32_t c1 = RGBc(118, 134, 160), c2 = RGBc(96, 110, 136);
    for (int k = 0; k < 24; k++) {
        float y = (k + 0.5f) * 24.0f * s;
        for (int w = 0; w < 3; w++) {
            float a = w * 2.0944f + 1.5708f, L = (70.0f * (1 - (k + w / 3.0f) / 25.0f) + 16.0f) * s;
            V3 d(sinf(a), 0, cosf(a));
            prop(V3(x, 0, z) + d * (L * 0.5f) + V3(0, y, 0), basisYPR(a, 0, 0), V3(13 * s, 12 * s, L * 0.5f), (k & 1) ? c1 : c2);
        }
    }
    for (int k = 0; k < 12; k++) prop(V3(x, (590 + k * 20 + 10) * s, z), basisYPR(k * 0.5f, 0, 0), V3((10 - k * 0.7f) * s, 10 * s, (10 - k * 0.7f) * s * 0.6f), c1);
    lamp(x, 830 * s, z, RGBc(255, 60, 40));
}
static void buildM10() {
    setEnv(V3(-0.35f, 0.55f, 0.4f), 0.34f, 0.24f, RGBc(58, 50, 52), 60, 700, RGBc(10, 14, 28), RGBc(92, 66, 58), 720);
    g_env.cloud = 0.3f;
    setGround(RGBc(78, 78, 76), RGBc(70, 70, 70), 0);
    bounds(-46, -8, 46, 150);
    g_enemyUni = U_SYND;
    // вода залива и причал
    addDeco(-400, 0.01f, -400, 400, 0.02f, -6, RGBc(16, 28, 40));
    addDeco(46, 0.01f, -6, 400, 0.02f, 160, RGBc(16, 28, 40));
    addDeco(-46, 0.02f, -6, 46, 0.04f, -5.6f, RGBc(170, 160, 60));       // жёлтая кромка причала
    for (int k = -4; k <= 4; k++) addBox(k * 10.0f - 0.3f, 0, -5.8f, k * 10.0f + 0.3f, 0.6f, -5.2f, RGBc(60, 60, 58), MT_METAL);   // кнехты
    // портальные краны на причале
    for (int c = -1; c <= 1; c++) {
        float x = c * 28.0f;
        uint32_t cr = c == 0 ? RGBc(200, 70, 50) : RGBc(70, 120, 170);
        for (int s = -1; s <= 1; s += 2) for (int z2 = 0; z2 < 2; z2++)
            addBox(x + s * 7 - 0.5f, 0, -2.0f + z2 * 10 - 0.5f, x + s * 7 + 0.5f, 26, -2.0f + z2 * 10 + 0.5f, cr, MT_METAL);
        addBox(x - 8, 26, -40, x + 8, 28.5f, 14, cr, MT_METAL);
        addBox(x - 3, 28.5f, 2, x + 3, 33, 10, shadeC(cr, 0.8f), MT_METAL);
        lamp(x - 7, 28.6f, -38, RGBc(255, 60, 40)); lamp(x + 7, 28.6f, 12, RGBc(255, 60, 40));
        for (int k = 0; k < 4; k++) addDeco(x - 6 + k * 4, 25.6f, 4, x - 5.2f + k * 4, 26, 4.8f, RGBc(255, 214, 160), true);
    }
    // контейнерный парк: колонки по 6 м, проходы, штабели 1–3
    static const uint32_t kBox[6] = {0xFF2C5A8Cu, 0xFF9A3228u, 0xFFC86E28u, 0xFF3C6E46u, 0xFF8C8C88u, 0xFF6A4A78u};
    float cols[8] = {-36, -28, -20, -12, 12, 20, 28, 36};
    for (int ci = 0; ci < 8; ci++)
        for (int k = 0; k < 16; k++) {
            float z = 23.0f + k * 6.2f;
            if (k == 4 || k == 9 || k == 14) continue;      // поперечные проходы
            uint32_t h = hash2(ci * 31 + k, 909);
            int stack = 1 + (int)(h % 3);
            for (int s = 0; s < stack; s++) container(cols[ci], z, true, kBox[(h >> (4 + s * 3)) % 6], s * 2.6f);
        }
    // вышки освещения с натриевыми лампами
    for (int i = 0; i < 6; i++) {
        float x = (i & 1) ? 8.0f : -8.0f, z = 20.0f + i * 22.0f;
        addBox(x - 0.25f, 0, z - 0.25f, x + 0.25f, 18, z + 0.25f, RGBc(90, 90, 92), MT_METAL);
        addDeco(x - 1.2f, 18, z - 0.4f, x + 1.2f, 18.6f, z + 0.4f, RGBc(255, 196, 120), true);
    }
    // склад на севере, площадка вертолёта
    wallX(132, -40, 10, 7, 0.4f, RGBc(150, 146, 136), {-20.0f}, {-32, -10, 2});
    addBox(-40, 7, 128, 10, 7.4f, 148, RGBc(110, 108, 104));
    wallX(148, -40, 10, 7, 0.4f, RGBc(150, 146, 136)); wallZ(-40, 132, 148, 7, 0.4f, RGBc(150, 146, 136)); wallZ(10, 132, 148, 7, 0.4f, RGBc(150, 146, 136), {140.0f});
    addDeco(kM10Pad.x - 6, 0.03f, kM10Pad.z - 6, kM10Pad.x + 6, 0.06f, kM10Pad.z + 6, RGBc(60, 62, 66));
    addDeco(kM10Pad.x - 3, 0.07f, kM10Pad.z - 3, kM10Pad.x - 2.2f, 0.08f, kM10Pad.z + 3, RGBc(230, 230, 220));
    addDeco(kM10Pad.x + 2.2f, 0.07f, kM10Pad.z - 3, kM10Pad.x + 3, 0.08f, kM10Pad.z + 3, RGBc(230, 230, 220));
    addDeco(kM10Pad.x - 2.2f, 0.07f, kM10Pad.z - 0.4f, kM10Pad.x + 2.2f, 0.08f, kM10Pad.z + 0.4f, RGBc(230, 230, 220));
    for (int k = 0; k < 4; k++) lamp(kM10Pad.x + ((k & 1) ? 6.0f : -6.0f), 0.2f, kM10Pad.z + ((k & 2) ? 6.0f : -6.0f), RGBc(90, 255, 120));
    carProp(-4, 120, true, RGBc(200, 200, 196)); carProp(4, 12, false, RGBc(40, 40, 44)); crate(3, 18, 0.6f, 1.2f); crate(-5, 16, 0.6f, 1.2f);
    sandbags(-3, 126, 3, 126.6f);
    // ночной Дубай на горизонте: башни с горящими окнами и сверхвысокая башня
    for (int i = 0; i < 46; i++) {
        uint32_t h = hash2(i, 1971);
        float x = ((h & 1023) / 1023.0f - 0.5f) * 900, z = 520 + ((h >> 10) & 255) / 255.0f * 160;
        float w = 9 + ((h >> 18) & 7) * 2.0f, hh = 60 + ((h >> 21) & 255) / 255.0f * 190;
        addBox(x - w, 0, z - w, x + w, hh, z + w, RGBc(22, 24, 32), MT_CONCRETE);
        for (float y = 8; y < hh - 4; y += 9) if (hash2(i * 97 + (int)y, 3) & 1) addDeco(x - w * 0.8f, y, z - w - 0.3f, x + w * 0.8f, y + 1.4f, z - w - 0.1f, RGBc(255, 214, 150), true);
        if (hh > 150) lamp(x, hh + 0.2f, z, RGBc(255, 50, 40));
    }
    skylineSpire(70, 600, 0.55f);
    spawnAlly(V3(-2, 0, -1), "Седой", U_GROM, 0); spawnAlly(V3(2, 0, -1), "Док", U_DOC, 1); spawnAlly(V3(0, 0, -3), "Сова", U_SOVA, 2);
    giveWeapon(0, W_AR, 270); giveWeapon(1, W_SMG, 180);
    G.P.nade[0] = 3; G.P.nade[1] = 2; G.P.nade[2] = 2;
    g_amb.wind = 0.35f; g_amb.echoAmt = 0.6f;
}
static float m10Signal() { return clampf(1.0f - dist2D(G.P.pos, kM10Cand[g_m10Target]) / 80.0f, 0, 1); }
static void defuseInit() {
    int perm[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) { int j = (int)(rnd() % (i + 1)); std::swap(perm[i], perm[j]); }
    for (int i = 0; i < 4; i++) { g_df.wire[i] = perm[i]; g_df.cut[i] = false; }
    g_df.indicator = (int)(rnd() % 4); g_df.step = 0; g_df.flash = 0; g_df.wrong = 0;
    // правило: первым — провод цвета индикатора; дальше слева направо, пропуская красный; красный — последним
    int n = 0; bool used[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) if (g_df.wire[i] == g_df.indicator) { g_df.order[n++] = i; used[i] = true; }
    for (int i = 0; i < 4; i++) if (!used[i] && g_df.wire[i] != 0) { g_df.order[n++] = i; used[i] = true; }
    for (int i = 0; i < 4; i++) if (!used[i]) { g_df.order[n++] = i; used[i] = true; }
}
static void stageM10(int s) {
    switch (s) {
    case 0:
        say("Седой", "Джебель-Али. Где-то здесь контейнер с последней боеголовкой.", true);
        say("Сова", "Огни Дубая видно отсюда. Если она сработает — городу конец.");
        setObjective("Пройдите к контейнерному парку", V3(0, 0, 18), true);
        {   // патрули в проходах
            V3 pp[10] = {V3(-16, 0, 30), V3(-16, 0, 90), V3(16, 0, 40), V3(16, 0, 100), V3(-32, 0, 50), V3(32, 0, 70), V3(0, 0, 60), V3(0, 0, 105), V3(-24, 0, 115), V3(24, 0, 25)};
            for (int i = 0; i < 10; i++) {
                int e = spawnEnemy(pp[i], U_SYND, i % 4 == 3 ? 1 : 0, false, 1);
                Actor& a = G.act[e];
                a.path = {pp[i], pp[i] + V3(0, 0, (i & 1) ? -24.0f : 24.0f)}; a.loopPath = true;
            }
        }
        break;
    case 1:
        checkpoint();
        g_m10Target = (int)(rnd() % 6);
        G.fl[10] = m10Door().x; G.fl[11] = m10Door().z;
        say("Док", "Включаю сканер. Чем ближе к заряду — тем чаще писк.");
        say("Седой", "Таймер на боеголовке уже идёт. Четыре минуты. Шевелись, Кот!", true);
        setObjective("Найдите контейнер по сигналу сканера", V3(0, 0, 0), false);
        G.timerOn = true; G.timer = 240 + (S.difficulty == 0 ? 60 : 0); G.timerLabel = "До подрыва";
        G.progOn = true; G.progLabel = "СИГНАЛ"; G.progress = 0; G.fl[12] = 0;
        break;
    case 2: {
        checkpoint();
        say("Док", "Это он! Вскрываю замок. Мне нужно сорок секунд!");
        say("Сова", "Нас засекли. Грузовик с севера!");
        setObjective("Прикройте Дока, пока он вскрывает контейнер", m10Door(), true);
        allyScript("Док", m10Door());
        { int dk = findAlly("Док"); if (dk >= 0 && dist2D(G.act[dk].pos, m10Door()) > 6) { V3 p = m10Door() + V3(0, 0, -2.5f); if (!bodyBlocked(p, 0.3f, 1.6f)) G.act[dk].pos = p; } }
        G.progLabel = "ВСКРЫТИЕ"; G.progress = 0; G.iv[3] = 0; G.fl[13] = 0;
        int t = spawnVeh(VK_TRUCK, V3(24, 0, 190), PI_F, 1); G.veh[t].path = {V3(24, 0, 150), V3(24, 0, 126)}; G.veh[t].cargo = 5; G.veh[t].tag = 2;
        for (auto& a : G.act) if (a.alive && a.team == 1) { a.alerted = true; a.lastKnown = m10Door(); a.lastSeen = 0; }
    } break;
    case 3:
        G.progOn = false;
        say("Док", "Открыто. Кот, у меня руки трясутся — режь ты. Я подскажу.");
        defuseInit();
        G.screen = SCR_DEFUSE;
        break;
    case 4:
        checkpoint();
        G.timerOn = false; allyFree("Док");
        say("Док", "Индикатор погас. Боеголовка мертва.");
        say("Седой", "Вертолёт на площадке у склада. Уходим, пока не подошли остальные!", true);
        setObjective("Прорвитесь к вертолётной площадке", kM10Pad, true);
        {
            V3 pp[6] = {V3(16, 0, 120), V3(24, 0, 128), V3(30, 0, 118), V3(-16, 0, 124), V3(40, 0, 128), V3(20, 0, 140)};
            for (int i = 0; i < 6; i++) spawnEnemy(pp[i], U_SYND, i % 3 == 2 ? 1 : 0, true, 3);
            int tc = spawnVeh(VK_TECH, V3(60, 0, 140), -PI_F * 0.5f, 1); G.veh[tc].path = {V3(44, 0, 132), V3(40, 0, 118)}; G.veh[tc].tag = 3;
        }
        break;
    case 5:
        G.progOn = false;
        showStory({"Вертолёт уходит над заливом.", "Внизу горят огни Дубая. Башня стоит.", "Седой: — Вот теперь, Кот, дело закрыто."}, 5.5f, -2, true, RGBc(8, 10, 18));
        break;
    }
}
static void updateM10(float dt) {
    if (G.timerOn && G.stage >= 1 && G.stage <= 3) {
        G.timer -= dt;
        if (G.timer <= 0) { missionFail("Боеголовка сработала."); return; }
    }
    switch (G.stage) {
    case 0: if (G.P.pos.z > 16) setStage(1); break;
    case 1: {
        float sg = m10Signal();
        G.progress = lerpf(G.progress, sg, std::min(1.0f, dt * 4));
        G.fl[12] -= dt;
        if (G.fl[12] <= 0) {   // писк сканера: чем ближе, тем чаще и выше
            G.fl[12] = lerpf(1.6f, 0.12f, sg * sg);
            playSnd(ST_BEEP, 0.18f + sg * 0.2f, 0, 0, 0.05f, 0, 0, 600 + sg * 900);
        }
        G.useHint = "";
        if (dist2D(G.P.pos, m10Door()) < 2.2f) {
            G.useHint = "Проверить контейнер (удерж.)";
            if (G.P.useT >= 1.5f) { G.P.useT = 0; G.useHint = ""; setStage(2); }
        } else {
            for (int i = 0; i < 6; i++) {
                if (i == g_m10Target) continue;
                V3 c = kM10Cand[i], d = c + V3(c.x < 0 ? 1.9f : -1.9f, 0, 0);
                if (dist2D(G.P.pos, d) < 2.2f && G.fl[14] <= 0) { say("Док", "Пусто. Сигнал слабый — ищи дальше.", false, true); G.fl[14] = 6; }
            }
        }
        G.fl[14] -= dt;
    } break;
    case 2: {
        G.fl[13] += dt;
        int dk = findAlly("Док");
        bool atDoor = dk >= 0 && dist2D(G.act[dk].pos, m10Door()) < 2.5f;
        if (atDoor) G.progress = std::min(1.0f, G.progress + dt / 40.0f);
        static const float kW[3] = {8, 20, 31};
        while (G.iv[3] < 3 && G.fl[13] >= kW[G.iv[3]]) {
            int w = G.iv[3]++;
            V3 base = w == 1 ? V3(-44, 0, m10Door().z) : V3(44, 0, m10Door().z + 10);
            if (w == 2) base = V3(m10Door().x, 0, 146);
            for (int k = 0; k < 4; k++) { int e = spawnEnemy(base + V3(0, 0, k * 2.0f), U_SYND, k % 2, true, 2); G.act[e].lastKnown = m10Door(); }
            say("Сова", w == 1 ? "Запад! Ещё четверо!" : (w == 2 ? "С севера, от склада!" : "Восточный проход!"));
        }
        if (G.progress >= 1) setStage(3);
    } break;
    case 4: if (dist2D(G.P.pos, kM10Pad) < 6 && countAlive(3) <= 2) setStage(5); break;
    }
}
// экран обезвреживания: мир на паузе, таймер идёт
static void defuseCut(int i) {
    if (i < 0 || i > 3 || g_df.cut[i]) return;
    g_df.cut[i] = true;
    if (g_df.order[g_df.step] == i) {
        g_df.step++;
        playSnd(ST_BEEP, 0.35f, 0, 0, 0.08f, 0, 0, 1400);
        if (g_df.step >= 4) { G.screen = SCR_PLAY; setStage(4); }
        else say("Док", g_df.step == 1 ? "Хорошо. Дальше — по правилу." : "Так. Ещё.", false, true);
    } else {
        g_df.cut[i] = false; g_df.wrong++; g_df.flash = 1;
        G.timer -= 15;
        playSnd(ST_EXPLO, 0.25f, 0, 0, 0.3f);
        say("Док", "Не тот! Цепь замкнуло — минус пятнадцать секунд!", false, true);
        G.P.shake = 0.6f; rumble(0.8f, 0.6f, 300);
    }
}
// ------------------------------------------------------------ диспетчер
static void stageSpawnPos(int m, int s, V3& p, float& yaw) {
    yaw = 0; p = V3(0, 0, 0);
    switch (m) {
    case 0: p = s >= 4 ? V3(-28, 0, 30) : (s == 3 ? V3(-34, 0, -2) : V3(0, 0, -2)); if (s >= 4) yaw = PI_F; break;
    case 1: p = s <= 1 ? V3(0, 0, -24) : (s == 2 ? V3(0, 0, 8) : (s == 3 ? V3(0, -5, 20) : V3(-3, -5, 26))); if (s == 4) yaw = PI_F; break;
    case 2: p = s == 0 ? V3(0, 0, 2) : (s == 1 ? V3(0, 0, 50) : V3(18, 0, 80)); break;
    case 3: {
        V3 ps[7] = {V3(0, 0, 0), V3(-5, 0, 42), V3(-24, 0, 84), V3(0, 0, 126), V3(0, 0.7f, 149), V3(0, 0.7f, 149), V3(-20, 0, 25)};
        p = ps[clampi(s, 0, 6)];
    } break;
    case 4: p = V3(0, 0, 8); break;
    case 6: p = s == 0 ? V3(0, 0, 0) : (s == 1 ? V3(0, 0, 168) : V3(0, 0, 236)); break;
    case 7: p = s == 0 ? V3(0, 0, 0) : (s == 1 ? V3(-12, 0, 124) : V3(14, 0, 140)); if (s == 2) yaw = -2.2f; break;
    case 8: p = s <= 2 ? V3(0, kRidgeY, 6) : V3(0, 0, 84); break;
    case 9: p = s == 0 ? V3(0, 0, 2) : (s == 1 ? V3(0, 0, 18) : m10Door() + V3(m10Door().x < 0 ? 1.5f : -1.5f, 0, -1.5f)); break;
    case 5: {
        V3 ps[5] = {V3(0, 0, -6), V3(0, 0, 36), V3(0, 0, 98), V3(0, 0, 158), V3(0, 0, 232)};
        p = ps[clampi(s, 0, 4)];
    } break;
    }
}
// атмосфера войны: облачность, дымы над городом, воронки, ежи, подбитая техника
static void missionAtmosphere(int m) {
    switch (m) {
    case 0: g_env.cloud = 0.32f; lawn(0.35f, RGBc(118, 128, 70), RGBc(150, 146, 86)); break;
    case 1: g_env.cloud = 0.96f; break;
    case 2:
        g_env.cloud = 0.42f; G.war = true;
        plume(V3(-62, 0, 92), 2.6f, true); plume(V3(72, 0, 40), 2.0f, false); plume(V3(18, 0, 150), 3.2f, true);
        scorch(0.5f, 20, 1.6f); scorch(-1, 45, 1.3f); scorch(2, 70, 1.8f);
        break;
    case 3: g_env.cloud = 0.55f; plume(V3(130, 0, 210), 2.2f, false); lawn(0.5f, RGBc(74, 88, 50), RGBc(104, 112, 62)); break;
    case 4:
        g_env.cloud = 0.68f; G.war = true;
        plume(V3(-92, 0, 60), 3.0f, true); plume(V3(84, 0, 120), 3.4f, true); plume(V3(10, 0, -75), 2.4f, false);
        battleScars(-55, -25, 55, 90, 16, 0, 41, {V3(0, 0, 10)}, 24);
        burntCar(-46, 72, true); burntCar(50, 32, false);
        break;
    case 5:
        g_env.cloud = 0.5f; G.war = true; lawn(0.12f, RGBc(124, 116, 76), RGBc(146, 132, 84));
        plume(V3(0, 0, 300), 3.4f, true); plume(V3(-70, 0, 150), 2.6f, false);
        crater(-15, 28, 1.6f); crater(14, 58, 1.4f); crater(-11, 92, 1.8f); scorch(8, 18, 2.0f);
        break;
    case 6:
        g_env.cloud = 0.5f; G.war = true; lawn(0.3f, RGBc(150, 136, 84), RGBc(122, 118, 70));
        battleScars(-55, 10, 55, 340, 34, 0, 77, {V3(0, 0, 0)}, 0);
        for (int k = 0; k < 6; k++) { float z = 190.0f + k * 24; hedgehog(k & 1 ? 16.0f + k : -15.0f - k, z, k * 0.7f); }
        wreckTank(-24, 58, 0.6f); wreckTank(28, 238, -2.4f);
        plume(V3(-20, 0, 124), 1.6f, true); plume(V3(96, 0, 300), 3.2f, false); plume(V3(-105, 0, 210), 3.0f, true);
        break;
    case 7: g_env.cloud = 0.86f; G.war = true; crater(-20, 40, 1.3f); crater(10, 70, 1.5f); crater(26, 118, 1.2f); break;
    case 8:
        g_env.cloud = 0.5f; G.war = true; lawn(0.55f, RGBc(84, 100, 54), RGBc(118, 124, 66));
        battleScars(-55, 26, 55, 70, 16, 0, 93, {V3(0, 0, 24), V3(0, 0, 42), V3(0, 0, 57)}, 4);
        for (int k = 0; k < 5; k++) hedgehog(-28.0f - k * 4.5f, 30.0f + k * 7.0f, k * 0.9f);
        wireFence(V3(-20, 0, 69), V3(-6.5f, 0, 69)); wireFence(V3(6.5f, 0, 69), V3(20, 0, 69));
        wreckTank(42, 30, 2.2f);
        plume(V3(0, 0, 215), 3.2f, true); plume(V3(-92, 0, 140), 2.8f, true); plume(V3(84, 0, 186), 2.4f, false);
        break;
    }
}
static void missionBuild(int m) {
    switch (m) { case 0: buildM1(); break; case 1: buildM2(); break; case 2: buildM3(); break; case 3: buildM4(); break; case 4: buildM5(); break; case 6: buildM7(); break; case 7: buildM8(); break; case 8: buildM9(); break; case 9: buildM10(); break; default: buildM6(); break; }
    missionAtmosphere(m);
}
static void setStage(int s) {
    const MissionInfo& C = MI[clampi(g_ch, 0, NCH - 1)];
    if (C.mission == G.mission && C.endStage >= 0 && s >= C.endStage) {
        if (C.outro[0]) {
            std::vector<std::string> L;
            for (int i = 0; i < 3 && C.outro[i]; i++) L.push_back(C.outro[i]);
            showStory(L, 5.0f, -2, true, RGBc(10, 12, 16));
        } else missionComplete();
        return;
    }
    G.stage = s; G.stageT = 0;
    switch (G.mission) { case 0: stageM1(s); break; case 1: stageM2(s); break; case 2: stageM3(s); break; case 3: stageM4(s); break; case 4: stageM5(s); break; case 6: stageM7(s); break; case 7: stageM8(s); break; case 8: stageM9(s); break; case 9: stageM10(s); break; default: stageM6(s); break; }
}
static void missionUpdate(float dt) {
    switch (G.mission) { case 0: updateM1(dt); break; case 1: updateM2(dt); break; case 2: updateM3(dt); break; case 3: updateM4(dt); break; case 4: updateM5(dt); break; case 6: updateM7(dt); break; case 7: updateM8(dt); break; case 8: updateM9(dt); break; case 9: updateM10(dt); break; default: updateM6(dt); break; }
}
static void resetLevel() {
    G.box.clear(); G.cover.clear(); G.grass.clear(); G.act.clear(); G.veh.clear(); G.gren.clear(); G.bul.clear();
    G.prisms.clear(); G.mtri.clear(); G.mgrp.clear(); G.props.clear(); G.plumes.clear(); G.lawn.clear(); g_lawnDen = 0; G.war = false; G.warT = 3; g_levelSerial++; G.parts.clear(); G.smoke.clear(); G.tracers.clear(); G.hits.clear(); G.subs.clear(); G.story.clear();
    G.P = Player();
    for (int i = 0; i < 24; i++) { G.fl[i] = 0; G.iv[i] = 0; }
    G.hasGround = true; G.groundY = 0; G.sea = false; G.rain = false; G.lightning = false; G.roll = 0; G.tilt = 0; G.waterOn = false; G.waterY = -100;
    G.objective.clear(); G.objT = 0; G.wpOn = false; G.timerOn = false; G.progOn = false; G.progress = 0;
    G.stealth = false; G.detected = false; G.kills = G.shots = G.hitsN = G.heads = 0;
    G.callContact = G.callReload = G.callKill = G.callNade = 0;
    G.failed = false; G.failMsg.clear(); G.missionDone = false; G.endT = -1; G.storyNext = -1; G.storyDur = 0; G.useHint.clear(); G.missionT = 0; G.stageT = 0;
    G.lightT = 4; G.fadeIn = 1; G.snow = false; G.warn.clear(); G.warnT = 0;
    g_scopeHigh = false; g_wind = 0; g_enemyUni = U_SYND;
    g_amb.ring = g_amb.rain = g_amb.wind = g_amb.breath = g_amb.heli = g_amb.heart = 0; g_amb.echoAmt = 0.3f;
}
static void startMission(int m, int stage) {
    resetLevel();
    G.mission = m;
    missionBuild(m);
    V3 p; float yaw; stageSpawnPos(m, stage, p, yaw);
    G.P.pos = p; G.P.yaw = yaw;
    if (G.P.tank) { G.P.hull = G.P.turret = yaw; G.P.eyeH = 2.3f; }
    if (m == 3) { G.P.stance = 2; G.P.eyeH = 0.35f; }
    if (m == 3 && stage >= 5) { G.stealth = false; G.detected = true; G.timerOn = false; }
    if (m == 3 && stage == 6) { G.timerOn = false; }
    if (m == 4 && stage >= 1) { G.fl[1] = 110; G.progress = 0.45f; G.iv[0] = 3; G.iv[1] = 1; }
    if (m == 7 && stage >= 1) { G.iv[0] = G.iv[1] = G.iv[2] = 1; G.iv[5] = 3; }
    if (m == 1 && stage >= 1) for (auto& a : G.act) if (a.kind == AK_ALLY) a.hidden = false;
    for (auto& b : G.box) {
        float area = (b.mx.x - b.mn.x) * (b.mx.z - b.mn.z);
        if (!b.solid && b.tex == TX_NONE && (b.mx.y - b.mn.y) < 0.06f && area > 20 && !b.glow) b.tex = TX_ASPHALT;
    }
    placeAllies();
    if (m == 8 && stage >= 1) m9Place(stage - 1);
    if (m == 9 && stage >= 2 && stage <= 3) { G.timerOn = true; G.timer = 150; G.timerLabel = "До подрыва"; G.progOn = true; G.progLabel = "ВСКРЫТИЕ"; }
    buildCover();
    buildMinimap();
    scatterLawn();
    G.cpStage = stage;
    setStage(stage);
    if (m == 4 && stage == 1) setStage(2);
    G.screen = (m == 9 && stage == 3) ? SCR_DEFUSE : SCR_PLAY;
}

// =====================================================================
//  СОХРАНЕНИЕ
// =====================================================================
static std::string g_prefPath;
static void saveAll() {
    if (g_prefPath.empty()) return;
    std::string fn = g_prefPath + "save.txt";
    SDL_RWops* f = SDL_RWFromFile(fn.c_str(), "w");
    if (!f) return;
    std::string s = fmt("unlocked %d\nbestkh %.2f\ncpm %d\ncps %d\ndone %d\nsens %.2f\npsens %.2f\ninv %d\nvol %d\nvib %d\nqual %d\ndiff %d\nfps %d\nrend %d\nassist %d\n",
                        PR.unlocked, PR.bestKH, PR.cpMission, PR.cpStage, PR.done ? 1 : 0, S.lookSens, S.padSens, S.invertY ? 1 : 0,
                        S.volume, S.vibration ? 1 : 0, S.quality, S.difficulty, S.showFps ? 1 : 0, S.renderer, S.aimAssist ? 1 : 0);
    SDL_RWwrite(f, s.data(), 1, s.size());
    SDL_RWclose(f);
}
static void loadAll() {
    if (g_prefPath.empty()) return;
    std::string fn = g_prefPath + "save.txt";
    SDL_RWops* f = SDL_RWFromFile(fn.c_str(), "r");
    if (!f) return;
    Sint64 sz = SDL_RWsize(f);
    if (sz <= 0 || sz > 65536) { SDL_RWclose(f); return; }
    std::string buf((size_t)sz, '\0');
    size_t rd = SDL_RWread(f, &buf[0], 1, (size_t)sz);
    SDL_RWclose(f);
    buf.resize(rd);
    size_t pos = 0;
    while (pos < buf.size()) {
        size_t e = buf.find('\n', pos);
        if (e == std::string::npos) e = buf.size();
        std::string line = buf.substr(pos, e - pos);
        pos = e + 1;
        char key[32] = {0}; float v = 0;
        if (sscanf(line.c_str(), "%31s %f", key, &v) != 2) continue;
        std::string k = key;
        if (k == "unlocked") PR.unlocked = clampi((int)v, 1, NCH);
        else if (k == "bestkh") PR.bestKH = std::max(0.0f, v);
        else if (k == "cpm") PR.cpMission = clampi((int)v, -1, NCH - 1);
        else if (k == "cps") PR.cpStage = clampi((int)v, 0, 9);
        else if (k == "done") PR.done = v > 0.5f;
        else if (k == "sens") S.lookSens = clampf(v, 0.2f, 3.0f);
        else if (k == "psens") S.padSens = clampf(v, 0.2f, 3.0f);
        else if (k == "inv") S.invertY = v > 0.5f;
        else if (k == "assist") S.aimAssist = v > 0.5f;
        else if (k == "vol") S.volume = clampi((int)v, 0, 10);
        else if (k == "vib") S.vibration = v > 0.5f;
        else if (k == "qual") S.quality = clampi((int)v, 0, 2);
        else if (k == "diff") S.difficulty = clampi((int)v, 0, 2);
        else if (k == "fps") S.showFps = v > 0.5f;
        else if (k == "rend") S.renderer = clampi((int)v, 0, 1);
    }
}

// =====================================================================
//  СЕНСОРНОЕ УПРАВЛЕНИЕ
// =====================================================================
enum TBtn { B_FIRE, B_ADS, B_RELOAD, B_STANCE, B_NADE, B_TAC, B_TACCYC, B_SWITCH, B_LEANL, B_LEANR, B_BREATH, B_USE, B_PAUSE, B_COUNT };
struct TouchBtn { float x, y, r; const char* label; bool vis, held, pressed, released; float downT; };
static TouchBtn g_btn[B_COUNT] = {
    {565, 250, 34, "ОГОНЬ", true, false, false, false, 0}, {600, 165, 24, "ПРИЦЕЛ", true, false, false, false, 0},
    {488, 305, 22, "ПЕРЕЗ.", true, false, false, false, 0}, {596, 322, 22, "ПРИСЕД", true, false, false, false, 0},
    {498, 238, 22, "ГРАН.", true, false, false, false, 0}, {452, 278, 20, "ТАКТ.", true, false, false, false, 0},
    {540, 330, 19, "ЛЕЧЬ", true, false, false, false, 0}, {528, 168, 22, "СМЕНА", true, false, false, false, 0},
    {540, 100, 19, "НАКЛ.Л", true, false, false, false, 0}, {600, 100, 19, "НАКЛ.П", true, false, false, false, 0},
    {450, 175, 22, "ДЫХАН.", false, false, false, false, 0}, {380, 300, 26, "ДЕЙСТВ.", false, false, false, false, 0},
    {474, 24, 19, "II", true, false, false, false, 0},
};
struct Finger { SDL_FingerID id; int role; float sx, sy, x, y, px, py; float t; bool on; int btn; };
static Finger g_fin[10];
static bool g_touchAds = false;
static float g_stickX = 0, g_stickY = 0, g_stickBX = 0, g_stickBY = 0; static bool g_stickOn = false;
static float g_tapX = -1, g_tapY = -1;
static float g_touchLookX = 0, g_touchLookY = 0;
static int g_winW = 1280, g_winH = 720;

static void toUI(float nx, float ny, float& ux, float& uy) {
    float wx = nx * g_winW, wy = ny * g_winH;
    ux = (wx - g_dst.x) * (float)UIW / std::max(1, g_dst.w);
    uy = (wy - g_dst.y) * (float)UIH / std::max(1, g_dst.h);
}
static void touchDown(SDL_FingerID id, float ux, float uy) {
    g_lastDev = 0;
    int slot = -1;
    for (int i = 0; i < 10; i++) if (!g_fin[i].on) { slot = i; break; }
    if (slot < 0) return;
    Finger& f = g_fin[slot];
    f.id = id; f.on = true; f.sx = f.x = f.px = ux; f.sy = f.y = f.py = uy; f.t = 0; f.btn = -1; f.role = -1;
    if (G.screen != SCR_PLAY) { f.role = 3; return; }
    for (int b = 0; b < B_COUNT; b++) {
        TouchBtn& B = g_btn[b];
        if (!B.vis) continue;
        float dx = ux - B.x, dy = uy - B.y;
        if (dx * dx + dy * dy <= (B.r + 6) * (B.r + 6)) { f.role = 2; f.btn = b; B.held = true; B.pressed = true; B.downT = 0; return; }
    }
    if (ux < UIW * 0.42f && uy > UIH * 0.25f && !g_stickOn) { f.role = 0; g_stickOn = true; g_stickBX = ux; g_stickBY = uy; g_stickX = g_stickY = 0; return; }
    f.role = 1;
}
static void touchMove(SDL_FingerID id, float ux, float uy) {
    for (auto& f : g_fin) {
        if (!f.on || f.id != id) continue;
        f.px = f.x; f.py = f.y; f.x = ux; f.y = uy;
        if (f.role == 0) {
            float dx = ux - g_stickBX, dy = uy - g_stickBY, L = sqrtf(dx * dx + dy * dy), R = 45;
            if (L > R * 1.6f) { g_stickBX = ux - dx / L * R * 1.6f; g_stickBY = uy - dy / L * R * 1.6f; }
            g_stickX = clampf(dx / R, -1.6f, 1.6f); g_stickY = clampf(dy / R, -1.6f, 1.6f);
        } else if (f.role == 1 || (f.role == 2 && f.btn == B_FIRE)) {
            g_touchLookX += (ux - f.px); g_touchLookY += (uy - f.py);
        }
    }
}
static void touchUp(SDL_FingerID id, float ux, float uy) {
    for (auto& f : g_fin) {
        if (!f.on || f.id != id) continue;
        f.on = false;
        if (f.role == 0) { g_stickOn = false; g_stickX = g_stickY = 0; }
        if (f.role == 2 && f.btn >= 0) { g_btn[f.btn].held = false; g_btn[f.btn].released = true; }
        if (f.role == 3 && fabsf(ux - f.sx) < 14 && fabsf(uy - f.sy) < 14) { g_tapX = ux; g_tapY = uy; }
    }
}
static void touchFrame(float dt) {
    if (G.screen != SCR_PLAY) {
        for (auto& b : g_btn) { b.held = false; b.pressed = b.released = false; }
        return;
    }
    const WState& W = G.P.w[G.P.slot];
    {
        static float baseX[B_COUNT]; static bool init = false;
        if (!init) { for (int b = 0; b < B_COUNT; b++) baseX[b] = g_btn[b].x; init = true; }
        for (int b = 0; b < B_COUNT; b++) g_btn[b].x = baseX[b] + (baseX[b] > 320 ? (float)(UIW - 640) : 0.0f);   // к правому краю экрана
        bool tk = G.P.tank;
        int hideInTank[5] = {B_RELOAD, B_STANCE, B_NADE, B_TAC, B_TACCYC};
        for (int b : hideInTank) g_btn[b].vis = !tk;
        g_btn[B_LEANL].vis = g_btn[B_LEANR].vis = false;   // наклоны убраны с экрана (остались на клавиатуре и геймпаде)
        g_btn[B_SWITCH].label = tk ? "СНАРЯД" : "СМЕНА";
    }
    g_btn[B_BREATH].vis = W.def == W_SNIPER && G.P.adsT > 0.5f && !G.P.tank;
    g_btn[B_USE].vis = !G.useHint.empty();
    for (auto& b : g_btn) if (b.held) b.downT += dt;
    float mx = g_stickX, my = -g_stickY;
    float L = sqrtf(mx * mx + my * my);
    if (L > 0.12f || g_lastDev == 0) {
        if (L > 1) { IN.mx = mx / L; IN.my = my / L; } else if (L > 0.12f) { IN.mx = mx; IN.my = my; }
        if (L > 1.25f && my > 0.8f) IN.sprint = true;
    }
    IN.lookX += g_touchLookX * 0.24f * S.lookSens;
    IN.lookY += g_touchLookY * 0.24f * S.lookSens;
    g_touchLookX = g_touchLookY = 0;
    if (g_btn[B_FIRE].held) IN.fire = true;
    if (g_btn[B_ADS].pressed) g_touchAds = !g_touchAds;
    if (g_touchAds) IN.ads = true;
    if (g_btn[B_RELOAD].pressed) IN.reload = true;
    if (g_btn[B_STANCE].released && g_btn[B_STANCE].downT < 0.4f) IN.stance = true;
    if (g_btn[B_STANCE].held && g_btn[B_STANCE].downT >= 0.4f && g_btn[B_STANCE].downT - dt < 0.4f) IN.prone = true;
    if (g_btn[B_NADE].held) IN.nadeHeld = true;
    if (g_btn[B_TAC].held) IN.tacHeld = true;
    if (g_btn[B_TACCYC].pressed) IN.prone = true;   // кнопка «ЛЕЧЬ» (вместо выбора тактической — та теперь меняется сама)
    if (g_btn[B_SWITCH].pressed) IN.sw = true;
    if (g_btn[B_LEANL].pressed) IN.leanL = true;
    if (g_btn[B_LEANR].pressed) IN.leanR = true;
    if (g_btn[B_BREATH].held) IN.breath = true;
    if (g_btn[B_USE].held) IN.useHeld = true;
    if (g_btn[B_USE].pressed) IN.use = true;
    if (g_btn[B_PAUSE].pressed) IN.pause = true;
    for (auto& b : g_btn) { b.pressed = false; b.released = false; }
}
static void drawTouch() {
    if (g_lastDev != 0 || G.screen != SCR_PLAY) return;
    for (int i = 0; i < B_COUNT; i++) {
        TouchBtn& b = g_btn[i];
        if (!b.vis) continue;
        bool on = b.held || (i == B_ADS && g_touchAds);
        uiCircle((int)b.x, (int)b.y, (int)b.r, on ? ARGBc(110, 255, 210, 80) : ARGBc(60, 20, 20, 20));
        uiRing((int)b.x, (int)b.y, (int)b.r, 2, ARGBc(150, 230, 230, 220));
        uiTextC((int)b.x, (int)b.y - 3, b.label, ARGBc(220, 255, 255, 255));
    }
    int tx = (int)g_btn[B_TAC].x;
    if (g_btn[B_TAC].vis) uiTextC(tx, (int)g_btn[B_TAC].y + 7, G.P.tac == 1 ? "СВЕТ" : "ДЫМ", ARGBc(200, 255, 220, 120));
    if (g_stickOn) {
        uiRing((int)g_stickBX, (int)g_stickBY, 45, 2, ARGBc(120, 230, 230, 220));
        float L = sqrtf(g_stickX * g_stickX + g_stickY * g_stickY), k = L > 1 ? 1 / L : 1;
        uiCircle((int)(g_stickBX + g_stickX * k * 45), (int)(g_stickBY + g_stickY * k * 45), 16, ARGBc(150, 230, 230, 220));
        if (IN.sprint) uiTextC((int)g_stickBX, (int)g_stickBY - 60, "БЕГ", ARGBc(220, 255, 220, 120));
    } else {
        uiRing(90, 280, 45, 1, ARGBc(70, 230, 230, 220));
        uiTextC(90, 277, "ДВИЖ.", ARGBc(90, 255, 255, 255));
    }
}

// =====================================================================
//  HUD
// =====================================================================
static bool projUI(V3 p, float& ux, float& uy) {
    V3 c = toCam(p);
    if (c.z < 0.1f) return false;
    float sx = g_cam.cx + c.x * g_cam.focal / c.z, sy = g_cam.cy - c.y * g_cam.focal / c.z;
    ux = sx * UIW / RW; uy = sy * UIH / RH;
    return true;
}
static const char* kDirNames[8] = {"С", "СВ", "В", "ЮВ", "Ю", "ЮЗ", "З", "СЗ"};
static void drawCompass() {
    int cx = UIW / 2, y = 8, w = 260;
    uiRect(cx - w / 2, y, w, 14, ARGBc(110, 0, 0, 0));
    float yaw = G.P.yaw;
    for (int d = 0; d < 360; d += 15) {
        float a = wrapPi(d * DEG - yaw);
        if (fabsf(a) > PI_F / 2) continue;
        int x = cx + (int)(a / (PI_F / 2) * (w / 2));
        if (d % 45 == 0) uiTextC(x, y + 4, kDirNames[d / 45], d == 0 ? ARGBc(255, 255, 90, 80) : ARGBc(230, 230, 230, 230));
        else uiRect(x, y + 9, 1, 4, ARGBc(160, 230, 230, 230));
    }
    auto mark = [&](V3 p, uint32_t col, int size) {
        float a = wrapPi(yawTo(G.P.pos, p) - yaw);
        if (fabsf(a) > PI_F / 2) return;
        int x = cx + (int)(a / (PI_F / 2) * (w / 2));
        uiRect(x - size / 2, y + 15, size, size, col);
    };
    for (auto& a : G.act) {
        if (!a.alive || a.hidden) continue;
        if (a.kind == AK_ALLY) mark(a.pos, ARGBc(230, 90, 230, 90), 3);
        else if (a.team == 1 && (a.kind == AK_SOLDIER || a.kind == AK_DOG) && a.muzzle > -1.5f && a.alerted && a.sees && !G.stealth) mark(a.pos, ARGBc(230, 240, 60, 50), 3);
    }
    if (G.wpOn) mark(G.wp, ARGBc(255, 255, 210, 40), 5);
    uiRect(cx, y - 2, 1, 3, ARGBc(255, 255, 255, 255));
}
static void drawTankHUD() {
    const Player& P = G.P;
    int cx = UIW / 2, cy = UIH / 2;
    uint32_t c = ARGBc(230, 120, 255, 140);
    if (P.adsT > 0.5f) {
        uint32_t k = ARGBc(255, 20, 20, 20);
        uiRect(cx - 150, cy, 140, 1, k); uiRect(cx + 10, cy, 140, 1, k); uiRect(cx, cy + 10, 1, 140, k);
        for (int m = 1; m <= 6; m++) { int y = cy + m * 14; uiRect(cx - 6 + (m & 1) * 2, y, 13 - (m & 1) * 4, 1, k); }
        uiFrame(cx - 4, cy - 4, 9, 9, k);
        uiText(cx + 60, cy + 90, "ПРИЦЕЛ НАВОДЧИКА", ARGBc(200, 30, 30, 30), 1, false);
    } else {
        uiRing(cx, cy, 9, 1, c);
        uiRect(cx - 18, cy, 8, 1, c); uiRect(cx + 11, cy, 8, 1, c); uiRect(cx, cy + 11, 1, 8, c);
        float ty = wrapPi(P.turret - P.yaw);
        int ox = cx + (int)(sinf(ty) * 40);
        uiRect(ox - 1, cy - 1, 3, 3, ARGBc(200, 255, 220, 120));
    }
    if (P.hitMarker > 0) uiTextC(cx, cy - 26, "ПОПАДАНИЕ", ARGBc(230, 255, 230, 120));
    int bx = UIW - 14, by = g_lastDev == 0 ? 12 : UIH - 70;
    if (g_lastDev != 0) by -= 30;
    uiTextR(bx, by, "ОРУДИЕ 125 ММ", ARGBc(220, 230, 230, 230));
    // два типа снарядов: выбранный — в рамке
    for (int k = 0; k < 2; k++) {
        int w = 62, x = bx - (2 - k) * (w + 4) + 4, y = by + 11;
        bool sel = P.shell == k;
        uiRect(x, y, w, 20, sel ? ARGBc(170, 60, 50, 20) : ARGBc(120, 0, 0, 0));
        if (sel) uiFrame(x - 1, y - 1, w + 2, 22, ARGBc(255, 255, 210, 60));
        uint32_t tc = P.shells[k] > 0 ? (sel ? ARGBc(255, 255, 230, 140) : ARGBc(220, 200, 200, 200)) : ARGBc(200, 200, 70, 60);
        uiText(x + 4, y + 3, kShellName[k], tc);
        uiTextR(x + w - 4, y + 3, fmt("%d", P.shells[k]).c_str(), tc);
        uiText(x + 4, y + 12, k == 0 ? "ПО ТАНКАМ" : "ПО ПЕХОТЕ", ARGBc(170, 180, 180, 180));
    }
    const char* key = g_lastDev == 1 ? "[Y]" : (g_lastDev == 0 ? "" : "[1/2]");
    if (*key) uiTextR(bx - 134, by + 17, key, ARGBc(200, 200, 200, 200));
    int ry = by + 36;
    if (P.shells[P.loaded] <= 0 && P.shells[1 - P.loaded] <= 0) uiTextR(bx, ry, "БОЕКОМПЛЕКТ ИЗРАСХОДОВАН", ARGBc(240, 255, 80, 60));
    else if (P.treload > 0) {
        float k = 1 - P.treload / 3.5f;
        uiRect(bx - 128, ry, 128, 4, ARGBc(130, 0, 0, 0)); uiRect(bx - 128, ry, (int)(128 * k), 4, ARGBc(230, 255, 200, 80));
        uiTextR(bx, ry + 7, fmt("ЗАРЯЖАНИЕ: %s", kShellName[P.loaded]).c_str(), ARGBc(220, 255, 220, 140));
    } else uiTextR(bx, ry, fmt("ЗАРЯЖЕН: %s", kShellFull[P.loaded]).c_str(), ARGBc(240, 120, 255, 140));
    uiTextR(bx, ry + 18, fmt("%d КМ/Ч", (int)(fabsf(P.tspeed) * 3.6f)).c_str(), ARGBc(220, 220, 220, 220));
    int hx = g_lastDev == 0 ? 160 : 20, hy = UIH - 34;
    uiText(hx, hy - 10, "КОРПУС", ARGBc(220, 230, 230, 230));
    uiRect(hx, hy, 90, 6, ARGBc(130, 0, 0, 0));
    uiRect(hx, hy, (int)(90 * P.hp / 100.0f), 6, P.hp < 35 ? ARGBc(230, 255, 70, 50) : ARGBc(230, 120, 220, 120));
    // схема: корпус и башня
    int sx = hx + 110, sy = hy - 6;
    float hr = wrapPi(P.hull - P.yaw), tr = wrapPi(P.turret - P.yaw);
    for (int t = -8; t <= 8; t++) uiPix(sx + (int)(sinf(hr) * t), sy - (int)(cosf(hr) * t), ARGBc(230, 200, 200, 200));
    for (int t = -4; t <= 4; t++) uiPix(sx + 1 + (int)(sinf(hr) * t), sy - (int)(cosf(hr) * t), ARGBc(230, 200, 200, 200));
    for (int t = 0; t <= 12; t++) uiPix(sx + (int)(sinf(tr) * t), sy - (int)(cosf(tr) * t), ARGBc(240, 255, 220, 120));
}
static void drawCrosshair() {
    const Player& P = G.P;
    if (P.tank) { drawTankHUD(); return; }
    int cx = UIW / 2, cy = UIH / 2;
    const WState& W = P.w[P.slot];
    if (P.dead || scopeView()) return;
    if (P.adsT < 0.5f && !P.sprint && W.def >= 0) {
        const WeaponDef& D = WD[W.def];
        float stM = P.stance == 0 ? 1.0f : (P.stance == 1 ? 0.75f : 0.55f);
        float sp = (D.hip + D.moveAdd * std::min(1.0f, P.speed / 4.0f) + P.bloom) * stM;
        if (D.pellets > 1) sp = 4.5f;
        int r = (int)(tanf(sp * DEG) * (UIW * 0.5f) / tanf(curFov() * DEG * 0.5f)) + 3;
        uint32_t c = ARGBc(200, 255, 255, 255);
        uiRect(cx - r - 6, cy, 6, 1, c); uiRect(cx + r + 1, cy, 6, 1, c); uiRect(cx, cy - r - 6, 1, 6, c); uiRect(cx, cy + r + 1, 1, 6, c);
    }
    if (P.adsT < 0.9f) uiRect(cx - 1, cy - 1, 2, 2, ARGBc(230, 255, 255, 255));
    if (P.hitMarker > 0) {
        uint32_t c = ARGBc((int)(255 * std::min(1.0f, P.hitMarker * 5)), 255, 255, 255);
        for (int k = 3; k < 8; k++) { uiPix(cx - k, cy - k, c); uiPix(cx + k, cy - k, c); uiPix(cx - k, cy + k, c); uiPix(cx + k, cy + k, c); }
    }
}
static void drawScope() {
    if (!scopeView()) return;
    int cx = UIW / 2, cy = UIH / 2;
    uint32_t k = ARGBc(255, 10, 10, 10);
    uiRect(cx - 170, cy, 155, 1, k); uiRect(cx + 15, cy, 155, 1, k); uiRect(cx, cy - 170, 1, 155, k); uiRect(cx, cy + 15, 1, 155, k);
    uiRect(cx - 170, cy - 1, 120, 3, k); uiRect(cx + 50, cy - 1, 120, 3, k); uiRect(cx - 1, cy + 50, 3, 120, k);
    float pxPerMil = (UIW * 0.5f) / tanf(curFov() * DEG * 0.5f) * 0.001f;
    for (int m = 1; m <= 12; m++) {
        int yy = cy + (int)(m * pxPerMil);
        if (yy - cy > 165) break;
        int w = (m % 5 == 0) ? 7 : 3;
        uiRect(cx - w, yy, 2 * w + 1, 1, k);
        if (m % 2 == 0 && pxPerMil > 6) uiText(cx + 10, yy - 3, fmt("%d", m).c_str(), ARGBc(220, 20, 20, 20), 1, false);
    }
    for (int m = -8; m <= 8; m++) {
        if (m == 0) continue;
        int xx = cx + (int)(m * pxPerMil);
        if (abs(xx - cx) > 165) continue;
        int h = (m % 5 == 0) ? 6 : 3;
        uiRect(xx, cy - h, 1, 2 * h + 1, k);
    }
    const Player& P = G.P;
    uiRect(cx - 40, cy - 122, 80, 5, ARGBc(150, 0, 0, 0));
    uiRect(cx - 40, cy - 122, (int)(80 * P.breath), 5, P.breathLock > 0 ? ARGBc(220, 220, 60, 50) : ARGBc(220, 120, 200, 255));
    uiTextC(cx, cy - 114, P.holding ? "ДЫХАНИЕ ЗАДЕРЖАНО" : (g_lastDev == 1 ? "L3 — ЗАДЕРЖАТЬ ДЫХАНИЕ" : (g_lastDev == 0 ? "ДЫХАН. — ЗАДЕРЖАТЬ" : "SHIFT — ЗАДЕРЖАТЬ ДЫХАНИЕ")), ARGBc(220, 230, 230, 230));
    if (g_scopeHigh) uiText(cx + 70, cy + 96, "КРАТНОСТЬ 20X", ARGBc(200, 230, 230, 230));
}

static void drawMinimap() {
    const Player& P = G.P;
    const int cx = 54, cy = 54, R = 46;
    float range = P.tank ? 95.0f : 55.0f;                   // метров до края
    float s = range / R;
    float fx = sinf(P.yaw), fz = cosf(P.yaw), rx = cosf(P.yaw), rz = -sinf(P.yaw);
    static const uint32_t pal[5] = {0xFF2C3530u, 0xFF4A4C4Au, 0xFF8A7A58u, 0xFFB8BEC2u, 0xFF2E5A7Au};
    int R2 = R * R;
    for (int j = -R; j <= R; j++) {
        int y = cy + j;
        for (int i = -R; i <= R; i++) {
            int d2 = i * i + j * j;
            if (d2 > R2) continue;
            float wx = P.pos.x + (rx * i - fx * j) * s, wz = P.pos.z + (rz * i - fz * j) * s;
            int t = g_mmW > 0 ? mmAt(wx, wz) : 0;
            uint32_t c = t < 0 ? 0xFF161A1Cu : pal[t];
            if (t == 3) {   // контур зданий
                float ox = rx * s, oz = rz * s, px = -fx * s, pz = -fz * s;
                if (mmAt(wx + ox, wz + oz) != 3 || mmAt(wx - ox, wz - oz) != 3 || mmAt(wx + px, wz + pz) != 3 || mmAt(wx - px, wz - pz) != 3) c = 0xFFDCE2E6u;
            }
            uiPix(cx + i, y, withA(c, d2 > (R - 3) * (R - 3) ? 230 : 200));
        }
    }
    uiRing(cx, cy, R + 1, 2, ARGBc(220, 20, 22, 24));
    uiRing(cx, cy, R, 1, ARGBc(160, 200, 205, 210));
    auto toMap = [&](V3 p, int& mx, int& my, bool clampEdge) -> bool {
        float dx = p.x - P.pos.x, dz = p.z - P.pos.z;
        float u = (dx * rx + dz * rz) / s, v = -(dx * fx + dz * fz) / s;
        float L = sqrtf(u * u + v * v);
        if (L > R - 3) { if (!clampEdge) return false; u *= (R - 3) / L; v *= (R - 3) / L; }
        mx = cx + (int)lrintf(u); my = cy + (int)lrintf(v);
        return true;
    };
    int mx, my;
    // север
    if (toMap(P.pos + V3(0, 0, 1000), mx, my, true)) uiTextC(mx, my - 3, "С", ARGBc(255, 255, 90, 80));
    // техника
    for (const Vehicle& v : G.veh) {
        if (!v.active || v.kind == VK_JET) continue;
        if (!toMap(v.pos, mx, my, false)) continue;
        uint32_t c = !v.alive ? ARGBc(200, 90, 90, 90) : (v.team == 0 ? ARGBc(240, 90, 220, 90) : ARGBc(240, 255, 70, 50));
        int sz = v.kind == VK_TANK || v.kind == VK_HELI ? 3 : 2;
        uiRect(mx - sz, my - sz, sz * 2 + 1, sz * 2 + 1, c);
        if (v.kind == VK_HELI && v.alive) uiRing(mx, my, 5, 1, c);
    }
    // люди
    bool scout = G.stealth && !G.detected;
    for (const Actor& a : G.act) {
        if (!a.alive || a.hidden || a.state == AS_CARRIED) continue;
        uint32_t c = 0;
        if (a.kind == AK_ALLY) c = ARGBc(255, 90, 230, 90);
        else if (a.kind == AK_CONTACT) c = ARGBc(255, 255, 230, 90);
        else if (a.team == 1 && (a.kind == AK_SOLDIER || a.kind == AK_DOG)) {
            if (scout) { if (dist2D(a.pos, P.pos) < 45) c = a.alert > 0.45f ? ARGBc(255, 255, 70, 50) : ARGBc(220, 255, 160, 40); }
            else if (a.alerted && (a.sees || a.muzzle > 0 || a.lastSeen < 0.5f)) c = ARGBc(255, 255, 60, 50);
        }
        if (!c || !toMap(a.pos, mx, my, false)) continue;
        uiRect(mx - 1, my - 1, 3, 3, c);
    }
    // гранаты рядом
    for (const Grenade& g : G.gren)
        if (g.type == 0 && toMap(g.p, mx, my, false) && ((int)(G.time * 8) & 1)) uiRect(mx - 1, my - 1, 3, 3, ARGBc(255, 255, 255, 255));
    // цель задания
    if (G.wpOn && toMap(G.wp, mx, my, true))
        for (int k = 0; k < 4; k++) { uiRect(mx - k, my - 3 + k, 2 * k + 1, 1, ARGBc(255, 255, 210, 40)); uiRect(mx - k, my + 3 - k, 2 * k + 1, 1, ARGBc(255, 255, 210, 40)); }
    // игрок: стрелка вверх
    for (int k = 0; k < 6; k++) uiRect(cx - k / 2, cy - 4 + k, k / 2 * 2 + 1, 1, ARGBc(255, 255, 255, 255));
    if (P.tank) { float tr = wrapPi(P.turret - P.yaw); for (int t = 2; t <= 9; t++) uiPix(cx + (int)lrintf(sinf(tr) * t), cy - (int)lrintf(cosf(tr) * t), ARGBc(255, 255, 220, 120)); }
    uiTextC(cx, cy + R + 4, fmt("%d М", (int)range).c_str(), ARGBc(170, 200, 200, 200));
}
static void drawHUD() {
    Player& P = G.P;
    drawCompass();
    if (!scopeView()) drawMinimap();
    // таймер и прогресс
    int ty = 34;
    if (G.timerOn) {
        float t = std::max(0.0f, G.timer);
        std::string s = G.iv[3] ? fmt("%s: %.1f", G.timerLabel.c_str(), t) : fmt("%s: %d:%02d", G.timerLabel.c_str(), (int)t / 60, (int)t % 60);
        uint32_t c = (!G.iv[3] && t < 20 && ((int)(G.time * 3) & 1)) ? ARGBc(255, 255, 70, 60) : ARGBc(240, 255, 230, 120);
        uiTextC(UIW / 2, ty, s.c_str(), c, 2); ty += 20;
    }
    if (G.progOn) {
        int w = 200; int x = UIW / 2 - w / 2;
        uiRect(x, ty, w, 8, ARGBc(140, 0, 0, 0)); uiRect(x, ty, (int)(w * clampf(G.progress, 0, 1)), 8, ARGBc(220, 90, 200, 255));
        uiFrame(x - 1, ty - 1, w + 2, 10, ARGBc(160, 255, 255, 255));
        uiTextC(UIW / 2, ty + 11, fmt("%s %d%%", G.progLabel.c_str(), (int)(G.progress * 100)).c_str(), ARGBc(230, 230, 240, 255));
        ty += 24;
    }
    if (G.stealth && !G.detected && g_stealthMax > 0.02f) {
        int w = 120, x = UIW / 2 - w / 2;
        uint32_t c = g_stealthMax > 0.7f ? ARGBc(230, 255, 60, 40) : ARGBc(220, 255, 180, 40);
        uiRect(x, ty, w, 5, ARGBc(120, 0, 0, 0)); uiRect(x, ty, (int)(w * clampf(g_stealthMax, 0, 1)), 5, c);
        uiTextC(UIW / 2, ty + 8, "ОБНАРУЖЕНИЕ", c); ty += 20;
        for (auto& a : G.act) {
            if (!a.alive || a.alert < 0.05f) continue;
            float ang = wrapPi(yawTo(P.pos, a.pos) - P.yaw);
            uiArc(UIW / 2, UIH / 2, 60, ang - 0.25f, ang + 0.25f, 3, ARGBc((int)(80 + 175 * clampf(a.alert, 0, 1)), 255, 170, 30));
        }
    }
    // задача
    if (G.objT > 0 && !G.objective.empty()) {
        int a = (int)(255 * clampf(G.objT, 0, 1));
        uiText(10, 116, "ЗАДАЧА:", ARGBc(a, 255, 210, 60));
        uiText(10, 126, G.objective.c_str(), ARGBc(a, 255, 255, 255));
    }
    // маркер точки
    if (G.wpOn && !scopeView()) {
        float ux, uy;
        V3 wp = G.wp + V3(0, 1.2f, 0);
        if (projUI(wp, ux, uy)) {
            ux = clampf(ux, 20, UIW - 20); uy = clampf(uy, 40, UIH - 40);
            int x = (int)ux, y = (int)uy;
            for (int k = 0; k < 6; k++) { uiRect(x - k, y - 6 + k, 2 * k + 1, 1, ARGBc(220, 255, 210, 40)); uiRect(x - k, y + 6 - k, 2 * k + 1, 1, ARGBc(220, 255, 210, 40)); }
            uiTextC(x, y + 9, fmt("%d м", (int)len(G.wp - P.pos)).c_str(), ARGBc(220, 255, 230, 120));
        }
    }
    drawCrosshair();
    drawScope();
    // индикаторы попаданий
    for (auto& h : G.hits) {
        float a = wrapPi(h.yaw - P.yaw);
        int al = (int)(220 * clampf(h.t, 0, 1));
        uiArc(UIW / 2, UIH / 2, 80, a - 0.32f, a + 0.32f, 5, ARGBc(al, 220, 30, 20));
    }
    // индикатор гранаты
    for (auto& g : G.gren) {
        if (g.type != 0) continue;
        float d = len(g.p - P.pos);
        if (d > 9) continue;
        float a = wrapPi(yawTo(P.pos, g.p) - P.yaw);
        int x = UIW / 2 + (int)(sinf(a) * 100), y = UIH / 2 - (int)(cosf(a) * 100);
        bool blink = ((int)(G.time * 8) & 1) != 0;
        uiCircle(x, y, 9, ARGBc(200, 30, 30, 30));
        uiRing(x, y, 9, 2, blink ? ARGBc(255, 255, 60, 40) : ARGBc(255, 255, 200, 60));
        uiRect(x - 3, y - 4, 6, 8, ARGBc(255, 120, 140, 70)); uiRect(x - 1, y - 7, 2, 3, ARGBc(255, 160, 160, 160));
        int ax = UIW / 2 + (int)(sinf(a) * 120), ay = UIH / 2 - (int)(cosf(a) * 120);
        uiCircle(ax, ay, 3, ARGBc(230, 255, 60, 40));
    }
    // оружие и боеприпасы
    const WState& W = P.w[P.slot];
    if (!P.tank) {
    int bx = UIW - 14, by = UIH - 70;
    if (g_lastDev == 0) { bx = UIW - 12; by = 10; }
    if (W.def >= 0) {
        const WeaponDef& D = WD[W.def];
        uiTextR(bx, by, D.name, ARGBc(220, 230, 230, 230));
        std::string am = fmt("%d", W.mag);
        uint32_t mc = W.mag <= D.mag / 4 ? ARGBc(240, 255, 90, 70) : ARGBc(240, 255, 255, 255);
        uiTextR(bx - 50, by + 12, am.c_str(), mc, 3);
        uiTextR(bx, by + 19, fmt("/%d", W.reserve).c_str(), ARGBc(220, 200, 200, 200), 2);
        if (W.mag > D.mag && !D.shellReload) uiTextR(bx - 50, by + 36, "+1", ARGBc(200, 200, 230, 200));
        if (W.reloadT > 0) {
            float k = 1 - W.reloadT / std::max(0.01f, W.reloadDur);
            uiRect(bx - 100, by + 40, 100, 3, ARGBc(120, 0, 0, 0)); uiRect(bx - 100, by + 40, (int)(100 * k), 3, ARGBc(220, 255, 255, 255));
            uiTextR(bx, by + 46, W.reloadEmpty ? "ПЕРЕЗАРЯДКА (ПУСТОЙ)" : "ПЕРЕЗАРЯДКА", ARGBc(200, 255, 255, 255));
        } else if (W.mag == 0) uiTextR(bx, by + 46, W.reserve > 0 ? "ПЕРЕЗАРЯДИТЕСЬ" : "НЕТ ПАТРОНОВ", ARGBc(230, 255, 80, 60));
    }
    int gx = UIW - 14, gy = g_lastDev == 0 ? 68 : UIH - 86;
    std::string gs = fmt("ОСК %d   %sСВЕТ %d   %sДЫМ %d", P.nade[0], P.tac == 1 ? ">" : "", P.nade[1], P.tac == 2 ? ">" : "", P.nade[2]);
    uiTextR(gx, gy, gs.c_str(), ARGBc(220, 220, 230, 200));
    if (P.cookType == 0) uiTextC(UIW / 2, UIH / 2 + 30, fmt("ГРАНАТА: %.1f", std::max(0.0f, 4.0f - P.cookT)).c_str(), ARGBc(240, 255, 120, 80));
    }
    // стойка и выносливость
    if (!P.tank) {
    int sx = g_lastDev == 0 ? 160 : 20, sy = UIH - 40;
    uint32_t sc = ARGBc(200, 230, 230, 230);
    if (P.stance == 0) { uiRect(sx + 4, sy, 4, 4, sc); uiRect(sx + 3, sy + 5, 6, 10, sc); uiRect(sx + 3, sy + 15, 2, 10, sc); uiRect(sx + 7, sy + 15, 2, 10, sc); }
    else if (P.stance == 1) { uiRect(sx + 4, sy + 8, 4, 4, sc); uiRect(sx + 3, sy + 13, 6, 7, sc); uiRect(sx + 3, sy + 20, 8, 2, sc); uiRect(sx + 9, sy + 20, 2, 5, sc); }
    else { uiRect(sx, sy + 20, 4, 4, sc); uiRect(sx + 5, sy + 21, 16, 3, sc); }
    if (P.lean != 0) uiText(sx + 14, sy + 2, P.lean < 0 ? "<" : ">", sc);
    if (P.stamina < 0.99f) { uiRect(sx, sy + 28, 40, 3, ARGBc(120, 0, 0, 0)); uiRect(sx, sy + 28, (int)(40 * P.stamina), 3, P.sprintLock > 0 ? ARGBc(220, 255, 80, 60) : ARGBc(220, 230, 230, 120)); }
    }
    if (P.carry >= 0) uiTextC(UIW / 2, UIH - 110, G.iv[22] == 2 ? "ВЫ ТАЩИТЕ ДОКА" : "СОВА НА ПЛЕЧАХ", ARGBc(220, 255, 220, 140));
    // подсказка действия
    if (!G.useHint.empty()) {
        std::string h = g_lastDev == 1 ? "[A] " + G.useHint : (g_lastDev == 0 ? G.useHint : "[F] " + G.useHint);
        uiTextC(UIW / 2, UIH / 2 + 44, h.c_str(), ARGBc(240, 255, 255, 255));
        if (P.useT > 0) { uiRect(UIW / 2 - 40, UIH / 2 + 54, 80, 3, ARGBc(120, 0, 0, 0)); uiRect(UIW / 2 - 40, UIH / 2 + 54, (int)(80 * clampf(P.useT / 3.0f, 0, 1)), 3, ARGBc(230, 255, 255, 255)); }
    }
    if (G.warnT > 0) { G.warnT -= 1.0f / 30; if (((int)(G.time * 6)) & 1) uiTextC(UIW / 2, UIH / 2 - 60, G.warn.c_str(), ARGBc(255, 255, 80, 60), 2); }
    if (G.fl[20] > 0) uiTextC(UIW / 2, UIH - 130, "КОНТРОЛЬНАЯ ТОЧКА", ARGBc((int)(255 * clampf(G.fl[20], 0, 1)), 255, 255, 255));
    if (G.fl[21] > 0) uiTextC(UIW / 2, UIH - 140, "БОЕПРИПАСЫ ПОПОЛНЕНЫ", ARGBc((int)(255 * clampf(G.fl[21], 0, 1)), 200, 255, 200));
    // субтитры
    if (!G.subs.empty() && G.subs[0].t >= 0) {
        const Sub& s = G.subs[0];
        std::string full = s.who + ": " + s.text;
        auto lines = wrapText(full, 420);
        int h = (int)lines.size() * 10 + 6, y = UIH - 52 - h;
        uiRect(UIW / 2 - 216, y, 432, h, ARGBc(130, 0, 0, 0));
        for (size_t i = 0; i < lines.size(); i++) {
            if (i == 0) {
                int w = uiTextW(lines[0].c_str());
                int x = UIW / 2 - w / 2;
                std::string who = s.who + ":";
                uiText(x, y + 3, who.c_str(), withA(s.col, 255));
                std::string rest = lines[0].size() > who.size() + 1 ? lines[0].substr(who.size() + 1) : std::string();
                uiText(x + uiTextW(who.c_str()) + 6, y + 3, rest.c_str(), ARGBc(255, 240, 240, 240));
            } else uiTextC(UIW / 2, y + 3 + (int)i * 10, lines[i].c_str(), ARGBc(255, 240, 240, 240));
        }
        if (s.radio) uiText(UIW / 2 - 212, y - 9, "РАЦИЯ", ARGBc(200, 120, 255, 160));
    }
    if (S.showFps) uiText(UIW - 60, UIH - 10, fmt("FPS %d", (int)G.fps).c_str(), ARGBc(200, 120, 255, 120));
}
static void drawStory() {
    if (G.story.empty()) return;
    float a = clampf(G.storyT * 2, 0, 1) * clampf((G.storyDur - G.storyT) * 2, 0, 1);
    if (G.storyFreeze) uiRect(0, 0, UIW, UIH, withA(G.storyBg ? G.storyBg : RGBc(0, 0, 0), (int)(235 * a)));
    int h = (int)G.story.size() * 16 + 20, y = UIH / 2 - h / 2 - 30;
    if (!G.storyFreeze) uiRect(UIW / 2 - 230, y, 460, h, ARGBc((int)(150 * a), 0, 0, 0));
    for (size_t i = 0; i < G.story.size(); i++) {
        float la = clampf((G.storyT - i * 0.5f) * 2, 0, 1) * a;
        uiTextC(UIW / 2, y + 10 + (int)i * 16, G.story[i].c_str(), ARGBc((int)(255 * la), i == 0 ? 255 : 230, i == 0 ? 220 : 230, i == 0 ? 120 : 230), 1);
    }
}

// =====================================================================
//  МЕНЮ
// =====================================================================
struct MItem { std::string label; int id; bool enabled; };
static int menuList(const std::vector<MItem>& items, int cx, int y0, int w, int& sel, bool allowLR, int* lr) {
    int n = (int)items.size();
    if (n == 0) return -1;
    sel = clampi(sel, 0, n - 1);
    if (IN.up) { do { sel = (sel + n - 1) % n; } while (!items[sel].enabled); playSnd(ST_BEEP, 0.2f, 0, 0, 0.04f, 0, 0, 660); }
    if (IN.down) { do { sel = (sel + 1) % n; } while (!items[sel].enabled); playSnd(ST_BEEP, 0.2f, 0, 0, 0.04f, 0, 0, 660); }
    int act = -1;
    if (lr) *lr = 0;
    if (allowLR && lr) { if (IN.left) *lr = -1; if (IN.right) *lr = 1; }
    int h = 22, gap = 4;
    for (int i = 0; i < n; i++) {
        int y = y0 + i * (h + gap), x = cx - w / 2;
        bool f = i == sel;
        uiRect(x, y, w, h, f ? ARGBc(170, 40, 40, 30) : ARGBc(130, 15, 18, 20));
        if (f && g_lastDev != 0) uiFrame(x - 2, y - 2, w + 4, h + 4, ARGBc(255, 255, 210, 40), 2);
        uiTextC(cx, y + 8, items[i].label.c_str(), items[i].enabled ? (f ? ARGBc(255, 255, 230, 120) : ARGBc(255, 230, 230, 230)) : ARGBc(255, 100, 100, 100));
        if (g_tapX >= 0 && g_tapX - g_uiOX >= x && g_tapX - g_uiOX < x + w && g_tapY >= y && g_tapY < y + h && items[i].enabled) {
            sel = i;
            if (allowLR && lr && items[i].id >= 100) *lr = g_tapX - g_uiOX < cx ? -1 : 1;
            else act = items[i].id;
        }
    }
    if (IN.accept && items[sel].enabled) act = items[sel].id;
    if (act >= 0) playSnd(ST_BEEP, 0.3f, 0, 0, 0.06f, 0, 0, 990);
    return act;
}
static void drawMenuBg(float t) {
    for (int y = 0; y < UIH; y++) {
        float k = (float)y / UIH;
        uint32_t c = mixC(RGBc(18, 24, 30), RGBc(60, 50, 42), k);
        uiRect(0, y, UIW, 1, c);
    }
    for (int x = 0; x < UIW; x += 2) {
        float h = 40 + 30 * sinf(x * 0.013f + 1.3f) + 18 * sinf(x * 0.041f + t * 0.05f) + 10 * sinf(x * 0.11f);
        uiRect(x, UIH - (int)h - 40, 2, (int)h + 40, ARGBc(255, 22, 24, 26));
    }
    for (int i = 0; i < 40; i++) {
        uint32_t h = hash2(i, 3);
        float x = fmodf((h & 1023) + t * (8 + (h >> 20 & 15)), (float)UIW + 40) - 20;
        float y = (float)((h >> 10) & 255) / 255.0f * UIH * 0.7f;
        uiPix((int)x, (int)y, ARGBc(80, 255, 220, 160));
    }
}
static void drawMap(int x0, int y0, int w, int h, int m, float t) {
    uiRect(x0, y0, w, h, ARGBc(255, 26, 44, 58));
    for (int gx = 0; gx < w; gx += 24) uiRect(x0 + gx, y0, 1, h, ARGBc(40, 160, 200, 220));
    for (int gy = 0; gy < h; gy += 24) uiRect(x0, y0 + gy, w, 1, ARGBc(40, 160, 200, 220));
    for (int yy = 0; yy < h; yy++) {
        float v = (float)yy / h;
        float coast = 0.30f + 0.08f * sinf(v * 7.0f) + 0.04f * sinf(v * 19.0f + 1.0f);
        int cxp = (int)(coast * w);
        uiRect(x0 + cxp, y0 + yy, w - cxp, 1, ARGBc(255, 88, 96, 68));
        uiRect(x0 + cxp - 1, y0 + yy, 2, 1, ARGBc(255, 210, 200, 150));
    }
    for (int k = 0; k < 18; k++) {
        uint32_t hh = hash2(k, 11);
        int mx = x0 + (int)(w * (0.6f + 0.35f * ((hh & 255) / 255.0f))), my = y0 + (int)(h * (0.05f + 0.4f * (((hh >> 8) & 255) / 255.0f)));
        for (int j = 0; j < 5; j++) uiRect(mx - j, my + j, 2 * j + 1, 1, ARGBc(200, 130, 115, 90));
    }
    uiText(x0 + (int)(w * 0.55f), y0 + h - 14, "КАРАЛЬСКАЯ РЕСПУБЛИКА", ARGBc(200, 230, 220, 170));
    uiText(x0 + 8, y0 + h / 2, "КАРАЛЬСКОЕ МОРЕ", ARGBc(160, 160, 200, 230));
    const char* cities[4] = {"ПОРТ КАРАЛИЯ", "САРЕН", "ВЕЛЕС", "ТАРСА"};
    float cp[4][2] = {{0.58f, 0.62f}, {0.52f, 0.40f}, {0.70f, 0.32f}, {0.80f, 0.18f}};
    for (int k = 0; k < 4; k++) {
        int cxp = x0 + (int)(cp[k][0] * w), cyp = y0 + (int)(cp[k][1] * h);
        uiRect(cxp - 2, cyp - 2, 4, 4, ARGBc(230, 230, 230, 230));
        uiText(cxp + 5, cyp - 3, cities[k], ARGBc(200, 230, 230, 230));
    }
    for (int k = 0; k < m; k++) {
        if (MI[k].op == 1 || MI[k + 1].op == 1) continue;
        int px = x0 + (int)(MI[k].mx * w), py = y0 + (int)(MI[k].my * h);
        int qx = x0 + (int)(MI[k + 1].mx * w), qy = y0 + (int)(MI[k + 1].my * h);
        uiLine(px, py, qx, qy, ARGBc(120, 255, 210, 60));
    }
    int mx = x0 + (int)(MI[m].mx * w), my = y0 + (int)(MI[m].my * h);
    int r = 6 + (int)(4 * (0.5f + 0.5f * sinf(t * 4)));
    uiRing(mx, my, r, 2, ARGBc(255, 255, 70, 50));
    uiCircle(mx, my, 3, ARGBc(255, 255, 70, 50));
    uiFrame(x0, y0, w, h, ARGBc(200, 200, 200, 180));
}
static void drawVargas(int x, int y) {
    uiRect(x, y, 90, 110, ARGBc(255, 20, 20, 22));
    uiRect(x + 25, y + 30, 40, 50, ARGBc(255, 175, 130, 100));
    uiRect(x + 20, y + 18, 50, 14, ARGBc(255, 70, 80, 50)); uiRect(x + 20, y + 28, 50, 3, ARGBc(255, 160, 30, 30));
    uiRect(x + 33, y + 45, 7, 4, ARGBc(255, 30, 25, 20)); uiRect(x + 50, y + 45, 7, 4, ARGBc(255, 30, 25, 20));
    uiRect(x + 34, y + 62, 22, 5, ARGBc(255, 50, 40, 35)); uiRect(x + 38, y + 71, 14, 2, ARGBc(255, 110, 60, 50));
    uiRect(x + 10, y + 82, 70, 28, ARGBc(255, 70, 80, 50)); uiRect(x + 10, y + 82, 18, 5, ARGBc(255, 200, 170, 60)); uiRect(x + 62, y + 82, 18, 5, ARGBc(255, 200, 170, 60));
    for (int k = 0; k < 110; k += 3) uiRect(x, y + k, 90, 1, ARGBc(40, 0, 0, 0));
    uiFrame(x, y, 90, 110, ARGBc(255, 160, 50, 40));
    uiText(x, y + 114, "ГЕН. Р. ВАРГАС", ARGBc(230, 235, 150, 60));
}

// =====================================================================
//  КИНОСЦЕНА: ПАДЕНИЕ СВЕРХВЫСОКОЙ БАШНИ (Бурдж-Халифа, Дубай)
//  Масштаб сцены 1:4 — пропорции и время падения сохраняются, точность глубины выше.
// =====================================================================
static const float CS_S = 0.25f;
struct TPiece { V3 c; float yaw; V3 h; uint32_t col; int seg; };
struct TSeg { V3 pos; Basis B; V3 vel, ang; bool loose = false, gone = false; float f = 1, crushT = -1; };
struct Debris { V3 p, v, ang; Basis B; V3 h; uint32_t col; bool rest; };
static std::vector<TPiece> g_tp;
static std::vector<TSeg> g_ts;
static std::vector<Debris> g_deb;
static float g_csT = 0, g_csSegH = 1, g_csTheta = 0, g_csOmega = 0, g_csNextSnd = 0;
static int g_csK0 = 11, g_csFront = -1, g_csFailMission = 0, g_csFailStage = 0;
static bool g_csJustDead = false, g_csSeen = false;
static bool g_csActive = false, g_csFromFail = false, g_csBroken = false, g_csImpact = false, g_csBoom = false;
static std::string g_csFailMsg;
static const int CS_NS = 18;
static const V3 kCsFallDir = norm(V3(1.0f, 0, 0.35f));
static V3 g_csMissile;
static Basis basisYaw(float y) { return basisYPR(y, 0, 0); }
static Basis rotAxis(V3 a, float t) {   // поворот вокруг оси a на угол t (формула Родрига)
    a = norm(a); float c = cosf(t), s = sinf(t), k = 1 - c;
    auto rot = [&](V3 v) { return v * c + cross(a, v) * s + a * (dot(a, v) * k); };
    Basis b; b.r = rot(V3(1, 0, 0)); b.u = rot(V3(0, 1, 0)); b.f = rot(V3(0, 0, 1)); return b;
}
static void csPiece(V3 c, float yaw, V3 h, uint32_t col) {
    TPiece p; p.c = c * CS_S; p.yaw = yaw; p.h = h * CS_S; p.col = col;
    p.seg = clampi((int)(c.y / 585.0f * CS_NS), 0, CS_NS - 1);
    g_tp.push_back(p);
}
// башня: Y-образный план из трёх крыльев вокруг шестигранного ядра, 26 уступов по спирали, шпиль
static void csBuildTower() {
    g_tp.clear();
    const int N = 26; const float H = 585.0f, bh = H / N, hw = 13.0f, Lmax = 72.0f;
    uint32_t glass = RGBc(78, 92, 112), glass2 = RGBc(70, 84, 104), mech = RGBc(44, 48, 56), fin = RGBc(132, 140, 150);
    for (int k = 0; k < N; k++) {
        float y = k * bh + bh * 0.5f;
        for (int w = 0; w < 3; w++) {
            float ang = w * 2.0944f + 1.5708f;
            float t = (k + w / 3.0f) / (N + 1.0f);
            float L = 18.0f + (Lmax - 18.0f) * powf(std::max(0.0f, 1 - t), 1.1f);
            if (L < 20) continue;
            V3 d(sinf(ang), 0, cosf(ang));
            uint32_t col = (k % 6 == 5) ? mech : ((k & 1) ? glass : glass2);
            csPiece(d * (L * 0.5f) + V3(0, y, 0), ang, V3(hw, bh * 0.5f, L * 0.5f), col);
            csPiece(d * (L + 2.5f) + V3(0, y, 0), ang, V3(hw * 0.62f, bh * 0.5f, 2.5f), shadeC(col, 1.05f));   // скруглённый торец крыла
            for (int s = -1; s <= 1; s += 2) {                                                                  // вертикальные рёбра
                V3 r(cosf(ang) * s, 0, -sinf(ang) * s);
                csPiece(d * (L * 0.55f) + r * (hw + 0.6f) + V3(0, y, 0), ang, V3(0.6f, bh * 0.5f, L * 0.4f), fin);
            }
        }
    }
    for (int k = 0; k < N + 2; k++) {   // ядро: шестигранник из трёх брусьев
        float y = k * bh + bh * 0.5f;
        if (y > 610) break;
        float r = k < N ? 17.0f : 13.0f;
        for (int q = 0; q < 3; q++) csPiece(V3(0, y, 0), q * 1.0472f, V3(r * 0.866f, bh * 0.5f, r * 0.5f), (k % 6 == 5) ? mech : glass);
    }
    for (int k = 0; k < 14; k++) {      // шпиль
        float y0 = 610 + k * 16.0f, r = 11.0f * (1 - k / 15.0f) + 1.0f;
        csPiece(V3(0, y0 + 8, 0), (k & 1) * 0.52f, V3(r * 0.86f, 8, r * 0.5f), k % 3 == 2 ? mech : fin);
        csPiece(V3(0, y0 + 8, 0), (k & 1) * 0.52f + 1.0472f, V3(r * 0.86f, 8, r * 0.5f), fin);
    }
    csPiece(V3(0, 828 - 2, 0), 0, V3(0.8f, 4, 0.8f), RGBc(230, 60, 50));
    g_ts.assign(CS_NS, TSeg());
    for (auto& s : g_ts) { s.B = basisYaw(0); }
    g_csSegH = 585.0f / CS_NS * CS_S;
}
// город: сетка улиц, небоскрёбы из стекла и бетона, залив, пустыня
static void csBuildCity() {
    resetLevel();
    G.mission = 99;
    setEnv(V3(-0.62f, 0.24f, -0.72f), 0.56f, 0.8f, RGBc(214, 172, 132), 220, 600, RGBc(70, 100, 156), RGBc(242, 176, 118), 620);
    g_env.cloud = 0.28f;
    setGround(RGBc(196, 168, 126), RGBc(184, 156, 116), 0);
    bounds(-200, -200, 200, 200);
    G.bmin = V3(-200, 0, -200); G.bmax = V3(200, 0, 200);
    uint32_t road = RGBc(64, 62, 60);
    for (int i = -4; i <= 4; i++) {
        float c = i * 40.0f * CS_S * 2.0f;
        addDeco(c - 2.5f, 0, -200, c + 2.5f, 0.02f, 200, road);
        addDeco(-200, 0.005f, c - 2.5f, 200, 0.025f, c + 2.5f, road);
    }
    addDeco(-200, 0.03f, 120, 200, 0.05f, 200, RGBc(40, 92, 124));        // залив
    addDeco(-30, 0.04f, -30, 30, 0.06f, 30, RGBc(70, 120, 150));           // озеро у подножия
    for (int i = 0; i < 90; i++) {
        uint32_t h = hash2(i, 2027);
        float x = ((h & 1023) / 1023.0f - 0.5f) * 360, z = (((h >> 10) & 1023) / 1023.0f - 0.5f) * 300 - 20;
        if (z > 112 || (fabsf(x) < 42 && fabsf(z) < 42)) continue;
        float gx = roundf(x / 20) * 20 + 10, gz = roundf(z / 20) * 20 + 10;
        // коридоры обзора: от точек съёмки к башне не ставим зданий
        bool blocked = false;
        static const V3 kCams[4] = {V3(-860, 0, -1090), V3(-415, 0, -500), V3(-280, 0, -530), V3(-240, 0, -336)};
        for (const V3& cm : kCams) {
            V3 a = cm * CS_S, d = V3(0, 0, 0) - a; float L = len(d); d = d / L;
            V3 q(gx - a.x, 0, gz - a.z); float along = dot(q, d);
            if (along > -10 && along < L && len(q - d * along) < 16 + along * 0.08f) { blocked = true; break; }
        }
        if (blocked) continue;
        float w = 3.0f + ((h >> 20) & 3) * 1.4f, d = 3.0f + ((h >> 22) & 3) * 1.2f;
        float hgt = (20.0f + ((h >> 24) & 255) / 255.0f * ((h >> 26) & 1 ? 95.0f : 50.0f)) * (1.0f - std::min(0.6f, len(V3(gx, 0, gz)) / 400));
        static const uint32_t kGlass[5] = {0xFF5C7A94u, 0xFF8A9AAAu, 0xFF44627Au, 0xFFB8B0A0u, 0xFF6E8070u};
        uint32_t col = kGlass[(h >> 4) % 5];
        int b = addBox(gx - w, 0, gz - d, gx + w, hgt, gz + d, col, MT_CONCRETE);
        G.box[b].tex = (h & 3) == 0 ? TX_CONCRETE : TX_METAL;
        if (hgt > 50) { addDeco(gx - w * 0.6f, hgt, gz - d * 0.6f, gx + w * 0.6f, hgt + 3, gz + d * 0.6f, shadeC(col, 0.8f)); lamp(gx, hgt + 3.3f, gz, RGBc(255, 60, 40)); }
    }
    mountain(-260, -320, 120, 40, RGBc(176, 140, 104)); mountain(240, -300, 140, 34, RGBc(170, 138, 100));
    csBuildTower();
    g_deb.clear();
    G.farZ = 620;
    g_amb.wind = 0.35f; g_amb.echoAmt = 0.6f;
}
static void csStart(bool fromFail) {
    g_csFromFail = fromFail;
    if (fromFail) { g_csFailMission = G.mission; g_csFailStage = G.cpStage; g_csFailMsg = G.failMsg; }
    csBuildCity();
    g_csT = 0; g_csTheta = 0; g_csOmega = 0; g_csBroken = false; g_csImpact = false; g_csBoom = false; g_csFront = -1; g_csNextSnd = 0;
    g_csMissile = V3(-520, 260, -380) * CS_S;
    g_csActive = true;
    G.screen = SCR_CUTSCENE;
    G.fadeIn = 1;
}
static void csEnd() {
    g_csActive = false;
    g_amb.wind = 0; g_amb.ring = 0;
    if (g_csFromFail) {
        startMission(g_csFailMission, g_csFailStage);
        G.failed = true; G.failMsg = g_csFailMsg;
        G.screen = SCR_DEAD; g_csJustDead = true;
    } else { resetLevel(); G.screen = SCR_MENU; }
}
// положение куска башни в мире с учётом сегмента (обрушение «блинами» и падение верхушки)
static bool csPieceXform(const TPiece& p, V3& c, Basis& B, V3& h) {
    const TSeg& s = g_ts[p.seg];
    if (s.gone) return false;
    h = p.h;
    Basis Y = basisYaw(p.yaw);
    if (p.seg <= g_csK0) {
        // нижняя часть: сегменты сминаются, всё выше опускается
        float base = 0;
        for (int i = 0; i < p.seg; i++) base += g_csSegH * g_ts[i].f;
        float ly = p.c.y - p.seg * g_csSegH;
        c = V3(p.c.x, base + ly * s.f, p.c.z);
        h.y *= std::max(0.02f, s.f);
        if (s.f < 0.999f) { float k = 1 - s.f; c.x += kCsFallDir.x * k * 1.2f; c.z += kCsFallDir.z * k * 1.2f; }
        B = Y;
        return s.f > 0.03f;
    }
    // верхняя часть: до отрыва — общий поворот вокруг шарнира, после — свободное падение сегментов
    if (!s.loose) {
        float yh = (g_csK0 + 1) * g_csSegH;
        V3 hinge = kCsFallDir * (15.0f * CS_S) + V3(0, yh, 0);
        Basis R = rotAxis(cross(V3(0, 1, 0), kCsFallDir), g_csTheta);
        c = hinge + bx(R, p.c - hinge);
        B = bmul(R, Y);
    } else {
        c = s.pos + bx(s.B, p.c);
        B = bmul(s.B, Y);
    }
    return true;
}
static void csDrawTower() {
    if (!g_csActive) return;
    for (const TPiece& p : g_tp) {
        V3 c, h; Basis B;
        if (!csPieceXform(p, c, B, h)) continue;
        drawOBB(c, B.r, B.u, B.f, h, p.col);
    }
    for (const Debris& d : g_deb) drawOBB(d.p, d.B.r, d.B.u, d.B.f, d.h, d.col);
}
static void csDust(V3 p, float spread, int n, float size) {
    for (int i = 0; i < n; i++) {
        if (G.parts.size() > 1150) return;
        V3 o(frange(-1, 1) * spread, frange(0, 0.5f) * spread, frange(-1, 1) * spread);
        uint32_t c = mixC(RGBc(150, 132, 108), RGBc(196, 178, 150), frand());
        spawnPart(p + o, V3(o.x, fabsf(o.y) * 0.3f + frange(0.5f, 2.0f), o.z) * 0.6f, frange(8, 14), size * frange(0.7f, 1.3f), c, RM_BLEND, -0.02f, size * 0.25f, 0.15f);
    }
}
static void csDebrisBurst(V3 p, int n, float spd) {
    for (int i = 0; i < n && g_deb.size() < 260; i++) {
        Debris d; d.p = p + V3(frange(-2, 2), frange(-2, 2), frange(-2, 2)) * CS_S * 4;
        d.v = V3(frange(-1, 1), frange(-0.3f, 0.8f), frange(-1, 1)) * spd + kCsFallDir * spd * 0.4f;
        d.ang = V3(frange(-3, 3), frange(-3, 3), frange(-3, 3)); d.B = basisYPR(frand() * 6, frand() * 3, frand() * 3);
        d.h = V3(frange(0.4f, 1.6f), frange(0.2f, 0.7f), frange(0.4f, 1.8f)) * (CS_S * 4);
        d.col = frand() < 0.6f ? RGBc(160, 170, 182) : RGBc(110, 106, 100); d.rest = false;
        g_deb.push_back(d);
    }
}
// камера: несколько планов с плавным движением внутри плана
static void csCamera(V3& eye, V3& tgt, float& fov) {
    float t = g_csT, S = CS_S;
    fov = 60;
    if (t < 6.5f) {                        // общий план на закате
        float k = t / 6.5f;
        eye = V3(-980 + k * 120, 70 + k * 20, -1150 + k * 60) * S; tgt = V3(0, 330, 0) * S; fov = 42;
    } else if (t < 11.5f) {                // попадание и пожар
        float k = (t - 6.5f) / 5.0f;
        eye = V3(-430 + k * 30, 260 + k * 10, -520 + k * 40) * S; tgt = V3(0, 370 - k * 20, 0) * S; fov = 50;
    } else if (t < 19.0f) {                // с улицы: верхушка валится, башня складывается
        float k = (t - 11.5f) / 7.5f;
        eye = V3(-300 + k * 40, 3, -560 + k * 60) * S; tgt = V3(60 * k, 300 - 180 * k, 0) * S; fov = 68;
    } else if (t < 26.0f) {                // сверху: облако пыли накрывает город
        float k = (t - 19.0f) / 7.0f;
        eye = V3(780 - k * 120, 520 - k * 80, -760 + k * 100) * S; tgt = V3(0, 40, 0) * S; fov = 55;
    } else {                               // стена пыли идёт на камеру
        float k = (t - 26.0f) / 6.0f;
        eye = V3(-240, 2.2f, -336) * S; tgt = V3(0, 60 - 30 * k, 0) * S; fov = 70;
    }
    if (g_csImpact && t < 11) { float sh = std::max(0.0f, 1 - (t - 6.2f) * 0.4f) * 0.8f; eye += V3(frange(-sh, sh), frange(-sh, sh), frange(-sh, sh)); }
    if (t > 11.5f && t < 19) { float sh = 0.25f; eye += V3(frange(-sh, sh), frange(-sh, sh), 0); }
}
static void csUpdate(float dt) {
    if (IN.pause || IN.back || (g_csT > 1.0f && (IN.accept || g_tapX >= 0))) { csEnd(); return; }
    g_csT += dt; G.time += dt; G.frame++; g_skyTime = G.time;
    float t = g_csT, S = CS_S;
    V3 impact = V3(-6, 0.61f * 585, -14) * S;
    // ракета
    if (t > 4.2f && !g_csImpact) {
        V3 d = impact - g_csMissile; float L = len(d);
        float sp = 330 * S;
        if (L < sp * dt) g_csImpact = true;
        else {
            g_csMissile += d / L * sp * dt;
            spawnPart(g_csMissile, V3(0, 0.3f, 0), 2.5f, 0.9f, RGBc(200, 196, 190), RM_BLEND, -0.05f, 1.2f, 0.2f);
            spawnPart(g_csMissile, V3(), 0.12f, 1.4f, RGBc(255, 230, 170), RM_ADD);
        }
        if (t > 4.25f && t < 4.3f) playSnd(ST_WHIZ, 0.5f, -0.4f, 0.4f, 2.2f);
    }
    if (g_csImpact && !g_csBoom) {         // взрыв
        g_csBoom = true;
        for (int i = 0; i < 60; i++) spawnPart(impact, V3(frange(-1, 1), frange(-0.4f, 1), frange(-1, 1)) * frange(6, 22) * S * 4, frange(0.5f, 1.4f), frange(4, 9) * S * 4, RGBc(255, (int)frange(110, 200), 40), RM_ADD, 0.5f, 2.0f, 1.0f);
        for (int i = 0; i < 40; i++) spawnPart(impact, V3(frange(-1, 1), frange(0, 1), frange(-1, 1)) * frange(3, 10) * S * 4, frange(5, 9), frange(5, 10) * S * 4, RGBc(52, 48, 44), RM_BLEND, -0.1f, 1.5f, 0.3f);
        csDebrisBurst(impact, 70, 24 * S * 4);
        g_env.flash = 0.8f;
        playSnd(ST_EXPLO, 1.0f, 0, 1.0f, 4.0f);
        g_amb.ring = 0.35f;
    }
    if (g_csBoom && t < 30) {               // пожар в месте удара
        if (frand() < dt * 30) spawnPart(impact + V3(frange(-5, 5), frange(-4, 4), frange(-5, 5)) * S * 2, V3(frange(-1, 1), frange(2, 5), frange(-1, 1)) * S * 2, frange(5, 9), frange(4, 7) * S * 4, RGBc(40, 38, 36), RM_BLEND, -0.08f, 1.2f, 0.1f);
        if (t < 13 && frand() < dt * 25) spawnPart(impact + V3(frange(-4, 4), frange(-3, 3), frange(-4, 4)) * S * 2, V3(0, frange(1, 3), 0), frange(0.5f, 1.0f), frange(2, 4) * S * 4, RGBc(255, (int)frange(100, 170), 40), RM_ADD, -0.5f, 1.0f);
    }
    // наклон и отрыв верхушки
    if (t > 10.0f && !g_csBroken) {
        g_csOmega += 0.11f * dt * (1 + g_csTheta * 3);
        g_csTheta += g_csOmega * dt;
        if (frand() < dt * 14) csDust(impact, 6 * S * 4, 2, 6 * S * 4);
        if (g_csTheta > 0.42f) {
            g_csBroken = true;
            V3 axis = cross(V3(0, 1, 0), kCsFallDir);
            float yh = (g_csK0 + 1) * g_csSegH;
            V3 hinge = kCsFallDir * (15.0f * CS_S) + V3(0, yh, 0);
            Basis R = rotAxis(axis, g_csTheta);
            for (int k = g_csK0 + 1; k < CS_NS; k++) {
                TSeg& s = g_ts[k];
                s.loose = true; s.B = R;
                s.pos = hinge - bx(R, hinge);              // так, чтобы pos + R*c совпало с текущим положением
                V3 mid = hinge + bx(R, V3(0, (k + 0.5f) * g_csSegH, 0) - hinge);
                s.vel = cross(axis * g_csOmega, mid - hinge) + V3(frange(-1, 1), 0, frange(-1, 1)) * S * 4;
                s.ang = axis * g_csOmega * frange(0.8f, 1.4f) + V3(frange(-0.15f, 0.15f), frange(-0.2f, 0.2f), frange(-0.15f, 0.15f));
            }
            g_ts[g_csK0].crushT = t; g_csFront = g_csK0;
            playSnd(ST_EXPLO, 0.9f, 0.2f, 1.0f, 5.0f);
        }
    }
    // свободно падающие сегменты верхушки
    float g = 9.8f * S * 4 * 0.5f;
    for (int k = g_csK0 + 1; k < CS_NS; k++) {
        TSeg& s = g_ts[k];
        if (!s.loose || s.gone) continue;
        s.vel.y -= g * dt;
        s.pos += s.vel * dt;
        float a = len(s.ang); if (a > 1e-4f) s.B = bmul(rotAxis(s.ang, a * dt), s.B);
        V3 mid = s.pos + bx(s.B, V3(0, (k + 0.5f) * g_csSegH, 0));
        if (mid.y < g_csSegH * 0.6f) {
            s.gone = true;
            csDust(V3(mid.x, 2, mid.z), 14 * S * 4, 26, 16 * S * 4);
            csDebrisBurst(V3(mid.x, 3, mid.z), 16, 14 * S * 4);
            float vol, pan, dd; spatial(mid, vol, pan, dd);
            playSnd(ST_EXPLO, std::min(1.0f, 0.5f + vol), pan, 1.0f, 3.5f);
            G.P.shake = 1;
        }
    }
    // обрушение нижней части «блинами»: фронт идёт вниз
    if (g_csFront >= 0) {
        TSeg& s = g_ts[g_csFront];
        float dur = 0.5f;
        s.f = clampf(1 - (t - s.crushT) / dur, 0, 1);
        float base = 0; for (int i = 0; i < g_csFront; i++) base += g_csSegH * g_ts[i].f;
        V3 fp(0, base + g_csSegH * s.f, 0);
        if (frand() < dt * 40) csDust(fp + kCsFallDir * frange(-10, 10) * S * 4, 10 * S * 4, 3, 10 * S * 4);
        if (frand() < dt * 10) csDebrisBurst(fp, 3, 12 * S * 4);
        if (s.f <= 0) { s.gone = true; g_csFront--; if (g_csFront >= 0) g_ts[g_csFront].crushT = t; }
        if (t > g_csNextSnd) { g_csNextSnd = t + 0.35f; playSnd(ST_EXPLO, 0.45f, frange(-0.3f, 0.3f), 1.0f, 2.0f); }
    }
    // волна пыли по городу
    if (t > 14.0f) {
        float r = std::min(240.0f, (t - 14.0f) * 26.0f) * S * 4;
        int n = t < 30 ? 6 : 0;
        for (int i = 0; i < n; i++) {
            float a = frand() * 6.2832f;
            csDust(V3(cosf(a) * r, 2, sinf(a) * r), 6 * S * 4, 1, 14 * S * 4);
        }
        // камера в последнем плане оказывается в облаке
        V3 eye, tgt; float fov; csCamera(eye, tgt, fov);
        float de = len(V3(eye.x, 0, eye.z));
        float inside = clampf((r - de + 20) / 60.0f, 0, 1);
        g_env.fogS = lerpf(180, 2, inside); g_env.fogE = lerpf(560, 40, inside);
        g_env.fog = mixC(RGBc(214, 168, 128), RGBc(170, 150, 124), inside);
    }
    // обломки
    for (auto& d : g_deb) {
        if (d.rest) continue;
        d.v.y -= 9.8f * S * 4 * dt;
        d.p += d.v * dt;
        float a = len(d.ang); if (a > 1e-4f) d.B = bmul(rotAxis(d.ang, a * dt), d.B);
        if (d.p.y < d.h.y) { d.p.y = d.h.y; d.rest = true; }
    }
    updateEffects(dt);
    G.P.shake = std::max(0.0f, G.P.shake - dt);
    g_env.flash = std::max(0.0f, g_env.flash - dt * 1.5f);
    g_env.amb = g_baseAmb + g_env.flash * 0.45f;
    g_amb.ring = std::max(0.0f, g_amb.ring - dt * 0.05f);
    if (t > 33) csEnd();
}
static void renderCutscene(float dt) {
    (void)dt;
    V3 eye, tgt; float fov;
    csCamera(eye, tgt, fov);
    if (G.P.shake > 0) eye += V3(frange(-1, 1), frange(-1, 1), frange(-1, 1)) * G.P.shake * 0.4f;
    V3 d = tgt - eye;
    float yaw = atan2f(d.x, d.z), pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
    setCam(eye, yaw, pitch, 0, fov * DEG, G.farZ);
    drawWorld();
    glFlushVM();
    float dark = g_csT > 30 ? clampf((g_csT - 30) / 3, 0, 1) : 0;
    postEffects(0, 0, dark, 0, RGBc(240, 210, 170), 0.18f);
}
static void csDrawUI() {
    int bar = 34;
    uiRect(0, 0, UIW, bar, ARGBc(255, 0, 0, 0)); uiRect(0, UIH - bar, UIW, bar, ARGBc(255, 0, 0, 0));
    float t = g_csT;
    auto cap = [&](float t0, float t1, const char* s, int y, int sc) {
        if (t < t0 || t > t1) return;
        float a = clampf(std::min(t - t0, t1 - t) * 2, 0, 1);
        uiTextC(UIW / 2, y, s, ARGBc((int)(a * 255), 235, 225, 205), sc);
    };
    cap(0.8f, 5.0f, g_csFromFail ? "ДУБАЙ. ШЕСТЬ МИНУТ ПОСЛЕ ПУСКА" : "ДУБАЙ. 18:47", UIH - bar + 12, 1);
    cap(0.8f, 5.0f, "Бурдж-Халифа, 828 м", UIH - bar + 24, 1);
    if (g_csFromFail) cap(26.5f, 32.5f, "Ракета Варгаса достигла цели.", UIH / 2 - 6, 2);
    else cap(26.5f, 32.5f, "Если ракеты не остановить — так будет везде.", UIH / 2 - 6, 1);
    uiTextR(UIW - 8, 12, g_lastDev == 0 ? "Коснитесь — пропустить" : "Esc / [B] — пропустить", ARGBc(140, 200, 200, 200));
    float fade = clampf(1 - t / 1.2f, 0, 1);
    if (fade > 0) uiRect(0, 0, UIW, UIH, ARGBc((int)(255 * fade), 0, 0, 0));
}

// =====================================================================
//  ИГРОВОЙ ЦИКЛ, ЭКРАНЫ, ВВОД
// =====================================================================
static SDL_Window* g_win = nullptr;
static SDL_Renderer* g_ren = nullptr;
static SDL_Texture* g_texScene = nullptr;
static SDL_Texture* g_texUI = nullptr;
static bool g_quit = false;
static int g_selMain = 0, g_selSel = 0, g_selSet = 0, g_selPause = 0, g_selDead = 0, g_selDeb = 0;
static int g_settingsReturn = SCR_MENU;
static float g_menuT = 0, g_briefT = 0, g_epiT = 0, g_deadT = 0;
static int g_briefM = 0, g_briefStage = 0, g_selOp = -1, g_selCh = 0;
static float g_padBDown = -1; static bool g_padProneSent = false; static bool g_padSprint = false;
static float g_navRepeat = 0;
static bool g_relMouse = false;
static float g_missionTime = 0;
static const char* kTips[6] = {
    "Укройтесь — здоровье восстанавливается само.", "Граната рядом — смотрите на индикатор и уходите.",
    "Тонкие доски и жесть пробиваются пулями.", "Сидя и лёжа разброс заметно меньше.",
    "Перезарядка с пустого магазина дольше: досылается патрон.", "Противник обходит с флангов — следите за компасом."};
static int g_tip = 0;

static void applyQuality() {
    if (g_useGL) return;
    static const int qh[3] = {144, 180, 240};
    int q = clampi(S.quality, 0, 2);
    int w = (int)lroundf(qh[q] * (float)UIW / UIH);
    if (g_texScene && RW == w && RH == qh[q]) return;
    rendResize(w, qh[q]);
    if (g_texScene) SDL_DestroyTexture(g_texScene);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    g_texScene = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RW, RH);
}
static float g_mouseSX = 1, g_mouseSY = 1;
static void updateDst() {
    if (g_useGL) SDL_GL_GetDrawableSize(g_win, &g_winW, &g_winH);
    else SDL_GetRendererOutputSize(g_ren, &g_winW, &g_winH);
    if (g_winW <= 0 || g_winH <= 0) { g_winW = 1280; g_winH = 720; }
    // весь экран: холст интерфейса растягивается по ширине под соотношение сторон (до 2.7:1)
    float aspect = g_winW / (float)g_winH;
    UIW = clampi((int)lroundf(UIH * aspect), 640, UIW_MAX);
    float sc = std::min(g_winW / (float)UIW, g_winH / (float)UIH);
    g_dst.w = (int)(UIW * sc); g_dst.h = (int)(UIH * sc);
    if (abs(g_dst.w - g_winW) <= 2) g_dst.w = g_winW;
    if (abs(g_dst.h - g_winH) <= 2) g_dst.h = g_winH;
    g_dst.x = (g_winW - g_dst.w) / 2; g_dst.y = (g_winH - g_dst.h) / 2;
    if (g_useGL) rendResize(std::max(64, (int)lroundf(180.0f * UIW / UIH)), 180);
    else if (g_ren) {
        applyQuality();
        if (g_texUI) SDL_DestroyTexture(g_texUI);
        g_texUI = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, UIW, UIH);
        if (g_texUI) SDL_SetTextureBlendMode(g_texUI, SDL_BLENDMODE_BLEND);
    }
    int ww = 0, wh = 0;
    SDL_GetWindowSize(g_win, &ww, &wh);
    g_mouseSX = ww > 0 ? (float)g_winW / ww : 1.0f; g_mouseSY = wh > 0 ? (float)g_winH / wh : 1.0f;
}

static void finishMission() {
    int ch = g_ch;
    PR.unlocked = std::max(PR.unlocked, std::min(NCH, ch + 2));
    if (ch >= NCH - 1) { PR.done = true; PR.cpMission = -1; }
    else { PR.cpMission = ch + 1; PR.cpStage = MI[ch + 1].startStage; }
    saveAll();
    float acc = G.shots > 0 ? 100.0f * G.hitsN / G.shots : 0.0f;
    G.debrief = fmt("Время: %d:%02d\nУничтожено противников: %d\nТочность: %.0f%%\nВ голову: %d",
                    (int)g_missionTime / 60, (int)g_missionTime % 60, G.kills, acc, G.heads);
    G.screen = SCR_DEBRIEF; g_selDeb = 0;
    g_amb.heli = g_amb.rain = g_amb.wind = g_amb.breath = g_amb.heart = g_amb.ring = 0;
}
static void squadCallouts(float dt) {
    G.callContact -= dt; G.callReload -= dt; G.callKill -= dt; G.callNade -= dt;
    if ((G.stealth && !G.detected) || (G.frame % 10) != 0) return;
    for (int i = 0; i < (int)G.act.size(); i++) {
        Actor& e = G.act[i];
        if (!e.alive || e.team != 1 || e.kind != AK_SOLDIER || e.counted || !e.alerted || !e.sees) continue;
        e.counted = true;
        if (G.callContact > 0) continue;
        int sp = -1;
        for (int k = 0; k < (int)G.act.size(); k++) {
            const Actor& a = G.act[k];
            if (a.kind == AK_ALLY && a.alive && !a.hidden && a.state != AS_DOWN && a.state != AS_CARRIED) { sp = k; if (frand() < 0.4f) break; }
        }
        if (sp < 0) return;
        float a = wrapPi(yawTo(G.P.pos, e.pos) - G.P.yaw);
        const char* t;
        if (e.pos.y > G.P.pos.y + 2.2f) t = G.mission == 2 ? "Контакт! На балконе!" : "Контакт! Наверху!";
        else if (fabsf(a) < 0.6f) t = "Контакт впереди!";
        else if (a > 0 && a < 2.4f) t = "Контакт справа!";
        else if (a < 0 && a > -2.4f) t = "Контакт слева!";
        else t = "Контакт сзади!";
        say(G.act[sp].name, t, false, true);
        G.callContact = 8;
    }
}
static void docChatter(float dt) {
    if (G.mission != 2 && G.mission != 5 && G.mission != 1) return;
    G.fl[22] += dt;
    if (G.fl[22] < 75) return;
    for (auto& a : G.act) if (a.alive && a.team == 1 && a.alerted && a.kind == AK_SOLDIER) return;
    if (findAlly("Док") < 0) return;
    G.fl[22] = 0;
    static const char* L[5] = {"Тишина. Не люблю тишину — она обычно перед стрельбой.", "Кот, если что — я медик. Но лучше не надо.",
                               "Седой, а нам за переработку платят?", "Помню, в учебке тоже говорили «будет легко».", "Кто брал последний сухпай с кашей? Признавайтесь."};
    say("Док", L[rnd() % 5], false, true);
}

static void gameUpdate(float dt) {
    G.time += dt; G.frame++; g_skyTime = G.time;
    g_lisPos = playerEye(); g_lisYaw = G.P.yaw;
    if (!G.story.empty()) {
        G.storyT += dt;
        if (G.storyFreeze && G.storyT > 1.2f && (IN.accept || g_tapX >= 0)) G.storyT = std::max(G.storyT, G.storyDur - 0.3f);
        if (G.storyT >= G.storyDur) {
            int nx = G.storyNext;
            G.story.clear(); G.storyNext = -1;
            if (nx >= 0) setStage(nx);
            else if (nx == -2) missionComplete();
        }
        if (G.storyFreeze && !G.story.empty()) { subsUpdate(dt); return; }
    }
    G.missionT += dt; G.stageT += dt; g_missionTime += dt;
    playerUpdate(dt);
    updateActors(dt);
    for (int i = 0; i < (int)G.veh.size(); i++) vehUpdate(i, dt);
    updateGrenades(dt);
    stepBullets(dt);
    updateEffects(dt);
    warUpdate(dt);
    stealthUpdate(dt);
    squadCallouts(dt);
    docChatter(dt);
    if (!G.missionDone && !G.failed) missionUpdate(dt);
    subsUpdate(dt);
    G.objT -= dt; G.fl[20] -= dt; G.fl[21] -= dt;
    g_env.flash = std::max(0.0f, g_env.flash - dt * 2.5f);
    g_env.amb = g_baseAmb + g_env.flash * 0.45f;
    if (G.P.muzzle > 0) g_env.amb += 0.08f;
    if (G.P.dead) { if (G.P.deadT > 2.2f) { G.screen = SCR_DEAD; g_selDead = 0; g_deadT = 0; g_tip = (int)(rnd() % 6); } }
    else if (G.failed && G.failMsg == "Ракеты запущены." && !g_csSeen && G.endT - dt <= 0) { g_csSeen = true; csStart(true); }
    else if (G.failed) { G.endT -= dt; if (G.endT <= 0) { G.screen = SCR_DEAD; g_selDead = 0; g_deadT = 0; g_tip = (int)(rnd() % 6); } }
    else if (G.missionDone) { G.endT -= dt; if (G.endT <= 0) finishMission(); }
}

static void renderPlay(float dt) {
    Player& P = G.P;
    V3 R(cosf(P.yaw), 0, -sinf(P.yaw));
    V3 eye = playerEye() + R * (P.lean * 0.45f);
    eye.y += sinf(P.bobPh * 2) * 0.03f * P.bobAmt * (1 - P.adsT * 0.85f);
    float sh = P.shake * 0.05f + P.suppress * 0.008f;
    eye += V3(frange(-sh, sh), frange(-sh, sh), frange(-sh, sh));
    float roll = G.roll + P.lean * 0.13f;
    if (P.dead) { float k = std::min(1.0f, P.deadT); eye.y = lerpf(eye.y, P.pos.y + 0.25f, k); roll += k * 0.6f; }
    if (P.tank && !P.dead) { eye = tankCamPos() + V3(frange(-sh, sh), frange(-sh, sh), frange(-sh, sh)); roll = 0; }
    setCam(eye, aimYaw(), aimPitch(), roll, curFov() * DEG, G.farZ);
    drawWorld();
    drawViewModel(dt);
    glFlushVM();
    float red = clampf((100 - P.hp) / 100.0f * 1.3f - 0.2f, 0, 1);
    if (P.sinceHit < 0.2f) red = std::max(red, 0.45f);
    float white = clampf(P.flash / 1.5f, 0, 1);
    float dark = P.dead ? std::min(0.85f, P.deadT * 0.4f) : P.suppress * 0.25f;
    uint32_t tint = RGBc(235, 215, 175); float ta = G.mission == 3 ? 0.45f : 0.0f;
    postEffects(red, white, dark, (scopeView() || (P.tank && P.adsT > 0.5f)) ? 1.0f : 0.0f, tint, ta);
}

// ------------------------------------------------------------ экраны
static void openBrief(int ch, int stage) {
    g_briefM = clampi(ch, 0, NCH - 1); g_briefStage = stage; g_briefT = 0; G.screen = SCR_BRIEF;
    playSnd(ST_RADIO, 0.4f, 0, 0, 0.24f);
}
static void beginPlay() {
    g_missionTime = 0;
    g_ch = g_briefM;
    const MissionInfo& C = MI[g_ch];
    int st = g_briefStage;
    if (st < C.startStage || (C.endStage >= 0 && st >= C.endStage)) st = C.startStage;
    startMission(C.mission, st);
}
static int chapterNo(int ch) { int n = 1; for (int i = 0; i < ch; i++) if (MI[i].op == MI[ch].op) n++; return n; }
static const char* kRoman[5] = {"I", "II", "III", "IV", "V"};
static void screenMenu() {
    drawMenuBg(g_menuT);
    uiTextC(UIW / 2, 40, "ПОЗЫВНОЙ: ГРОМ", ARGBc(255, 255, 220, 120), 4);
    uiTextC(UIW / 2, 78, "КАРАЛЬСКИЙ КРИЗИС. 2027", ARGBc(220, 220, 220, 220), 1);
    std::vector<MItem> it;
    if (PR.cpMission >= 0 && PR.cpMission < NCH && (PR.cpMission > 0 || PR.cpStage > 0))
        it.push_back({fmt("Продолжить: %s. %s", kRoman[MI[PR.cpMission].op], MI[PR.cpMission].title), 0, true});
    it.push_back({"Новая кампания", 1, true});
    it.push_back({"Операции", 2, true});
    it.push_back({"Настройки", 3, true});
    it.push_back({"Сцена: падение башни", 5, true});
    it.push_back({"Выход", 4, true});
    int act = menuList(it, UIW / 2, 130, 280, g_selMain, false, nullptr);
    if (act == 0) openBrief(PR.cpMission, PR.cpStage);
    else if (act == 1) { PR.cpMission = 0; PR.cpStage = 0; openBrief(0, 0); }
    else if (act == 2) { G.screen = SCR_SELECT; g_selSel = 0; g_selOp = -1; }
    else if (act == 3) { g_settingsReturn = SCR_MENU; G.screen = SCR_SETTINGS; g_selSet = 0; }
    else if (act == 4) g_quit = true;
    else if (act == 5) csStart(false);
    if (PR.done) uiTextC(UIW / 2, UIH - 30, "Кампания пройдена. Дело не закрыто.", ARGBc(200, 200, 200, 200));
    std::string hint = g_lastDev == 1 ? "[A] выбрать   [B] назад" : (g_lastDev == 0 ? "Коснитесь пункта меню" : "Enter — выбрать, Esc — назад");
    uiTextC(UIW / 2, UIH - 14, hint.c_str(), ARGBc(160, 200, 200, 200));
}
// выбор операции, затем главы (как «Военные истории»)
static void screenSelect() {
    drawMenuBg(g_menuT);
    if (g_selOp < 0) {
        uiTextC(UIW / 2, 20, "ОПЕРАЦИИ", ARGBc(255, 255, 220, 120), 2);
        std::vector<MItem> it;
        for (int o = 0; o < NOPS; o++) {
            int first = 0; for (int c = 0; c < NCH; c++) if (MI[c].op == o) { first = c; break; }
            bool open = first < PR.unlocked;
            int n = 0, done = 0;
            for (int c = 0; c < NCH; c++) if (MI[c].op == o) { n++; if (c + 1 < PR.unlocked || (PR.done && c == NCH - 1)) done++; }
            it.push_back({open ? fmt("%s. %s  (%d/%d глав)", kRoman[o], OPS[o].name, done, n) : fmt("%s. ЗАКРЫТО", kRoman[o]), o, open});
        }
        it.push_back({"Назад", 10, true});
        int act = menuList(it, UIW / 2, 56, 400, g_selSel, false, nullptr);
        int f = clampi(g_selSel, 0, NOPS);
        if (f < NOPS) {
            uiTextC(UIW / 2, 250, OPS[f].hero, ARGBc(230, 140, 200, 255));
            uiTextC(UIW / 2, 264, OPS[f].tag, ARGBc(220, 220, 220, 220));
        }
        if (act >= 0 && act < NOPS) { g_selOp = act; g_selCh = 0; }
        if (act == 10 || IN.back) G.screen = SCR_MENU;
        return;
    }
    const OpInfo& O = OPS[g_selOp];
    uiTextC(UIW / 2, 16, fmt("ОПЕРАЦИЯ %s: %s", kRoman[g_selOp], O.name).c_str(), ARGBc(255, 255, 220, 120), 2);
    uiTextC(UIW / 2, 40, O.hero, ARGBc(230, 140, 200, 255));
    std::vector<MItem> it;
    for (int c = 0; c < NCH; c++) {
        if (MI[c].op != g_selOp) continue;
        bool open = c < PR.unlocked;
        std::string l = open ? fmt("Глава %d. %s", chapterNo(c), MI[c].title) : fmt("Глава %d. ЗАКРЫТО", chapterNo(c));
        if (c == 0 && PR.bestKH > 0) l += fmt("  (рекорд %.1f с)", PR.bestKH);
        it.push_back({l, c, open});
    }
    it.push_back({"Назад", 100 + 99, true});
    int act = menuList(it, UIW / 2, 64, 360, g_selCh, false, nullptr);
    if (g_selCh < (int)it.size() - 1) { int c = it[g_selCh].id; if (c < NCH && it[g_selCh].enabled) uiTextC(UIW / 2, 250, MI[c].sub, ARGBc(220, 220, 220, 220)); }
    if (act >= 0 && act < NCH) openBrief(act, MI[act].startStage);
    if (act == 199 || IN.back) { g_selOp = -1; }
}
static void screenSettings() {
    drawMenuBg(g_menuT);
    uiTextC(UIW / 2, 20, "НАСТРОЙКИ", ARGBc(255, 255, 220, 120), 2);
    static const char* qls[3] = {"Низкое", "Среднее", "Высокое"};
    static const char* qlg[3] = {"Низкое (без теней)", "Среднее (тени)", "Высокое (тени HD)"};
    const char* const* ql = g_useGL ? qlg : qls;
    static const char* dl[3] = {"Рекрут", "Регуляр", "Ветеран"};
    std::vector<MItem> it = {
        {fmt("< Обзор (тач/мышь): %.1f >", S.lookSens), 100, true}, {fmt("< Обзор (геймпад): %.1f >", S.padSens), 101, true},
        {fmt("< Инверсия по вертикали: %s >", S.invertY ? "Да" : "Нет"), 102, true}, {fmt("< Громкость: %d >", S.volume), 103, true},
        {fmt("< Вибрация: %s >", S.vibration ? "Вкл" : "Выкл"), 104, true}, {fmt("< Качество графики: %s >", ql[clampi(S.quality, 0, 2)]), 105, true},
        {fmt("< Сложность: %s >", dl[clampi(S.difficulty, 0, 2)]), 106, true}, {fmt("< Показывать FPS: %s >", S.showFps ? "Да" : "Нет"), 107, true},
        {fmt("< Рендер: %s%s >", S.renderer == 0 ? "Видеокарта" : "Программный", (S.renderer == 0) != g_useGL ? " (после перезапуска)" : ""), 108, true},
        {fmt("< Помощь в прицеливании (геймпад): %s >", S.aimAssist ? "Вкл" : "Выкл"), 109, true},
        {"Назад", 1, true}};
    int lr = 0;
    int act = menuList(it, UIW / 2, 50, 340, g_selSet, true, &lr);
    int id = it[clampi(g_selSet, 0, (int)it.size() - 1)].id;
    if (act >= 100) { lr = 1; id = act; }
    if (lr != 0 && id >= 100) {
        switch (id) {
        case 100: S.lookSens = clampf(S.lookSens + lr * 0.1f, 0.2f, 3.0f); break;
        case 101: S.padSens = clampf(S.padSens + lr * 0.1f, 0.2f, 3.0f); break;
        case 102: S.invertY = !S.invertY; break;
        case 103: S.volume = clampi(S.volume + lr, 0, 10); break;
        case 104: S.vibration = !S.vibration; if (S.vibration) rumble(0.5f, 0.5f, 150); break;
        case 105: S.quality = (S.quality + lr + 3) % 3; applyQuality(); break;
        case 106: S.difficulty = (S.difficulty + lr + 3) % 3; break;
        case 107: S.showFps = !S.showFps; break;
        case 108: S.renderer = 1 - S.renderer; break;
        case 109: S.aimAssist = !S.aimAssist; break;
        }
        playSnd(ST_BEEP, 0.3f, 0, 0, 0.05f, 0, 0, 770);
    }
    g_amb.master = S.volume / 10.0f * 0.9f;
    if (act == 1 || IN.back) { saveAll(); G.screen = g_settingsReturn; }
    uiTextC(UIW / 2, UIH - 14, g_lastDev == 0 ? "Касание слева — меньше, справа — больше" : "Влево/вправо — изменить", ARGBc(160, 200, 200, 200));
}
static void screenBrief(float dt) {
    g_briefT += dt;
    const MissionInfo& M = MI[g_briefM];
    uiRect(0, 0, UIW, UIH, ARGBc(255, 12, 16, 20));
    g_uiOX = (UIW - 640) / 2;
    drawMap(14, 40, 360, 300, g_briefM, g_briefT);
    if (M.vargas) drawVargas(20, 206);
    uiText(14, 6, fmt("ОПЕРАЦИЯ %s: %s", kRoman[M.op], OPS[M.op].name).c_str(), ARGBc(220, 200, 200, 200));
    uiText(14, 17, fmt("ГЛАВА %d: %s", chapterNo(g_briefM), M.title).c_str(), ARGBc(255, 255, 220, 120), 2);
    int x = 390, y = 44;
    uiText(x, y, M.sub, ARGBc(230, 200, 220, 255)); y += 12;
    uiText(x, y, OPS[M.op].hero, ARGBc(200, 140, 200, 255)); y += 14;
    uiText(x, y, "МАЙОР ГРИН, ШТАБ:", ARGBc(220, 190, 190, 255)); y += 12;
    int chars = (int)(g_briefT * 50);
    int total = 0;
    for (int i = 0; i < 5 && M.brief[i]; i++) {
        auto lines = wrapText(M.brief[i], 236);
        for (auto& l : lines) {
            int n = utf8Len(l.c_str());
            if (chars <= total) break;
            std::string vis;
            if (chars - total >= n) vis = l;
            else { const char* p = l.c_str(); const char* s0 = p; for (int k = 0; k < chars - total; k++) utf8Next(p); vis.assign(s0, (size_t)(p - s0)); }
            uiText(x, y, vis.c_str(), ARGBc(240, 230, 230, 230));
            y += 10; total += n;
        }
        y += 4;
    }
    int allChars = 0;
    for (int i = 0; i < 5 && MI[g_briefM].brief[i]; i++) allChars += utf8Len(MI[g_briefM].brief[i]);
    bool done = chars >= allChars;
    uiText(x, 268, "ЦЕЛЬ:", ARGBc(240, 255, 210, 60));
    auto gl = wrapText(M.goal, 236);
    for (size_t i = 0; i < gl.size(); i++) uiText(x, 280 + (int)i * 10, gl[i].c_str(), ARGBc(240, 255, 255, 255));
    if (g_briefStage > M.startStage) uiText(x, 256, "С контрольной точки", ARGBc(200, 160, 220, 160));
    std::vector<MItem> it = {{done ? "НАЧАТЬ" : "ПРОПУСТИТЬ", 0, true}};
    int sel = 0;
    int act = menuList(it, 510, 316, 220, sel, false, nullptr);
    if (act == 0) { if (!done) g_briefT = 1000; else beginPlay(); }
    if (IN.back) G.screen = SCR_MENU;
}
static void screenPause() {
    uiRect(0, 0, UIW, UIH, ARGBc(150, 0, 0, 0));
    uiTextC(UIW / 2, 40, "ПАУЗА", ARGBc(255, 255, 220, 120), 3);
    uiTextC(UIW / 2, 74, fmt("Операция %s, глава %d: %s", kRoman[MI[g_ch].op], chapterNo(g_ch), MI[g_ch].title).c_str(), ARGBc(230, 230, 230, 230));
    if (!G.objective.empty()) uiTextC(UIW / 2, 88, ("Задача: " + G.objective).c_str(), ARGBc(230, 255, 230, 140));
    std::vector<MItem> it = {{"Продолжить", 0, true}, {"С контрольной точки", 1, true}, {"Настройки", 2, true}, {"Выход в меню", 3, true}};
    int act = menuList(it, UIW / 2, 120, 260, g_selPause, false, nullptr);
    if (act == 0 || IN.back || IN.pause) G.screen = SCR_PLAY;
    else if (act == 1) { g_missionTime = 0; startMission(G.mission, G.cpStage); }
    else if (act == 2) { g_settingsReturn = SCR_PAUSE; G.screen = SCR_SETTINGS; g_selSet = 0; }
    else if (act == 3) { saveAll(); G.screen = SCR_MENU; g_amb.heli = g_amb.rain = g_amb.wind = g_amb.breath = g_amb.heart = g_amb.ring = 0; }
}
static void screenDead(float dt) {
    g_deadT += dt;
    uiRect(0, 0, UIW, UIH, ARGBc((int)(170 * clampf(g_deadT * 2, 0, 1)), 20, 0, 0));
    bool fail = G.failed && !G.P.dead;
    uiTextC(UIW / 2, 60, fail ? "ЗАДАНИЕ ПРОВАЛЕНО" : "ВЫ ПОГИБЛИ", ARGBc(255, 255, 90, 70), 3);
    if (fail) uiTextC(UIW / 2, 96, G.failMsg.c_str(), ARGBc(240, 240, 240, 240));
    else if (!G.failMsg.empty()) uiTextC(UIW / 2, 96, G.failMsg.c_str(), ARGBc(240, 240, 240, 240));
    uiTextC(UIW / 2, 116, (std::string("Совет: ") + kTips[g_tip]).c_str(), ARGBc(220, 200, 200, 200));
    std::vector<MItem> it = {{"С контрольной точки", 0, true}, {"Выход в меню", 1, true}};
    int act = menuList(it, UIW / 2, 160, 260, g_selDead, false, nullptr);
    if (act == 0) { g_missionTime = 0; startMission(G.mission, G.cpStage); }
    else if (act == 1 || IN.back) { G.screen = SCR_MENU; g_amb.heli = g_amb.rain = g_amb.wind = g_amb.breath = g_amb.heart = g_amb.ring = 0; }
}
// обезвреживание боеголовки: панель с проводами, индикатор, подсказка Дока, таймер
static int g_selDef = 0;
static void screenDefuse(float dt) {
    G.timer -= dt; g_df.flash = std::max(0.0f, g_df.flash - dt * 2);
    if (G.timer <= 0) { G.screen = SCR_PLAY; missionFail("Боеголовка сработала."); return; }
    uiRect(0, 0, UIW, UIH, ARGBc(150, 0, 0, 0));
    if (g_df.flash > 0) uiRect(0, 0, UIW, UIH, ARGBc((int)(g_df.flash * 120), 200, 30, 20));
    g_uiOX = (UIW - 640) / 2;
    int px = 70, py = 40, pw = 250, ph = 250;
    uiRect(px, py, pw, ph, ARGBc(255, 36, 40, 38)); uiRect(px + 4, py + 4, pw - 8, ph - 8, ARGBc(255, 52, 58, 54));
    uiTextC(px + pw / 2, py + 12, "БОЕГОЛОВКА: ВЗРЫВАТЕЛЬ", ARGBc(255, 210, 210, 200));
    int ss = (int)ceilf(G.timer);
    uiRect(px + pw / 2 - 52, py + 26, 104, 26, ARGBc(255, 10, 14, 10));
    uiTextC(px + pw / 2, py + 33, fmt("%d:%02d", ss / 60, ss % 60).c_str(), ARGBc(255, 255, (int)(fmodf(G.time * 2, 1) < 0.5f ? 70 : 140), 60), 2);
    bool blink = fmodf(G.time * 1.5f, 1) < 0.6f;
    uint32_t ic = kWireCol[g_df.indicator];
    uiTextC(px + pw / 2 - 30, py + 64, "ИНДИКАТОР", ARGBc(255, 190, 190, 180));
    uiRect(px + pw / 2 + 20, py + 60, 16, 14, blink ? (ic | 0xFF000000u) : ARGBc(255, 30, 30, 30));
    for (int i = 0; i < 4; i++) {
        int x = px + 40 + i * 56;
        uint32_t c = kWireCol[g_df.wire[i]] | 0xFF000000u;
        uiTextC(x, py + 86, fmt("%d", i + 1).c_str(), ARGBc(255, 220, 220, 210));
        if (g_df.cut[i]) { uiRect(x - 4, py + 98, 8, 50, c); uiRect(x - 4, py + 170, 8, 50, c); uiRect(x - 7, py + 148, 14, 3, ARGBc(255, 255, 220, 120)); }
        else uiRect(x - 4, py + 98, 8, 122, c);
        if (i == clampi(g_selDef, 0, 3) && !g_df.cut[i]) uiRect(x - 9, py + 94, 18, 130, ARGBc(70, 255, 230, 90));
    }
    uiRect(px + 20, py + 222, pw - 40, 10, ARGBc(255, 30, 32, 30));
    // подсказка Дока (правило)
    int tx = 340;
    uiText(tx, 50, "ДОК (по рации):", ARGBc(255, 140, 220, 255));
    const char* rules[4] = {"1. Первым режь провод того же цвета,", "   что и индикатор.", "2. Дальше — слева направо, красный пропускай.", "3. Красный — всегда последним."};
    for (int i = 0; i < 4; i++) uiText(tx, 66 + i * 12, rules[i], ARGBc(255, 230, 230, 230));
    if (g_df.wrong) uiText(tx, 122, fmt("Ошибок: %d (каждая — минус 15 с)", g_df.wrong).c_str(), ARGBc(255, 255, 120, 90));
    std::vector<MItem> it;
    for (int i = 0; i < 4; i++) it.push_back({g_df.cut[i] ? fmt("Провод %d — перерезан", i + 1) : fmt("Перерезать провод %d (%s)", i + 1, kWireName[g_df.wire[i]]), i, !g_df.cut[i]});
    if (g_df.cut[clampi(g_selDef, 0, 3)]) for (int i = 0; i < 4; i++) if (!g_df.cut[i]) { g_selDef = i; break; }
    int act = menuList(it, tx + 120, 150, 250, g_selDef, false, nullptr);
    if (IN.sel >= 1 && IN.sel <= 4) act = IN.sel - 1;
    if (act >= 0 && act < 4) defuseCut(act);
}
static void screenDebrief() {
    drawMenuBg(g_menuT);
    bool opEnd = g_ch == NCH - 1 || MI[g_ch + 1].op != MI[g_ch].op;
    uiTextC(UIW / 2, 40, opEnd ? "ОПЕРАЦИЯ ЗАВЕРШЕНА" : "ГЛАВА ПРОЙДЕНА", ARGBc(255, 160, 255, 160), 3);
    uiTextC(UIW / 2, 76, fmt("%s. %s — %s", kRoman[MI[g_ch].op], OPS[MI[g_ch].op].name, MI[g_ch].title).c_str(), ARGBc(240, 255, 220, 120), 1);
    if (opEnd && g_ch < NCH - 1) uiTextC(UIW / 2, 92, fmt("Далее: операция %s «%s»", kRoman[MI[g_ch + 1].op], OPS[MI[g_ch + 1].op].name).c_str(), ARGBc(220, 220, 220, 220));
    int y = 110;
    auto lines = wrapText(G.debrief, 400);
    for (auto& l : lines) { uiTextC(UIW / 2, y, l.c_str(), ARGBc(240, 235, 235, 235)); y += 14; }
    std::vector<MItem> it = {{g_ch == NCH - 1 ? "Эпилог" : "Далее", 0, true}, {"В меню", 1, true}};
    int act = menuList(it, UIW / 2, 220, 240, g_selDeb, false, nullptr);
    if (act == 0) { if (g_ch == NCH - 1) { G.screen = SCR_EPILOGUE; g_epiT = 0; } else openBrief(g_ch + 1, MI[g_ch + 1].startStage); }
    else if (act == 1) G.screen = SCR_MENU;
}
static void screenEpilogue(float dt) {
    g_epiT += dt;
    uiRect(0, 0, UIW, UIH, ARGBc(255, 0, 0, 0));
    static const char* L[6] = {"Ракеты не поднялись в воздух, последняя боеголовка обезврежена в Джебель-Али.", "Генерал Варгас бежал из страны до рассвета.",
                               "Синдикат Ворона разгромлен. Его счета заморожены.", "Тело Эрика Холстена не найдено.", "Отряд «Гром» возвращается домой.", nullptr};
    int y = 90;
    for (int i = 0; i < 5; i++) {
        float a = clampf((g_epiT - i * 2.5f) * 0.8f, 0, 1);
        if (a > 0) uiTextC(UIW / 2, y, L[i], ARGBc((int)(255 * a), 230, 230, 230));
        y += 18;
    }
    float a = clampf((g_epiT - 15.0f) * 0.6f, 0, 1);
    if (a > 0) uiTextC(UIW / 2, 230, "Дело не закрыто.", ARGBc((int)(255 * a), 255, 80, 60), 2);
    if (g_epiT > 18) uiTextC(UIW / 2, UIH - 30, "ПОЗЫВНОЙ: ГРОМ — КОНЕЦ", ARGBc(180, 200, 200, 200));
    if ((IN.accept || IN.back || g_tapX >= 0) && g_epiT > 3) { if (g_epiT < 15) g_epiT = 15; else if (g_epiT > 17) G.screen = SCR_MENU; }
}

// ------------------------------------------------------------ события
// геймпад: известные контроллеры открываются сразу; для неизвестных добавляем стандартную раскладку
static std::string g_padToast; static float g_padToastT = 0;
static void openPad(int idx) {
    if (g_pad) return;
    const char* jn = SDL_JoystickNameForIndex(idx);
    std::string name = jn ? jn : "";
    for (auto& ch : name) ch = (char)tolower((unsigned char)ch);
    if (name.find("accelerometer") != std::string::npos || name.find("sensor") != std::string::npos) return;
    if (!SDL_IsGameController(idx)) {
        char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), guid, sizeof guid);
        std::string map = std::string(guid) + "," + (jn ? jn : "Gamepad") +
            ",a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
            "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
        SDL_GameControllerAddMapping(map.c_str());
        if (!SDL_IsGameController(idx)) return;
    }
    g_pad = SDL_GameControllerOpen(idx);
    if (g_pad) {
        const char* n = SDL_GameControllerName(g_pad);
        g_padToast = std::string("Геймпад подключён: ") + (n ? n : "контроллер"); g_padToastT = 3.5f;
        g_lastDev = 1; rumble(0.4f, 0.4f, 200);
    }
}
static void handleEvent(const SDL_Event& e) {
    bool play = G.screen == SCR_PLAY;
    switch (e.type) {
    case SDL_QUIT: g_quit = true; break;
    case SDL_APP_WILLENTERBACKGROUND:
        if (play) G.screen = SCR_PAUSE;
        saveAll();
        break;
    case SDL_WINDOWEVENT: if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) updateDst(); break;
    case SDL_FINGERDOWN: { float ux, uy; toUI(e.tfinger.x, e.tfinger.y, ux, uy); touchDown(e.tfinger.fingerId, ux, uy); } break;
    case SDL_FINGERMOTION: { float ux, uy; toUI(e.tfinger.x, e.tfinger.y, ux, uy); touchMove(e.tfinger.fingerId, ux, uy); } break;
    case SDL_FINGERUP: { float ux, uy; toUI(e.tfinger.x, e.tfinger.y, ux, uy); touchUp(e.tfinger.fingerId, ux, uy); } break;
    case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
    case SDL_JOYDEVICEADDED: if (!SDL_IsGameController(e.jdevice.which)) openPad(e.jdevice.which); break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (g_pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pad))) {
            SDL_GameControllerClose(g_pad); g_pad = nullptr;
            g_padToast = "Геймпад отключён"; g_padToastT = 2.5f; if (g_lastDev == 1) g_lastDev = 0;
            if (G.screen == SCR_PLAY) { G.screen = SCR_PAUSE; g_selPause = 0; }
            for (int i = 0; i < SDL_NumJoysticks(); i++) openPad(i);
            if (play) G.screen = SCR_PAUSE;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        g_lastDev = 1;
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A: IN.accept = true; IN.use = true; break;
        case SDL_CONTROLLER_BUTTON_B:
            if (!play) IN.back = true;
            g_padBDown = G.time; g_padProneSent = false;
            break;
        case SDL_CONTROLLER_BUTTON_X: IN.reload = true; break;
        case SDL_CONTROLLER_BUTTON_Y: IN.sw = true; break;
        case SDL_CONTROLLER_BUTTON_START: case SDL_CONTROLLER_BUTTON_BACK:
            IN.pause = true;
            if (!play) IN.back = true;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: IN.up = true; IN.tacCycle = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: IN.down = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: IN.left = true; IN.leanL = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: IN.right = true; IN.leanR = true; break;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: g_padSprint = !g_padSprint; break;
        default: break;
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_B && play) { if (!g_padProneSent && G.time - g_padBDown < 0.4f) IN.stance = true; g_padBDown = -1; }
        break;
    case SDL_KEYDOWN:
        if (e.key.repeat) {
            SDL_Scancode sc = e.key.keysym.scancode;
            if (!play && (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)) IN.up = true;
            if (!play && (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)) IN.down = true;
            break;
        }
        g_lastDev = 2;
        switch (e.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_AC_BACK: IN.pause = true; IN.back = true; break;
        case SDL_SCANCODE_UP: IN.up = true; break;
        case SDL_SCANCODE_DOWN: IN.down = true; break;
        case SDL_SCANCODE_LEFT: IN.left = true; break;
        case SDL_SCANCODE_RIGHT: IN.right = true; break;
        case SDL_SCANCODE_W: if (!play) IN.up = true; break;
        case SDL_SCANCODE_S: if (!play) IN.down = true; break;
        case SDL_SCANCODE_A: if (!play) IN.left = true; break;
        case SDL_SCANCODE_D: if (!play) IN.right = true; break;
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: IN.accept = true; break;
        case SDL_SCANCODE_R: IN.reload = true; break;
        case SDL_SCANCODE_C: IN.stance = true; break;
        case SDL_SCANCODE_Z: case SDL_SCANCODE_LCTRL: IN.prone = true; break;
        case SDL_SCANCODE_Q: IN.leanL = true; break;
        case SDL_SCANCODE_E: IN.leanR = true; break;
        case SDL_SCANCODE_F: IN.use = true; break;
        case SDL_SCANCODE_V: IN.tacCycle = true; break;
        case SDL_SCANCODE_1: IN.sel = 1; break;
        case SDL_SCANCODE_2: IN.sel = 2; break;
        case SDL_SCANCODE_3: IN.sel = 3; break;
        case SDL_SCANCODE_4: IN.sel = 4; break;
        case SDL_SCANCODE_TAB: G.objT = 5; break;
        default: break;
        }
        break;
    case SDL_MOUSEMOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID) break;
        if (play && g_relMouse) { IN.lookX += e.motion.xrel * 0.12f * S.lookSens; IN.lookY += e.motion.yrel * 0.12f * S.lookSens; }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (e.button.which == SDL_TOUCH_MOUSEID) break;
        g_lastDev = 2;
        if (!play && e.button.button == SDL_BUTTON_LEFT) {
            float ux = (e.button.x * g_mouseSX - g_dst.x) * (float)UIW / std::max(1, g_dst.w), uy = (e.button.y * g_mouseSY - g_dst.y) * (float)UIH / std::max(1, g_dst.h);
            g_tapX = ux; g_tapY = uy;
        }
        break;
    case SDL_MOUSEWHEEL: if (play) IN.sw = true; break;
    default: break;
    }
}
// помощь в прицеливании с геймпада: замедление на цели, доводка при прицеливании, лёгкое «прилипание»
static float g_adsHeldT = 0;
static void aimAssist(float dt, bool sticking) {
    Player& P = G.P;
    if (P.dead || P.tank) return;
    g_adsHeldT = IN.ads ? g_adsHeldT + dt : 0;
    V3 eye = playerEye();
    float ay = aimYaw(), ap = aimPitch();
    int best = -1; float bestA = 1e9f, bdy = 0, bdp = 0;
    for (int i = 0; i < (int)G.act.size(); i++) {
        const Actor& a = G.act[i];
        if (!a.alive || a.team != 1 || a.hidden || (a.kind != AK_SOLDIER && a.kind != AK_DOG)) continue;
        V3 c = actorChest(a); float d = len(c - eye);
        if (d > 70 || d < 1) continue;
        float dy = wrapPi(atan2f(c.x - eye.x, c.z - eye.z) - ay), dp = atan2f(c.y - eye.y, dist2D(c, eye)) - ap;
        float ang = sqrtf(dy * dy + dp * dp), lim = atan2f(1.4f, d) + 2.5f * DEG;
        if (ang < lim * 2.2f && ang < bestA && losClear(eye, c)) { bestA = ang; best = i; bdy = dy; bdp = dp; }
    }
    if (best < 0) return;
    IN.lookX *= 0.55f; IN.lookY *= 0.55f;                                     // замедление на цели
    float k = 0;
    if (IN.ads && g_adsHeldT < 0.3f) k = std::min(1.0f, dt * 14);           // доводка при вскидывании оружия
    else if (sticking || IN.fire) k = std::min(1.0f, dt * (IN.ads ? 4.0f : 2.0f));
    P.yaw = wrapPi(P.yaw + bdy * k);
    P.pitch += bdp * k;
}
static float padAxis(SDL_GameControllerAxis a) {
    if (!g_pad) return 0;
    return SDL_GameControllerGetAxis(g_pad, a) / 32767.0f;
}
static void pollInput(float dt) {
    bool play = G.screen == SCR_PLAY;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (play && g_lastDev == 2) {
        if (ks[SDL_SCANCODE_W]) IN.my += 1;
        if (ks[SDL_SCANCODE_S]) IN.my -= 1;
        if (ks[SDL_SCANCODE_D]) IN.mx += 1;
        if (ks[SDL_SCANCODE_A]) IN.mx -= 1;
        if (ks[SDL_SCANCODE_LSHIFT]) { IN.sprint = true; IN.breath = true; }
        if (ks[SDL_SCANCODE_G]) IN.nadeHeld = true;
        if (ks[SDL_SCANCODE_T]) IN.tacHeld = true;
        if (ks[SDL_SCANCODE_F]) IN.useHeld = true;
        Uint32 mb = SDL_GetMouseState(nullptr, nullptr);
        if (g_relMouse) {
            if (mb & SDL_BUTTON(SDL_BUTTON_LEFT)) IN.fire = true;
            if (mb & SDL_BUTTON(SDL_BUTTON_RIGHT)) IN.ads = true;
        }
    }
    if (g_pad) {
        float lx = padAxis(SDL_CONTROLLER_AXIS_LEFTX), ly = padAxis(SDL_CONTROLLER_AXIS_LEFTY);
        float rx = padAxis(SDL_CONTROLLER_AXIS_RIGHTX), ry = padAxis(SDL_CONTROLLER_AXIS_RIGHTY);
        float lt = padAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), rt = padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        float ll = sqrtf(lx * lx + ly * ly), rl = sqrtf(rx * rx + ry * ry);
        bool active = ll > 0.25f || rl > 0.25f || lt > 0.3f || rt > 0.3f;
        if (active) g_lastDev = 1;
        if (play) {
            if (ll > 0.2f) { float k = (ll - 0.2f) / 0.8f / ll; IN.mx += lx * k; IN.my -= ly * k; }
            if (rl > 0.15f) {
                float m = powf(std::min(1.0f, (rl - 0.15f) / 0.85f), 1.7f) / rl;
                IN.lookX += rx * m * 170.0f * S.padSens * dt;
                IN.lookY += ry * m * 120.0f * S.padSens * dt;
            }
            if (lt > 0.3f) IN.ads = true;
            if (rt > 0.3f) IN.fire = true;
            if (S.aimAssist && g_lastDev == 1) aimAssist(dt, rl > 0.15f);
            if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) IN.nadeHeld = true;
            if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) IN.tacHeld = true;
            if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A)) IN.useHeld = true;
            if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_LEFTSTICK) || SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) IN.breath = true;
            if (g_padSprint) { if (-ly < 0.3f) g_padSprint = false; else IN.sprint = true; }
            if (g_padBDown >= 0 && !g_padProneSent && G.time - g_padBDown >= 0.4f && SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_B)) { IN.prone = true; g_padProneSent = true; }
        } else {
            g_navRepeat -= dt;
            if (fabsf(ly) > 0.6f && g_navRepeat <= 0) { if (ly < 0) IN.up = true; else IN.down = true; g_navRepeat = 0.22f; }
            if (fabsf(lx) > 0.6f && g_navRepeat <= 0) { if (lx < 0) IN.left = true; else IN.right = true; g_navRepeat = 0.22f; }
            if (fabsf(ly) < 0.3f && fabsf(lx) < 0.3f) g_navRepeat = 0;
        }
    }
    touchFrame(dt);
    float L = sqrtf(IN.mx * IN.mx + IN.my * IN.my);
    if (L > 1) { IN.mx /= L; IN.my /= L; }
    bool wantRel = play && g_lastDev == 2;
    if (wantRel != g_relMouse) { g_relMouse = wantRel; SDL_SetRelativeMouseMode(wantRel ? SDL_TRUE : SDL_FALSE); }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    fontInit();
    texInit();
    char* pp = SDL_GetPrefPath("GromStudio", "PozyvnoyGrom");
    if (pp) { g_prefPath = pp; SDL_free(pp); }
    loadAll();
    audioInit();
    Uint32 wflags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
#if defined(__ANDROID__) || defined(__IPHONEOS__)
    wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    // 1) пробуем видеокарту: OpenGL ES 2.0, затем OpenGL 2.1 (ПК); 2) иначе — программный рендер
    if (S.renderer == 0) {
        for (int attempt = 0; attempt < 2 && !g_useGL; attempt++) {
            SDL_GL_ResetAttributes();
            if (attempt == 0) {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            } else {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            }
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16); SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            g_glDesktop = attempt == 1;
            g_win = SDL_CreateWindow("Позывной: Гром", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, wflags | SDL_WINDOW_OPENGL);
            if (!g_win) continue;
            if (glInitRenderer(g_win)) { g_useGL = true; break; }
            SDL_DestroyWindow(g_win); g_win = nullptr;
        }
        if (g_useGL) SDL_GL_SetSwapInterval(1);
        else SDL_Log("OpenGL недоступен — программный рендер");
    }
    if (!g_useGL) {
        g_win = SDL_CreateWindow("Позывной: Гром", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, wflags);
        if (!g_win) { SDL_Log("Window: %s", SDL_GetError()); SDL_Quit(); return 1; }
        g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
        if (!g_ren) { SDL_Log("Renderer: %s", SDL_GetError()); SDL_DestroyWindow(g_win); SDL_Quit(); return 1; }
        applyQuality();
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        g_texUI = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, UIW, UIH);
        if (!g_texScene || !g_texUI) { SDL_Log("Texture: %s", SDL_GetError()); SDL_Quit(); return 1; }
        SDL_SetTextureBlendMode(g_texUI, SDL_BLENDMODE_BLEND);
    } else rendResize(320, 180);
    updateDst();
    for (int i = 0; i < SDL_NumJoysticks(); i++) openPad(i);
    g_amb.master = S.volume / 10.0f * 0.9f;
    Uint32 last = SDL_GetTicks();
    float fpsAcc = 0; int fpsN = 0;
    while (!g_quit) {
        Uint32 now = SDL_GetTicks();
        float dt = clampf((now - last) / 1000.0f, 0.0005f, 0.05f);
        last = now;
        fpsAcc += dt; fpsN++;
        if (fpsAcc > 0.5f) { G.fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0; }
        IN = Input();
        g_tapX = g_tapY = -1;
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);
        pollInput(dt);
        g_menuT += dt;
        memset(g_ui, 0, sizeof(g_ui));
        bool scene = false;
        int scr = G.screen;
        if (scr == SCR_PLAY) {
            if (IN.pause) { G.screen = SCR_PAUSE; g_selPause = 0; IN.pause = false; IN.back = false; }   // иначе меню паузы сразу закрывалось тем же нажатием
            else gameUpdate(dt);
        }
        if (scr == SCR_CUTSCENE) { csUpdate(dt); scr = G.screen; }
        if (g_csJustDead) { g_csJustDead = false; g_selDead = 0; g_deadT = 0; g_csSeen = false; }
        if (scr == SCR_CUTSCENE) { renderCutscene(dt); scene = true; csDrawUI(); }
        scr = G.screen;
        if (scr == SCR_PLAY || scr == SCR_PAUSE || scr == SCR_DEAD || scr == SCR_DEFUSE || (scr == SCR_SETTINGS && g_settingsReturn == SCR_PAUSE)) {
            renderPlay(dt);
            scene = true;
            if (scr == SCR_PLAY) { drawHUD(); drawStory(); drawTouch(); }
            if (G.fadeIn > 0) { uiRect(0, 0, UIW, UIH, ARGBc((int)(255 * G.fadeIn), 0, 0, 0)); G.fadeIn -= dt * 1.5f; }
        }
        g_uiOX = 0;
        switch (G.screen) {
        case SCR_MENU: screenMenu(); break;
        case SCR_SELECT: screenSelect(); break;
        case SCR_SETTINGS: screenSettings(); break;
        case SCR_BRIEF: screenBrief(dt); break;
        case SCR_PAUSE: screenPause(); break;
        case SCR_DEAD: screenDead(dt); break;
        case SCR_DEBRIEF: screenDebrief(); break;
        case SCR_EPILOGUE: screenEpilogue(dt); break;
        case SCR_DEFUSE: screenDefuse(dt); break;
        default: break;
        }
        g_uiOX = 0;
        if (g_padToastT > 0) {
            g_padToastT -= dt;
            float a = clampf(g_padToastT, 0, 1);
            int w = (int)g_padToast.size() * 4 + 30;
            uiRect(UIW / 2 - w / 2, UIH - 64, w, 18, ARGBc((int)(170 * a), 10, 12, 14));
            uiTextC(UIW / 2, UIH - 58, g_padToast.c_str(), ARGBc((int)(255 * a), 140, 230, 160));
        }
        if (G.screen != SCR_PLAY) { g_amb.breath = 0; g_amb.heart = 0; }
        audioSync();
        if (g_useGL) { glPresent(scene, g_winW, g_winH); SDL_GL_SwapWindow(g_win); continue; }
        SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
        SDL_RenderClear(g_ren);
        if (scene) { SDL_UpdateTexture(g_texScene, nullptr, g_fb.data(), RW * 4); SDL_RenderCopy(g_ren, g_texScene, nullptr, &g_dst); }
        SDL_UpdateTexture(g_texUI, nullptr, g_ui, UIW * 4);
        SDL_RenderCopy(g_ren, g_texUI, nullptr, &g_dst);
        SDL_RenderPresent(g_ren);
    }
    saveAll();
    if (g_pad) SDL_GameControllerClose(g_pad);
    if (g_adev) SDL_CloseAudioDevice(g_adev);
    if (g_useGL) { SDL_GL_DeleteContext(g_glc); }
    else { SDL_DestroyTexture(g_texUI); SDL_DestroyTexture(g_texScene); SDL_DestroyRenderer(g_ren); }
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    return 0;
}
