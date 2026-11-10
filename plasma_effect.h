// plasma_effect.h  (täydellinen)
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdint>
#include <vector>
#include <string>
#include <array>

struct Color { uint8_t r, g, b; };
using Palette = std::array<Color, 256>;

struct Glyph {
    SDL_Surface* mask    = nullptr;
    SDL_Surface* outline = nullptr;
    SDL_Surface* shadow  = nullptr;
    int width = 0;
};

class PlasmaEffect {
public:
    int width, height;
    SDL_Surface* surface;   // 320×200 ARGB8888 – "sisäpinta"

    float time, speed, palette_speed, intensity;

    std::vector<float> x_coords, y_coords, distance;

    float palette_hold_time, palette_transition_time;
    std::vector<Palette> palettes;
    Palette current_palette;

    std::string scroller_text;
    TTF_Font* font = nullptr;

    float scroller_base_y, scroller_amplitude, scroller_wavelength;
    float scroller_wave_speed, scroller_speed;
    int   scroller_letter_spacing, scroller_gap;
    float scroller_rainbow_speed, scroller_rainbow_density;
    float scroller_rainbow_brightness, scroller_rainbow_saturation;
    float scroller_rainbow_wavelength;
    int   scroller_rainbow_levels;
    int   scroller_outline_thickness;
    uint8_t scroller_outline_alpha;

    std::vector<Glyph> scroller_glyphs;
    int scroller_width = 0;

    PlasmaEffect(int width = 320, int height = 200);
    ~PlasmaEffect();

    // --- Elinkaari ---
    void start();
    void update(float dt);
    void render(SDL_Renderer* renderer);
    void stop() {}

    // --- Paletti ---
    Palette get_current_palette() const;

    // --- Scroller ---
    void draw_scroller(SDL_Surface* target);
    void draw_scroller_copy(SDL_Surface* target, float start_x);
    void draw_outline(SDL_Surface* target, SDL_Surface* outline,
                      int x, int y, int thickness) const;
    Glyph create_glyph(char character) const;
    SDL_Surface* create_rainbow_text_surface(SDL_Surface* mask,
                                             float ribbon_x) const;

private:
    Palette create_duotone_palette(Color c1, Color c2) const;
    Palette create_grayscale_palette(int levels = 16) const;
    Palette create_rainbow_palette(int levels = 32) const;
    void    build_scroller_glyphs();

    // Plasma → surface (sisäinen)
    void compute_plasma();
};