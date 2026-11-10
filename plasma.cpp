// plasma.cpp
// Compile: g++ -O2 -o plasma plasma.cpp $(sdl2-config --cflags --libs) \
//          $(sdl2_ttf-config --cflags --libs) $(sdl2_mixer-config --cflags --libs) -lm
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cstdio>

#ifndef M_2PI
#define M_2PI 6.28318530717958647692f
#endif

// ════════════════════════════════════════════════════════════════════════════
//  Utility helpers
// ════════════════════════════════════════════════════════════════════════════
static void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (s < 1e-6f) { r = g = b = (uint8_t)(v * 255.0f); return; }
    h /= 60.0f;
    int sector = (int)h % 6;
    float f = h - (float)((int)h);
    uint8_t v255 = (uint8_t)(v * 255.0f);
    uint8_t p = (uint8_t)(v * (1.0f - s) * 255.0f);
    uint8_t q = (uint8_t)(v * (1.0f - s * f) * 255.0f);
    uint8_t t = (uint8_t)(v * (1.0f - s * (1.0f - f)) * 255.0f);
    switch (sector) {
        case 0:  r=v255; g=t;   b=p; break;
        case 1:  r=q;   g=v255; b=p; break;
        case 2:  r=p;   g=v255; b=t; break;
        case 3:  r=p;   g=q;   b=v255;break;
        case 4:  r=t;   g=p;   b=v255;break;
        default: r=v255;g=p;   b=q; break;
    }
}

// Draw a thick outline around src by blitting it in 8 directions.
static void draw_outline(SDL_Surface *dst, const SDL_Rect &dstRect,
                         const SDL_Surface *src, int thickness) {
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) continue;
            SDL_Rect r = dstRect;
            r.x += dx;
            r.y += dy;
            SDL_BlitSurface(src, nullptr, dst, &r);
        }
    }
}

// Build a rainbow-coloured copy of a white-text mask.
// Each pixel's hue is shifted by its world-x position → flowing rainbow ribbon.
static SDL_Surface *make_rainbow_text(const SDL_Surface *mask, float ribbon_x) {
    int w = mask->w, h = mask->h;
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!out) return nullptr;

    uint32_t *dstPx  = (uint32_t *)out->pixels;
    const uint32_t *srcPx = (const uint32_t *)mask->pixels;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t sp = srcPx[y * w + x];
            uint8_t a   = (sp >> 24) & 0xFF;
            if (a == 0) continue;

            float hue = fmod((float)(ribbon_x + x) * 0.7f, 360.0f);
            if (hue < 0) hue += 360.0f;
            uint8_t r, g, b;
            hsv_to_rgb(hue, 1.0f, 1.0f, r, g, b);
            dstPx[y * w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════════
//  Per-glyph precomputed data
// ════════════════════════════════════════════════════════════════════════════
struct Glyph {
    SDL_Surface *mask     = nullptr;   // white text on transparent
    SDL_Surface *outline  = nullptr;   // black outline (pre-rendered)
    SDL_Surface *shadow   = nullptr;   // dark shadow (pre-rendered)
    int width              = 0;
};

// ════════════════════════════════════════════════════════════════════════════
//  PlasmaEffect
// ════════════════════════════════════════════════════════════════════════════
class PlasmaEffect {
public:
    // Low-res buffer dimensions
    int width, height;

    // Animation state
    float time          = 0.0f;
    float speed         = 1.0f;
    float palette_speed = 180.0f;
    float intensity     = 1.6f;

    // Scroller settings
    static constexpr int   scroller_font_size       = 42;
    static constexpr float scroller_speed           = 50.0f;
    static constexpr float scroller_base_y          = 130.0f;
    static constexpr float scroller_amplitude       = 10.0f;
    static constexpr float scroller_wavelength      = 240.0f;
    static constexpr float scroller_wave_speed      = 2.0f;
    static constexpr float scroller_letter_spacing  = 2.0f;
    static constexpr int   scroller_outline_thickness = 1;
    static constexpr int   scroller_gap             = 40;

    // Precomputed numpy-equivalent arrays (row-major, width × height)
    std::vector<float> x_arr, y_arr, dist_arr;

    // Palette state
    float palette_hold_time        = 5.0f;
    float palette_transition_time  = 2.0f;
    int   current_palette_index    = 0;
    int   next_palette_index       = 1;
    float palette_timer            = 0.0f;

    // Palettes: each is 256 × 3 bytes (RGB)
    std::vector<std::array<uint8_t, 3>> palettes[4];

    // Scroller glyphs
    std::string scroller_text = "RETRO PLASMA ★";
    std::vector<Glyph> scroller_glyphs;
    int scroller_width = 0;   // total width of one text copy

    // Render surface (low-res)
    SDL_Surface *surface = nullptr;
    TTF_Font *font      = nullptr;

    PlasmaEffect(int w, int h) : width(w), height(h) {
        build_coordinate_arrays();
        build_palettes();
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        load_scroller_glyphs();
    }

    ~PlasmaEffect() {
        for (auto &g : scroller_glyphs) {
            if (g.mask)    SDL_FreeSurface(g.mask);
            if (g.outline) SDL_FreeSurface(g.outline);
            if (g.shadow)  SDL_FreeSurface(g.shadow);
        }
        if (surface) SDL_FreeSurface(surface);
        if (font)    TTF_CloseFont(font);
    }

    // ── Coordinate grid (equivalent of numpy arange + meshgrid) ──────────────
    void build_coordinate_arrays() {
        int n = width * height;
        x_arr.resize(n);
        y_arr.resize(n);
        dist_arr.resize(n);
        float cx = width  * 0.5f;
        float cy = height * 0.5f;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int i = y * width + x;
                x_arr[i]    = (float)x;
                y_arr[i]    = (float)y;
                dist_arr[i] = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            }
        }
    }

    // ── Palette generation (equivalent of Python colorsys hsv_to_rgb loops) ─
    void build_palettes() {
        auto make_palette = [](int type) -> std::array<uint8_t, 3> * {
            static std::vector<std::array<uint8_t,3>> buf;
            return nullptr;
        };

        // Palette 0: classic rainbow
        palettes[0].resize(256);
        for (int i = 0; i < 256; ++i) {
            float h = fmod(i * 360.0f / 256.0f, 360.0f);
            uint8_t r,g,b; hsv_to_rgb(h, 1.0f, 1.0f, r,g,b);
            palettes[0][i] = {r,g,b};
        }

        // Palette 1: fire / hot
        palettes[1].resize(256);
        for (int i = 0; i < 256; ++i) {
            float h = fmod(30.0f + i * 60.0f / 256.0f, 360.0f);
            float v = 0.5f + 0.5f * (float)i / 255.0f;
            uint8_t r,g,b; hsv_to_rgb(h, 1.0f, v, r,g,b);
            palettes[1][i] = {r,g,b};
        }

        // Palette 2: ocean / cool
        palettes[2].resize(256);
        for (int i = 0; i < 256; ++i) {
            float h = fmod(180.0f + i * 90.0f / 256.0f, 360.0f);
            uint8_t r,g,b; hsv_to_rgb(h, 0.85f, 1.0f, r,g,b);
            palettes[2][i] = {r,g,b};
        }

        // Palette 3: neon / synthwave
        palettes[3].resize(256);
        for (int i = 0; i < 256; ++i) {
            float h = fmod(i * 720.0f / 256.0f, 360.0f);
            float s = 1.0f;
            uint8_t r,g,b; hsv_to_rgb(h, s, 1.0f, r,g,b);
            palettes[3][i] = {r,g,b};
        }
    }

    const std::array<uint8_t,3> &get_palette(int idx) const {
        return palettes[idx % 4];
    }

    // ── Scroller glyph precomputation ────────────────────────────────────────
    void load_scroller_glyphs() {
        font = TTF_OpenFont("arial.ttf", scroller_font_size);
        if (!font) font = TTF_OpenFont(0, scroller_font_size); // fallback

        SDL_Color white{255,255,255,255};
        SDL_Color black{  0,  0,  0,255};
        SDL_Color shadow_c{ 0,  0,  0,180};

        scroller_glyphs.clear();
        scroller_width = 0;

        for (char ch : scroller_text) {
            Glyph g;
            // Render white glyph
            g.mask = TTF_RenderUTF8Blended(font, std::string(1, ch).c_str(), white, black);
            if (!g.mask) continue;
            g.width = g.mask->w;

            // Pre-render outline (black, thickened)
            int pad = scroller_outline_thickness;
            int ow = g.mask->w + 2 * pad;
            int oh = g.mask->h + 2 * pad;
            g.outline = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
            if (g.outline) {
                SDL_FillSurface(g.outline, 0);
                SDL_Rect center{pad, pad, g.mask->w, g.mask->h};
                // Draw outline: blit mask in black at offsets
                for (int dy = -scroller_outline_thickness; dy <= scroller_outline_thickness; ++dy) {
                    for (int dx = -scroller_outline_thickness; dx <= scroller_outline_thickness; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        SDL_Rect r = center;
                        r.x += dx; r.y += dy;
                        // Temporarily tint: just blit mask as-is (white on transparent)
                        // We want black outline, so create a black version first
                    }
                }
                // Simpler: create a black-tinted copy of the mask
                SDL_Surface *black_mask = TTF_RenderUTF8Blended(font, std::string(1,ch).c_str(), black, black);
                if (black_mask) {
                    for (int dy = -scroller_outline_thickness; dy <= scroller_outline_thickness; ++dy) {
                        for (int dx = -scroller_outline_thickness; dx <= scroller_outline_thickness; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            SDL_Rect r{pad + dx, pad + dy, black_mask->w, black_mask->h};
                            SDL_BlitSurface(black_mask, nullptr, g.outline, &r);
                        }
                    }
                    SDL_FreeSurface(black_mask);
                }

                // Shadow: offset copy with lower alpha
                g.shadow = TTF_RenderUTF8Blended(font, std::string(1,ch).c_str(), shadow_c, black);
            }

            scroller_width += g.width + (int)scroller_letter_spacing;
            scroller_glyphs.push_back(std::move(g));
        }
    }

    // ── Draw one copy of the scroller text at a given x offset ───────────────
    void draw_scroller_copy(SDL_Surface *target, float x_offset) {
        float cursor_x = x_offset;
        float ribbon_x = 0.0f;

        for (const auto &g : scroller_glyphs) {
            if (!g.mask) continue;

            int gw = g.width;
            float center_x = cursor_x + gw * 0.5f;

            // Wave phase
            float phase = (center_x / scroller_wavelength) * M_2PI
                         + time * scroller_wave_speed;
            float center_y = scroller_base_y + std::sin(phase) * scroller_amplitude;

            int x = (int)cursor_x;
            int y = (int)(center_y - g.mask->h * 0.5f);

            // Skip if fully off-screen
            if (x + gw >= 0 && x < width) {
                // Rainbow text
                SDL_Surface *rainbow = make_SDL_Surface *rainbow = make_rainbow_text(g.mask, ribbon_x);
                if (rainbow) {
                    // Shadow (slight down-right offset)
                    if (g.shadow) {
                        SDL_Rect sr;
                        sr.x = x + 2;
                        sr.y = y + 2;
                        sr.w = gw;
                        sr.h = g.mask->h;
                        SDL_BlitSurface(g.shadow, nullptr, target, &sr);
                    }

                    // Black outline in all directions
                    SDL_Rect r;
                    r.x = x;
                    r.y = y;
                    r.w = gw;
                    r.h = g.mask->h;
                    draw_outline(target, r, rainbow, scroller_outline_thickness);

                    // Rainbow text on top
                    SDL_BlitSurface(rainbow, nullptr, target, &r);

                    SDL_FreeSurface(rainbow);
                }
            }

            ribbon_x += gw + (int)scroller_letter_spacing;
            cursor_x += gw + (int)scroller_letter_spacing;
        }
    }

    // ── Render the full scroller (two copies for seamless wrap) ──────────────
    void render_scroller(SDL_Surface *target) {
        int total_w = scroller_width;
        if (total_w <= 0) return;

        float scroll_x = std::fmod(time * scroller_speed, (float)total_w);

        // Draw two copies so there's always text on screen
        draw_scroller_copy(target, -scroll_x);
        draw_scroller_copy(target, total_w - scroll_x);
    }

    // ── Main render: plasma background + scroller overlay ────────────────────
    void render(SDL_Surface *screen) {
        int w = width, h = height;
        uint32_t *px = (uint32_t *)surface->pixels;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float dist = distanceGrid[y * gridSize + x] * intensity;
                int idx = (int)(std::fmod(dist, 256.0f) + 256.0f) % 256;
                uint32_t c = palette[idx];
                px[y * w + x] = c;
            }
        }

        // Draw scroller on top of plasma
        render_scroller(surface);

        // Blit to screen (nearest-neighbour upscale)
        SDL_BlitSurface(surface, nullptr, screen, nullptr);
    }

    void tick(float dt) {
        time += dt;
        update_plasma_grid();
    }

private:
    uint32_t palette[256]; // current blended palette (uint32 ARGB)

    void build_palette() {
        int cur = numPalettes > 0 ? (int)(std::fmod(time / (paletteHoldTime + paletteTransitionTime), (float)numPalettes)) % numPalettes : 0;
        int next = (cur + 1) % numPalettes;

        float hold = std::fmod(time, paletteHoldTime + paletteTransitionTime);
        float t = (hold > paletteHoldTime) ? (hold - paletteHoldTime) / paletteTransitionTime : 0.0f;
        t = t * t * (3.0f - 2.0f * t); // smoothstep

        for (int i = 0; i < 256; ++i) {
            uint8_t r = palettes[cur][i][0] + (palettes[next][i][0] - palettes[cur][i][0]) * (int)t;
            uint8_t g = palettes[cur][i][1] + (palettes[next][i][1] - palettes[cur][i][1]) * (int)t;
            uint8_t b = palettes[cur][i][2] + (palettes[next][i][2] - palettes[cur][i][2]) * (int)t;
            palette[i] = SDL_MakeRGB(r, g, b);
        }
    }

    void update_plasma_grid() {
        build_palette();

        float t = time * speed;
        for (int gy = 0; gy < gridSize; ++gy) {
            for (int gx = 0; gx < gridSize; ++gx) {
                float x = (float)gx / (gridSize - 1);
                float y = (float)gy / (gridSize - 1);

                float v = std::sin(x * 6.28f + t)
                        + std::sin(y * 4.0f + t * 0.7f)
                        + std::sin((x + y) * 5.0f + t * 0.5f)
                        + std::sin(std::sqrt(x*x + y*y) * 8.0f - t);
                distanceGrid[gy * gridSize + gx] = v;
            }
        }
    }
};


// ─── Glyph data ──────────────────────────────────────────────────────────────
struct Glyph {
    SDL_Surface *mask;      // white text, alpha mask
    SDL_Surface *shadow;    // semi-transparent shadow
    int width;
    bool empty;             // true for spaces
};

// ─── PlasmaEffect ────────────────────────────────────────────────────────────
class PlasmaEffect {
public:
    int width = 320, height = 200;
    float time = 0.0f;
    float speed = 1.0f;
    float intensity = 1.0f;
    SDL_Surface *surface = nullptr;

    // Plasma grid
    std::vector<float> distanceGrid;
    int gridSize = 320;

    // Palettes (procedural)
    float paletteHoldTime = 5.0f;
    float paletteTransitionTime = 2.0f;
    std::vector<std::array<uint8_t,3>> palettes[10];
    int numPalettes = 0;
    uint32_t palette[256] = {};

    // Scroller
    const char *scrollerText = "HELLO PLASMA WORLD";
    float scrollerBaseY = 100.0f;
    float scrollerAmplitude = 6.0f;
    float scrollerWavelength = 100.0f;
    float scrollerWaveSpeed = 1.0f;
    float scrollerSpeed = 100.0f;
    int scrollerLetterSpacing = 2;
    float scrollerRainbowWavelength = 300.0f;
    int scrollerOutlineThickness = 2;

    TTF_Font *font = nullptr;
    std::vector<Glyph> scrollerGlyphs;
    int scrollerWidth = 0;

    // ── Init ──────────────────────────────────────────────────────────────
    bool init(const char *fontPath, int fontSize) {
        if (TTF_Init() < 0) return false;
        font = TTF_OpenFont(fontPath, fontSize);
        if (!font) { printf("Font error: %s\n", TTF_GetError()); return false; }

        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface) return false;

        distanceGrid.resize(gridSize * gridSize);
        generate_palettes();
        build_scroller_glyphs();
        return true;
    }

    void cleanup() {
        for (auto &g : scrollerGlyphs) {
            if (g.mask)   SDL_FreeSurface(g.mask);
            if (g.shadow) SDL_FreeSurface(g.shadow);
        }
        if (surface) SDL_FreeSurface(surface);
        if (font)    TTF_CloseFont(font);
        TTF_Quit();
    }

    // ── Palette generation (simple procedural gradients) ──────────────────
    void generate_palettes() {
        // 0: classic rainbow
        for (int i = 0; i < 256; ++i) palettes[0][i] = {
            (uint8_t)(127+127*sin(i*0.049f)), (uint8_t)(127+127*sin(i*0.049f+2.09f)), (uint8_t)(127+127*sin(i*0.049f+4.19f))};
        // 1: fire
        for (int i = 0; i < 256; ++i) palettes[1][i] = {
            (uint8_t)(i), (uint8_t)(i*0.3f), (uint8_t)(255-i)};
        // 2: ocean
        for (int i = 0; i < 256; ++i) palettes[2][i] = {
            (uint8_t)(i*0.1f), (uint8_t)(i*0.5f+50), (uint8_t)(i*0.8f)};
        // 3: neon
        for (int i = 0; i < 256; ++i) palettes[3][i] = {
            (uint8_t)(127+127*sin(i*0.098f)), (uint8_t)(127+127*sin(i*0.098f+1.5f)), (uint8_t)(255)};
        numPalettes = 4;
    }

    void build_palette() {
        if (numPalettes == 0) return;
        float cycle = paletteHoldTime + paletteTransitionTime;
        int cur  = (int)((time / cycle) % numPalettes);
        int next = (cur + 1) % numPalettes;
        float hold = std::fmod(time, cycle);
        float t = (hold > paletteHoldTime) ? (hold - paletteHoldTime) / paletteTransitionTime : 0.0f;
        t = t * t * (3.0f - 2.0f * t); // smoothstep

        for (int i = 0; i < 256; ++i) {
            uint8_t r = palettes[cur][i][0] + ((palettes[next][i][0] - palettes[cur][i][0]) * (int)(t * 255.0f)) / 255;
            uint8_t g = palettes[cur][i][1] + ((palettes[next][i][1] - palettes[cur][i][1]) * (int)(t * 255.0f)) / 255;
            uint8_t b = palettes[cur][i][2] + ((palettes[next][i][2] - palettes[cur][i][2]) * (int)(t * 255.0f)) / 255;
            palette[i] = SDL_MakeRGB(r, g, b);
        }
    }

    // ── Scroller glyph construction ───────────────────────────────────────
    void build_scroller_glyphs() {
        scrollerGlyphs.clear();
        scrollerWidth = 0;
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color black = {0, 0, 0, 255};

        for (const char *p = scrollerText; *p; ++p) {
            char ch = *p;
            Glyph g{};
            if (ch == ' ') {
                // Measure space width
                SDL_Surface *tmp = TTF_RenderUTF8Blended(font, " ", white, black);
                g.width = tmp ? tmp->w : 1;
                if (tmp) SDL_FreeSurface(tmp);
                g.empty = true;
            } else {
                std::string str(1, ch);
                g.mask = TTF_RenderUTF8Blended(font, str.c_str(), white, black);
                if (!g.mask) { g.empty = true; g.width = 1; }
                else {
                    g.width = g.mask->w;
                    // Shadow: dark semi-transparent copy
                    SDL_Color shadC = {0, 0, 0, 128};
                    g.shadow = TTF_RenderUTF8Blended(font, str.c_str(), shadC, black);
                }
            }
            scrollerWidth += g.width + scrollerLetterSpacing;
            scrollerGlyphs.push_back(std::move(g));
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────
    static void draw_outline(SDL_Surface *dst, const SDL_Rect &r,
                            const SDL_Surface *src, int thickness) {
        for (int dy = -thickness; dy <= thickness; ++dy)
            for (int dx = -thickness; dx <= thickness; ++dx) {
                if (!dx && !dy) continue;
                SDL_Rect rr = r; rr.x += dx; rr.y += dy;
                SDL_BlitSurface(src, nullptr, dst, &rr);
            }
    }

    static SDL_Surface *make_rainbow_text(const SDL_Surface *mask, float ribbon_x) {
        int w = mask->w, h = mask->h;
        SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!out) return nullptr;
        const uint32_t *sp = (const uint32_t *)mask->pixels;
        uint32_t *dp       = (uint32_t *)out->pixels;

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint32_t p = sp[y * w + x];
                uint8_t a  = (p >> 24) & 0xFF;
                if (!a) continue;
                // Hue based on world-x → flowing rainbow
                float hue = ((float)(x + ribbon_x) / 300.0f + time * 0.18f) * M_2PI;
                uint8_t r = (uint8_t)(128 + 127 * sin(hue));
                uint8_t g = (uint8_t)(128 + 127 * sin(hue + 2.094f));
                uint8_t b = (uint8_t)(128 + 127 * sin(hue + 4.189f));
                dp[y * w + x] = SDL_MakeRGBA(r, g, b, a);
            }
        return out;
    }

    // ── Draw one copy of scroller at given x offset ───────────────────────
    void draw_scroller_copy(SDL_Surface *target, float x_offset) {
        float cursor_x = x_offset;
        float ribbon_x = 0.0f;

        for (const auto &g : scrollerGlyphs) {
            if (!g.mask) continue;
            int gw = g.width;
            float center_x = cursor_x + gw * 0.5f;
            float phase = (center_x / scrollerWavelength) * M_2PI + time * scrollerWaveSpeed;
            float center_y = scrollerBaseY + std::sin(phase) * scrollerAmplitude;
            int x = (int)cursor_x;
            int y = (int)(center_y - g.mask->h * 0.5f);

            // Skip if fully off-screen
            if (x + gw >= 0 && x < width) {
                SDL_Surface *rainbow = make_rainbow_text(g.mask, ribbon_x);
                if (rainbow) {
                    // Shadow
                    if (g.shadow) {
                        SDL_Rect sr{x + 2, y + 2, gw, g.mask->h};
                        SDL_BlitSurface(g.shadow, nullptr, target, &sr);
                    }
                    // Outline
                    SDL_Rect r{x, y, gw, g.mask->h};
                    draw_outline(target, r, rainbow, scrollerOutlineThickness);
                    // Main text
                    SDL_BlitSurface(rainbow, nullptr, target, &r);
                    SDL_FreeSurface(rainbow);
                }
            }
            ribbon_x += gw + scrollerLetterSpacing;
            cursor_x += gw + scrollerLetterSpacing;
        }
    }

    // ── Render full frame ─────────────────────────────────────────────────
    void render(SDL_Surface *screen) {
        uint32_t *px = (uint32_t *)surface->pixels;
        build_palette();

        // Plasma
        float t = time * speed;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                float fx = (float)x / width * 6.28f;
                float fy = (float)y / height * 4.0f;
                float v = std::sin(fx + t)
                        + std::sin(fy + t * 0.7f)
                        + std::sin((fx + fy) * 0.5f + t * 0.4f)
                        + std::sin(std::sqrt(x*x + y*y) * 0.05f - t);
                int idx = (int)(std::fmod(v * intensity, 256.0f) + 256.0f) % 256;
                px[y * width + x] = palette[idx];
            }

        // Scroller: two copies for seamless wrap
        float total_w = (float)scrollerWidth;
        float offset  = std::fmod(time * scrollerSpeed, total_w);
        draw_scroller_copy(surface, -offset);
        draw_scroller_copy(surface, -offset + total_w);

        // Blit to screen
        SDL_BlitSurface(surface, nullptr, screen, nullptr);
    }

    void update(float dt) { time += dt; }
};

// ─── Main ────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window *win = SDL_CreateWindow("Plasma Scroller",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        320, 200, SDL_WINDOW_SHOWN);
    if (!win) { SDL_Quit(); return 1; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 320, 200);

    PlasmaEffect plasma;
    // Try common font paths
    const char *fontPath = nullptr;
    for (const char *p : {"/usr/share/fonts/TTF/DejaVuSans.ttf"}) {
        if (SDL_RWops *f = SDL_RWFromFile(p, "rb")) { fontPath = p; SDL_RWclose(f); break; }
    }
    if (!fontPath) { printf("No font found\n"); return 1; }

    if (!plasma.init(fontPath, 28)) {
        SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win); SDL_Quit(); return 1;
    }

    bool running = true;
    Uint32 lastTicks = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - lastTicks) / 1000.0f;
        lastTicks = now;

        plasma.update(dt);
        plasma.render((SDL_Surface *)tex); // stream to texture via surface blit
        // Actually: render into a surface then lock texture
        // Simpler: use SDL_UpdateTexture with the surface pixels
        uint8_t *dst; int pitch;
        SDL_LockTexture(tex, nullptr, (void **)&dst, &pitch);
        memcpy(dst, plasma.surface->pixels, 320 * 200 * 4);
        SDL_UnlockTexture(tex);

        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }

    plasma.cleanup();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
