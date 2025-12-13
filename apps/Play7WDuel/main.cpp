#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <string>

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

    // UI state is owned by the application (transient clicks, mouse pos, etc.)
    SevenWDuelRenderer::UIState uiState;
    // Provide pointer to GameController so renderer may display controller state
    uiState.gameController = &gameController;

    // Ensure initial mouse position is set
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    uiState.mouseX = static_cast<int>(mx + 0.5f);
    uiState.mouseY = static_cast<int>(my + 0.5f);

    // Helper to check if the controller is in a terminal (win) state
    auto isGameOver = [&gameController]() -> bool
    {
        using S = sevenWD::GameController::State;
        return gameController.m_state == S::WinPlayer0 || gameController.m_state == S::WinPlayer1;
    };

    // -----------------------------------------------------------
    // MAIN LOOP
    // -----------------------------------------------------------
    while (running)
    {
        // ---- delta time ----
        Uint64 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        // Reset transient click flags at start of frame
        uiState.leftClick = false;
        uiState.rightClick = false;
        uiState.moveRequested = false; // renderer will set if user clicked a valid action

        // ---- events ----
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                running = false;

            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                running = false;

            // Left arrow: play a random legal move (only while game active)
            if (e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_LEFT)
                {
                    if (isGameOver())
                    {
                        std::cout << "Game has ended. No moves can be played.\n";
                    }
                    else
                    {
                        std::vector<sevenWD::Move> moves;
                        gameController.enumerateMoves(moves);
                        if (!moves.empty())
                        {
                            // Choose a random move from the list returned by enumerateMoves.
                            std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
                            sevenWD::Move chosen = moves[dist(rng)]; // copy is fine

                            // enumerateMoves produced this move, so it's already legal for current state.
                            std::cout << "Playing random move: ";
                            gameController.printMove(std::cout, chosen) << "\n";

                            // Execute move. play(...) returns true if the game ended as a result.
                            bool ended = gameController.play(chosen);
                            if (ended)
                                std::cout << "Game ended after this move.\n";
                        }
                        else
                        {
                            std::cout << "No legal moves to play\n";
                        }
                    }
                }
            }

            // Mouse motion: update coordinates (SDL3 provides floats)
            if (e.type == SDL_EVENT_MOUSE_MOTION)
            {
                uiState.mouseX = static_cast<int>(e.motion.x + 0.5f);
                uiState.mouseY = static_cast<int>(e.motion.y + 0.5f);
            }

            // Button down: set transient click flags and update mouse coords
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                uiState.mouseX = static_cast<int>(e.button.x + 0.5f);
                uiState.mouseY = static_cast<int>(e.button.y + 0.5f);

                if (e.button.button == SDL_BUTTON_LEFT)
                    uiState.leftClick = true;
                else if (e.button.button == SDL_BUTTON_RIGHT)
                    uiState.rightClick = true;
            }
        }

        // Keep mouse position accurate even if no events were processed this frame
        SDL_GetMouseState(&mx, &my);
        uiState.mouseX = static_cast<int>(mx + 0.5f);
        uiState.mouseY = static_cast<int>(my + 0.5f);

        // ---- update ----
        // game.update(dt); (if you want)

        // ---- render ----
        SDL_SetRenderDrawColor(renderer.GetSDLRenderer(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.GetSDLRenderer());

        // Pass UI state into renderer. Renderer will set hover/selection and may set moveRequested + requestedMove.
        ui.draw(&uiState);

        // If renderer requested a move, validate it against legal moves before executing.
        if (uiState.moveRequested)
        {
            if (isGameOver())
            {
                // If game already ended ignore any requested moves and inform.
                std::cout << "Ignoring requested move: game already ended.\n";
                uiState.moveRequested = false;
                uiState.leftClick = false;
                uiState.rightClick = false;
            }
            else
            {
                std::vector<sevenWD::Move> legalMoves;
                gameController.enumerateMoves(legalMoves);

                auto moveEqual = [](const sevenWD::Move& a, const sevenWD::Move& b) -> bool {
                    return a.action == b.action &&
                           a.playableCard == b.playableCard &&
                           a.wonderIndex == b.wonderIndex &&
                           a.additionalId == b.additionalId;
                };

                bool valid = false;
                for (const auto& m : legalMoves)
                {
                    if (moveEqual(m, uiState.requestedMove))
                    {
                        valid = true;
                        break;
                    }
                }

                if (valid)
                {
                    bool end = gameController.play(uiState.requestedMove);
                    (void)end;
                }
                else
                {
                    std::cout << "Illegal move attempted: ";
                    gameController.printMove(std::cout, uiState.requestedMove) << "\n";
                }

                // Consume the requested move so it won't be processed again
                uiState.moveRequested = false;

                // Clear transient clicks to avoid double-processing in same frame
                uiState.leftClick = false;
                uiState.rightClick = false;
            }
        }

        SDL_RenderPresent(renderer.GetSDLRenderer());
    }

    SDL_DestroyWindow(gWindow);

    SDL_Quit();

    return 0;
}