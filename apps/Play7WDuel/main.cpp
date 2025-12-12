#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>

#include "7WDuelRenderer.h"
#include "7WDuel/GameController.h"

SDL_Window* gWindow = nullptr;

int main(int argc, char** argv)
{
    // -----------------------------------------------------------
    // SDL3 INIT
    // -----------------------------------------------------------
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // -----------------------------------------------------------
    // WINDOW + RENDERER (SDL3 syntax)
    // -----------------------------------------------------------
    gWindow = SDL_CreateWindow("7 Wonders Duel",
        1920, 1080,
        SDL_WINDOW_RESIZABLE);

    if (!gWindow)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // -----------------------------------------------------------
    // GAME + UI RENDERER
    // -----------------------------------------------------------
    sevenWD::GameContext gameContext;
    sevenWD::GameController gameController(gameContext);
    // game.initializeForNewGame();
    
    RendererInterface renderer(gWindow);
    SevenWDuelRenderer ui(gameController.m_gameState, &renderer);

    // RNG for random move selection
    std::mt19937 rng(static_cast<uint32_t>(SDL_GetTicks()));

    bool running = true;
    Uint64 last = SDL_GetTicks();

    // -----------------------------------------------------------
    // MAIN LOOP
    // -----------------------------------------------------------
    while (running)
    {
        // ---- delta time ----
        Uint64 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        // ---- events ----
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                running = false;

            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                running = false;

            // Left arrow: play a random legal move
            if (e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_LEFT)
                {
                    std::vector<sevenWD::Move> moves;
                    gameController.enumerateMoves(moves);
                    if (!moves.empty())
                    {
                        std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
                        const sevenWD::Move& chosen = moves[dist(rng)];
                        if (gameController.play(chosen))
                        {
                            std::cout << "Played random move: ";
                            gameController.printMove(std::cout, chosen) << "\n";
                        }
                        else
                        {
                            std::cout << "Attempt to play random move failed\n";
                        }
                    }
                    else
                    {
                        std::cout << "No legal moves to play\n";
                    }
                }
            }
        }

        // ---- update ----
        // game.update(dt); (if you want)

        // ---- render ----
        SDL_SetRenderDrawColor(renderer.GetSDLRenderer(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.GetSDLRenderer());

        ui.draw();

        SDL_RenderPresent(renderer.GetSDLRenderer());
    }

    SDL_DestroyWindow(gWindow);

    SDL_Quit();

    return 0;
}