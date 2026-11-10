// main.cpp
#include <SDL.h>
#include <SDL_mixer.h>
#include "plasma_effect.h"
#include <algorithm>
#include <cmath>

int main(int argc, char* argv[]) {
    // ── Init ─────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512);

    Mix_Music* music = Mix_LoadMUS("./doomscroller.ogg");
    if (music) {
        Mix_PlayMusic(music, -1);   // loop forever
    } else {
        SDL_Log("Musiikkia ei löytynyt: %s", Mix_GetError());
    }

    // ── Window + Renderer ────────────────────────────────
    SDL_Window* window = SDL_CreateWindow(
        "Retro Plasma",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        Mix_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        SDL_Log("CreateRenderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        Mix_Quit(); SDL_Quit();
        return 1;
    }

    // ── PlasmaEffect (320×200 sisäinen resoluutio) ──────
    PlasmaEffect plasma(320, 200);
    plasma.start();

    // ── Pääsilukka ───────────────────────────────────────
    bool running = true;
    Uint64 last_frame = SDL_GetPerformanceCounter();

    while (running) {
        // dt sekunteina, clamped (ikkunan raahaus tms.)
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - last_frame) / static_cast<float>(SDL_GetPerformanceFrequency());
        last_frame = now;
        dt = std::min(dt, 0.1f);

        // ── Eventit ──────────────────────────────────────
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;

            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_UP:
                        plasma.speed += 0.1f;
                        break;
                    case SDLK_DOWN:
                        plasma.speed = std::max(0.1f, plasma.speed - 0.1f);
                        break;
                    case SDLK_RIGHT:
                        plasma.palette_speed += 5.0f;
                        break;
                    case SDLK_LEFT:
                        plasma.palette_speed -= 5.0f;
                        break;
                }
            }
        }

        // ── Update + Render ──────────────────────────────
        plasma.update(dt);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        plasma.render(renderer);
        SDL_RenderPresent(renderer);
    }

    // ── Cleanup ──────────────────────────────────────────
    Mix_HaltMusic();
    if (music) Mix_FreeMusic(music);
    Mix_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}