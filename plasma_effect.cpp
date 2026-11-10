// plasma_effect.cpp  (täydellinen)
#include "plasma_effect.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>

#ifndef M_TAU
#define M_TAU 6.2831853071795864769f
#endif

namespace {
    void blit_clipped(SDL_Surface* dst, SDL_Surface* src, int dx, int dy) {
        if (dx >= dst->w || dy >= dst->h) return;
        if (dx + src->w <= 0 || dy + src->h <= 0) return;

        SDL_Rect r{dx, dy, src->w, src->h};
        SDL_BlitSurface(src, nullptr, dst, &r);
    }
}

// ═══════════════════════════════════════════════════════
//  CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════
PlasmaEffect::PlasmaEffect(int width, int height)
    : width(width), height(height),
      surface(SDL_CreateRGBSurface(0, width, height, 32,
                                   0x00FF0000, 0x0000FF00,
                                   0x000000FF, 0xFF000000)),
      time(0.0f), speed(1.0f), palette_speed(180.0f), intensity(1.6f),
      palette_hold_time(5.0f), palette_transition_time(2.0f)
{
    // Koordinaatit
    x_coords.resize(width);
    y_coords.resize(height);
    for (int i = 0; i < width;  ++i) x_coords[i] = static_cast<float>(i);
    for (int j = 0; j < height; ++j) y_coords[j] = static_cast<float>(j);

    const float cx = width * 0.5f, cy = height * 0.5f;
    distance.resize(static_cast<size_t>(width) * height);
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i) {
            float dx = x_coords[i] - cx, dy = y_coords[j] - cy;
            distance[static_cast<size_t>(j)*width + i] = std::sqrt(dx*dx+dy*dy);
        }

    // Paletit
    palettes.push_back(create_duotone_palette({0,0,255},{20,20,50}));
    palettes.push_back(create_duotone_palette({255,0,0},{50,20,20}));
    palettes.push_back(create_duotone_palette({255,255,20},{50,50,20}));
    palettes.push_back(create_duotone_palette({15,0,90},{255,0,180}));
    palettes.push_back(create_duotone_palette({0,30,120},{0,255,210}));
    palettes.push_back(create_duotone_palette({120,0,20},{255,180,0}));
    palettes.push_back(create_duotone_palette({20,100,0},{180,255,20}));
    palettes.push_back(create_grayscale_palette(16));
    palettes.push_back(create_rainbow_palette(32));
    palettes.push_back(create_rainbow_palette(255));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, palettes.size()-1);
    current_palette = palettes[dist(gen)];

    // Fontti
    TTF_Init();
    font = TTF_OpenFont("scroller.ttf", 26);
    if (!font) font = TTF_OpenFont(nullptr, 26);

    scroller_text =
        "    OLD SCHOOL PLASMA AND RAINBOW SCROLLER"
        "  MADE TO HONOR THE OLD BEARDS OF THE SCENE  "
        "      YOU ARE KINGS             "
        "  HAILS TO FAIRLIGHT :: FARBRAUSCH :: NEURO ::  "
        "RAZOR1911 :: SPACEBALLS :: ANDROMEDA SOFTWARE DEVELOPMENT :: "
        "KEWLERZ :: MFX :: MOPPI       JUST TO MENTION A FEW ... YOU KNOW WHO YOU ARE     "
        "KUDOS TO THE OG CODERS AND TRACKER MUSICIANS - "
        "STAY TUNED FOR MORE PSYCHEDELIC VISIONS   "
        "CONCEIVED, DIRECTED AND MUSIC BY PROMETHEUS -- CODE FORGED WITH THE CHATGPT GREMLIN ***   ";

    scroller_base_y = 100.0f;
    scroller_amplitude = 6.0f;
    scroller_wavelength = 100.0f;
    scroller_wave_speed = 1.0f;
    scroller_speed = 100.0f;
    scroller_letter_spacing = 1;
    scroller_gap = 0;
    scroller_rainbow_speed = 0.8f;
    scroller_rainbow_density = 1.8f;
    scroller_rainbow_brightness = 1.0f;
    scroller_rainbow_saturation = 1.0f;
    scroller_rainbow_wavelength = 300.0f;
    scroller_rainbow_levels = 32;
    scroller_outline_thickness = 2;
    scroller_outline_alpha = 220;

    build_scroller_glyphs();
}

PlasmaEffect::~PlasmaEffect() {
    for (auto& g : scroller_glyphs) {
        if (g.mask)    SDL_FreeSurface(g.mask);
        if (g.outline) SDL_FreeSurface(g.outline);
        if (g.shadow)  SDL_FreeSurface(g.shadow);
    }
    scroller_glyphs.clear();
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    if (surface) { SDL_FreeSurface(surface); surface = nullptr; }
}

// ═══════════════════════════════════════════════════════
//  ELINKAARI
// ═══════════════════════════════════════════════════════
void PlasmaEffect::start() {
    time = 0.0f;
}

void PlasmaEffect::update(float dt) {
    time += dt * speed;
    // Intensity palaa 1.0:een (audio-kick lisätään myöhemmin)
    intensity += (1.0f - intensity) * std::min(1.0f, dt * 4.0f);
}

// ═══════════════════════════════════════════════════════
//  RENDER – plasma + scroller → surface → renderer
// ═══════════════════════════════════════════════════════
void PlasmaEffect::render(SDL_Renderer* renderer) {
    compute_plasma();
    draw_scroller(surface);

    // Skaalattu blitta ikkunan kokoiseksi
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
        SDL_RenderCopy(renderer, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
}

// ═══════════════════════════════════════════════════════
//  PLASMA-LASKENTA → surface-pikselit
// ═══════════════════════════════════════════════════════
void PlasmaEffect::compute_plasma() {
    const float t = time;
    const int   palette_offset = static_cast<int>(t * palette_speed) % 256;

    Palette pal = get_current_palette();

    SDL_LockSurface(surface);
    uint8_t* px = static_cast<uint8_t*>(surface->pixels);

    for (int j = 0; j < height; ++j) {
        float y = y_coords[j];
        for (int i = 0; i < width; ++i) {
            float x = x_coords[i];
            float d = distance[static_cast<size_t>(j)*width + i];

            // Viivetoiminto (4 sin-termiä)
            float plasma =
                std::sin(x * 0.045f + t * 1.30f) +
                std::sin(y * 0.060f - t * 1.10f) +
                std::sin((x + y) * 0.035f + t * 0.75f) +
                std::sin(d * 0.080f - t * 1.65f);

            plasma *= intensity;

            // Paletti-indeksi: [-4,+4] → [0,255], truncation toward zero
            int idx = static_cast<int>((plasma + 4.0f) * 31.875f);

            // Kiertävä offset + mod 256 (C++ % voi olla negatiivinen)
            idx = ((idx + palette_offset) % 256 + 256) % 256;

            const Color& c = pal[idx];
            size_t p = (static_cast<size_t>(j)*width + i) * 4;
            px[p + 0] = c.b;   // B
            px[p + 1] = c.g;   // G
            px[p + 2] = c.r;   // R
            px[p + 3] = 255;   // A
        }
    }
    SDL_UnlockSurface(surface);
}

// ═══════════════════════════════════════════════════════
//  PALETTI
// ═══════════════════════════════════════════════════════
Palette PlasmaEffect::get_current_palette() const {
    float cycle_length = palette_hold_time + palette_transition_time;
    int   cycle_index  = static_cast<int>(time / cycle_length);

    size_t cur_idx  = static_cast<size_t>(cycle_index) % palettes.size();
    size_t next_idx = (cur_idx + 1) % palettes.size();

    float cycle_time = std::fmod(time, cycle_length);

    if (cycle_time < palette_hold_time)
        return palettes[cur_idx];

    // Smoothstep-blend
    float blend = (cycle_time - palette_hold_time) / palette_transition_time;
    blend = blend * blend * (3.0f - 2.0f * blend);

    const Palette& A = palettes[cur_idx];
    const Palette& B = palettes[next_idx];

    Palette result{};
    for (int i = 0; i < 256; ++i) {
        float inv = 1.0f - blend;
        result[i] = {
            static_cast<uint8_t>(A[i].r * inv + B[i].r * blend),
            static_cast<uint8_t>(A[i].g * inv + B[i].g * blend),
            static_cast<uint8_t>(A[i].b * inv + B[i].b * blend)
        };
    }
    return result;
}

// ═══════════════════════════════════════════════════════
//  SCROLLER – draw_scroller (looping copies)
// ═══════════════════════════════════════════════════════
void PlasmaEffect::draw_scroller(SDL_Surface* target) {
    float cycle_width = scroller_width + scroller_gap;
    float distance_   = time * scroller_speed;

    // Ensimmäinen kopio alkaa oikean reunan ulkopuolelta
    float first_x = static_cast<float>(width) - distance_;

    // Kun teksti on kokonaan vasemmalla pois, kierrä eteenpäin
    while (first_x + cycle_width < 0.0f)
        first_x += cycle_width;

    float x = first_x;
    while (x < static_cast<float>(width)) {
        draw_scroller_copy(target, x);
        x += cycle_width;
    }
}

// ═══════════════════════════════════════════════════════
//  SCROLLER – draw_scroller_copy (yksi teksti-kopio)
// ═══════════════════════════════════════════════════════
void PlasmaEffect::draw_scroller_copy(SDL_Surface* target, float start_x) {
    float cursor_x = start_x;
    float ribbon_x = 0.0f;

    for (const auto& glyph : scroller_glyphs) {
        int gw = glyph.width;
        float center_x = cursor_x + gw * 0.5f;

        // Aallonvaihe
        float phase = (center_x / scroller_wavelength) * M_TAU
                    + time * scroller_wave_speed;

        float center_y = scroller_base_y
                       + std::sin(phase) * scroller_amplitude;

        if (glyph.mask != nullptr) {
            int x = static_cast<int>(cursor_x);
            int y = static_cast<int>(center_y - glyph.mask->h * 0.5f);

            // Klippaus: näkyvä alue
            if (x + gw >= 0 && x < target->w) {
                // Rainbow-pinta (luodaan ja vapautetaan kutsukohtaisesti)
                SDL_Surface* rainbow = create_rainbow_text_surface(
                    glyph.mask, ribbon_x);

                // 1) Musta reunus (outline)
                draw_outline(target, glyph.outline, x, y,
                             scroller_outline_thickness);

                // 2) Varjo (+1, +2 offset)
                SDL_Rect shad_rect{x + 1, y + 2, glyph.shadow->w, glyph.shadow->h};
                SDL_BlitSurface(glyph.shadow, nullptr, target, &shad_rect);

                // 3) Värillinen glyfi
                SDL_Rect rb_rect{x, y, rainbow->w, rainbow->h};
                SDL_BlitSurface(rainbow, nullptr, target, &rb_rect);

                SDL_FreeSurface(rainbow);
            }
        }

        float advance = gw + scroller_letter_spacing;
        cursor_x += advance;
        ribbon_x += advance;
    }
}

// ═══════════════════════════════════════════════════════
//  draw_outline – pyöreä maski, blitta offseteilla
// ═══════════════════════════════════════════════════════
void PlasmaEffect::draw_outline(SDL_Surface* target, SDL_Surface* outline,
                                int x, int y, int thickness) const {
    for (int oy = -thickness; oy <= thickness; ++oy) {
        for (int ox = -thickness; ox <= thickness; ++ox) {
            if (ox == 0 && oy == 0) continue;

            // Pyöreä maski: r² ≤ threshold + 0.5
            if (ox * ox + oy * oy > thickness * thickness + 1) continue;

            int dx = x + ox;
            int dy = y + oy;

            // Täysin ruudun ulkopuolella?
            if (dx >= target->w || dy >= target->h) continue;
            if (dx + outline->w <= 0 || dy + outline->h <= 0) continue;

            SDL_Rect dst{dx, dy, outline->w, outline->h};
            SDL_BlitSurface(outline, nullptr, target, &dst);
        }
    }
}

// ═══════════════════════════════════════════════════════
//  create_glyph – yksi merkki → mask + outline + shadow
// ═══════════════════════════════════════════════════════
Glyph PlasmaEffect::create_glyph(char character) const {
    Glyph g{};

    if (character == ' ') {
        int w, h;
        TTF_SizeText(font, " ", &w, &h);
        g.width = std::max(1, w);
        return g;
    }

    // ✅ Oikea null-loppuinen C-string:
    char str[2] = {character, '\0'};

    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* mask = TTF_RenderText_Blended(font, str, white);
    if (!mask || mask->w <= 0 || mask->h <= 0) {
        if (mask) SDL_FreeSurface(mask);
        g.width = 4;
        return g;
    }
    int w = mask->w, h = mask->h;
    g.mask  = mask;
    g.width = w;

    // Outline- ja shadow-pinnat (musta, vain alfa vaihtelee)
    g.outline = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    g.shadow  = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

    SDL_LockSurface(mask);
    SDL_LockSurface(g.outline);
    SDL_LockSurface(g.shadow);

    const uint8_t* mpx = static_cast<const uint8_t*>(mask->pixels);
    uint8_t* opx  = static_cast<uint8_t*>(g.outline->pixels);
    uint8_t* spx  = static_cast<uint8_t*>(g.shadow->pixels);

    // SDL_ttf may pad each row: use each surface's pitch, not width * 4.
    for (int y = 0; y < h; ++y) {
        const auto* mask_row = reinterpret_cast<const Uint32*>(mpx + y * mask->pitch);
        auto* outline_row = reinterpret_cast<Uint32*>(opx + y * g.outline->pitch);
        auto* shadow_row = reinterpret_cast<Uint32*>(spx + y * g.shadow->pitch);
        for (int x = 0; x < w; ++x) {
            Uint8 mr, mg, mb, alpha;
            SDL_GetRGBA(mask_row[x], mask->format, &mr, &mg, &mb, &alpha);
            outline_row[x] = SDL_MapRGBA(
                g.outline->format, 0, 0, 0,
                static_cast<Uint8>(alpha * scroller_outline_alpha / 255));
            shadow_row[x] = SDL_MapRGBA(
                g.shadow->format, 0, 0, 0, static_cast<Uint8>(alpha * 180 / 255));
        }
    }

    SDL_UnlockSurface(mask);
    SDL_UnlockSurface(g.outline);
    SDL_UnlockSurface(g.shadow);

    return g;
}

// ═══════════════════════════════════════════════════════
//  build_scroller_glyphs
// ═══════════════════════════════════════════════════════
void PlasmaEffect::build_scroller_glyphs() {
    scroller_glyphs.clear();
    scroller_width = 0;

    for (char ch : scroller_text) {
        Glyph g = create_glyph(ch);
        scroller_glyphs.push_back(std::move(g));
        scroller_width += scroller_glyphs.back().width + scroller_letter_spacing;
    }
}

// ═══════════════════════════════════════════════════════
//  create_rainbow_text_surface – sateenkaari per sarakke
// ═══════════════════════════════════════════════════════
SDL_Surface* PlasmaEffect::create_rainbow_text_surface(
    SDL_Surface* mask, float ribbon_x) const
{
    int w = mask->w, h = mask->h;

    SDL_Surface* rainbow = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

    SDL_LockSurface(mask);
    SDL_LockSurface(rainbow);

    const uint8_t* mpx = static_cast<const uint8_t*>(mask->pixels);
    uint8_t* rpx = static_cast<uint8_t*>(rainbow->pixels);

    for (int lx = 0; lx < w; ++lx) {
        float gx = ribbon_x + lx;

        // Hue: positiosta + ajasta, kvantisoitu
        float hue_f = std::fmod(gx / scroller_rainbow_wavelength
                                + time * scroller_rainbow_speed, 1.0f);
        if (hue_f < 0.0f) hue_f += 1.0f;

        int levels = scroller_rainbow_levels;
        float hue_q = std::floor(hue_f * levels) / static_cast<float>(levels);

        // Helmeilevä kirkkaus
        float brightness = 0.78f + 0.22f * std::sin(
            gx / 18.0f - time * 3.0f);
        brightness = std::max(0.0f, std::min(1.0f, brightness));

        // HSV → RGB (S=1)
        float r = 0, g = 0, b = 0;
        {
            float hp = hue_q * 6.0f;
            float x_ = brightness * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
            if      (hp < 1) { r=brightness; g=x_; b=0; }
            else if (hp < 2) { r=x_; g=brightness; b=0; }
            else if (hp < 3) { r=0; g=brightness; b=x_; }
            else if (hp < 4) { r=0; g=x_; b=brightness; }
            else if (hp < 5) { r=x_; g=0; b=brightness; }
            else             { r=brightness; g=0; b=x_; }
        }

        uint8_t cr = static_cast<uint8_t>(r * 255.0f);
        uint8_t cg = static_cast<uint8_t>(g * 255.0f);
        uint8_t cb = static_cast<uint8_t>(b * 255.0f);

        // Täytä sarakke: RGB = rainbow, A = mask_alpha
        for (int y = 0; y < h; ++y) {
            const auto* mask_row = reinterpret_cast<const Uint32*>(mpx + y * mask->pitch);
            auto* rainbow_row = reinterpret_cast<Uint32*>(rpx + y * rainbow->pitch);
            Uint8 mr, mg, mb, alpha;
            SDL_GetRGBA(mask_row[lx], mask->format, &mr, &mg, &mb, &alpha);
            rainbow_row[lx] = SDL_MapRGBA(rainbow->format, cr, cg, cb, alpha);
        }
    }

    SDL_UnlockSurface(mask);
    SDL_UnlockSurface(rainbow);

    return rainbow;
}

// ═══════════════════════════════════════════════════════
//  PALETTE-HELPERIT
// ═══════════════════════════════════════════════════════
Palette PlasmaEffect::create_duotone_palette(Color c1, Color c2) const {
    Palette p{};
    for (int i = 0; i < 256; ++i) {
        float phase = static_cast<float>(i) / 255.0f;
        // Cosine ping-pong: A → B → A
        float mix = 0.5f - 0.5f * std::cos(phase * M_TAU);

        p[i] = {
            static_cast<uint8_t>(c1.r * (1.0f - mix) + c2.r * mix),
            static_cast<uint8_t>(c1.g * (1.0f - mix) + c2.g * mix),
            static_cast<uint8_t>(c1.b * (1.0f - mix) + c2.b * mix)
        };
    }
    return p;
}

Palette PlasmaEffect::create_grayscale_palette(int levels) const {
    Palette p{};
    for (int i = 0; i < 256; ++i) {
        int lv = (i * (levels - 1)) / 255;
        int v  = (lv * 255) / std::max(1, levels - 1);
        p[i] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v), static_cast<uint8_t>(v)};
    }
    return p;
}

Palette PlasmaEffect::create_rainbow_palette(int levels) const {
    Palette p{};
    for (int i = 0; i < 256; ++i) {
        int li = i * levels / 256;
        float hue = static_cast<float>(li % levels) / levels;
        float hp  = hue * 6.0f;
        float r=0, g=0, b=0;
        float x_ = 1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f);
        if      (hp < 1) { r=1; g=x_; b=0; }
        else if (hp < 2) { r=x_; g=1; b=0; }
        else if (hp < 3) { r=0; g=1; b=x_; }
        else if (hp < 4) { r=0; g=x_; b=1; }
        else if (hp < 5) { r=x_; g=0; b=1; }
        else             { r=1; g=0; b=x_; }
        p[i] = {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
    }
    return p;
}
