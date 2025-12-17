#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "7WDuelRenderer.h"
#include "Slider.h"
#include "7WDuel/GameController.h"
#include "AI/AI.h"
#include "AI/ML.h"
#include "AI/MCTS.h"

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

    // ---------------
    // Prepare AI 
    // ---------------
    MCTS_Deterministic* activeAI = nullptr;
    // auto[pLoadedAI, _] = ML_Toolbox::loadAIFromFile<MCTS_Simple>(NetworkType::Net_TwoLayer8, "", false);
	// if (pLoadedAI)
    // {
    //     pLoadedAI->m_depth = 15;
    //     pLoadedAI->m_numSimu = 200;
	// 	activeAI = pLoadedAI;
    // }

    if (!activeAI) {
        activeAI = new MCTS_Deterministic(10000, 50);
    }
    
    std::cout << "Loaded AI: " << activeAI->getName() << "\n";

    // create per-thread context for the active AI (may be nullptr for simple AIs)
    void* aiThreadCtx = activeAI->createPerThreadContext();

    // -----------------------------------------------------------
    // GAME + UI RENDERER
    // -----------------------------------------------------------
    sevenWD::GameContext gameContext((unsigned int)time(0));
    sevenWD::GameController gameController(gameContext);
    // game.initializeForNewGame();
    
    RendererInterface renderer(gWindow);
    SevenWDuelRenderer ui(gameController.m_gameState, &renderer);

    // RNG for random move selection (still used for tie-breaking / other code)
    std::mt19937 rng(static_cast<uint32_t>(SDL_GetTicks()));

    bool running = true;
    Uint64 last = SDL_GetTicks();

    // UI state is owned by the application (transient clicks, mouse pos, etc.)
    SevenWDuelRenderer::UIState uiState;
    UIGameState uiGameState;
    // Provide pointer to GameController so renderer may display controller state
    uiState.gameController = &gameController;

    // Ensure initial mouse position is set
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    uiState.mouseX = static_cast<int>(mx + 0.5f);
    uiState.mouseY = static_cast<int>(my + 0.5f);

    // Toggle: when true only player 1 may interact with mouse; player 2 moves must be triggered by space (AI).
    bool onlyPlayer1Mouse = false;

    // Store last AI-chosen score for UI rendering
    float lastAIScore = 0.0f;
    bool lastAIScoreValid = false;
    std::string lastAIScoreText;

    // Helper to check if the controller is in a terminal (win) state
    auto isGameOver = [&gameController]() -> bool
    {
        using S = sevenWD::GameController::State;
        return gameController.m_state == S::WinPlayer0 || gameController.m_state == S::WinPlayer1;
    };

    // ---------------------------
    // History / backtrack support
    // ---------------------------
    struct Snapshot
    {
        sevenWD::GameState state;
        sevenWD::GameController::State controllerState;
        sevenWD::WinType winType;
        UIGameState uiGameState;
    };

    std::vector<Snapshot> history;
    size_t historyIndex = 0;

    // Save initial state
    history.push_back(Snapshot{ gameController.m_gameState, gameController.m_state, gameController.m_winType, uiGameState });
    historyIndex = 0;

    // Helper to restore a snapshot and reset transient UI
    auto restoreSnapshot = [&](size_t idx)
    {
        if (idx >= history.size())
            return;

        gameController.m_gameState = history[idx].state;
        gameController.m_state = history[idx].controllerState;
        gameController.m_winType = history[idx].winType;
        uiGameState = history[idx].uiGameState;

        // Reset transient UI state to avoid accidental double-actions
        uiState.moveRequested = false;
        uiState.leftClick = false;
        uiState.rightClick = false;
        uiState.hoveredNode = -1;
        uiState.hoveredPlayableIndex = -1;
        uiState.hoveredWonder = -1;
        uiState.selectedWonder = -1;
        uiState.requestedMove = sevenWD::Move{};
    };

    auto removeRecordedCard = [&](u32 player, u8 cardId)
    {
        if (player >= uiGameState.pickedCards.size())
            return;
        auto& entries = uiGameState.pickedCards[player];
        auto it = std::find_if(entries.begin(), entries.end(), [&](const sevenWD::Card* card)
        {
            return card && card->getId() == cardId;
        });
        if (it != entries.end())
            entries.erase(it);
    };

    auto recordMoveForUI = [&](u32 actingPlayer, const sevenWD::Move& move)
    {
        using Action = sevenWD::Move::Action;
        switch (move.action)
        {
        case Action::Pick:
        {
            const sevenWD::Card& card = gameController.m_gameState.getPlayableCard(move.playableCard);
            uiGameState.pickedCards[actingPlayer].push_back(&card);
            break;
        }
        case Action::BuildWonder:
        {
            const sevenWD::Card& wonderCard = gameController.m_gameState.getCurrentPlayerWonder(move.wonderIndex);
            uiGameState.pickedCards[actingPlayer].push_back(&wonderCard);
            sevenWD::Wonders wonder = static_cast<sevenWD::Wonders>(wonderCard.getSecondaryType());
            if (wonder == sevenWD::Wonders::Mausoleum && move.additionalId != u8(-1))
            {
                uiGameState.pickedCards[actingPlayer].push_back(&gameContext.getCard(move.additionalId));
            }
            else if ((wonder == sevenWD::Wonders::Zeus || wonder == sevenWD::Wonders::CircusMaximus) && move.additionalId != u8(-1))
            {
                removeRecordedCard((actingPlayer + 1) % 2, move.additionalId);
            }
            break;
        }
        case Action::ScienceToken:
        {
            const sevenWD::Card* tokenCard = nullptr;
            auto ctrlState = gameController.m_state;
            if (ctrlState == sevenWD::GameController::State::PickScienceToken)
            {
                tokenCard = &gameController.m_gameState.getPlayableScienceToken(move.playableCard);
            }
            else if ((ctrlState == sevenWD::GameController::State::GreatLibraryToken ||
                      ctrlState == sevenWD::GameController::State::GreatLibraryTokenThenReplay) &&
                      move.additionalId != u8(-1))
            {
                tokenCard = &gameContext.getCard(move.additionalId);
            }

            if (tokenCard)
                uiGameState.pickedCards[actingPlayer].push_back(tokenCard);
            break;
        }
        default:
            break;
        }
    };

    auto playMove = [&](const sevenWD::Move& move)
    {
        const u32 actingPlayer = gameController.m_gameState.getCurrentPlayerTurn();
        recordMoveForUI(actingPlayer, move);
        bool ended = gameController.play(move);

        if (historyIndex + 1 < history.size())
            history.resize(historyIndex + 1);

        history.push_back(Snapshot{ gameController.m_gameState, gameController.m_state, gameController.m_winType, uiGameState });
        historyIndex = history.size() - 1;

        // Invalidate AI score display when human plays (optional)
        // lastAIScoreValid = false;

        return ended;
    };

    // Create two sliders with desired ranges and default values.
    std::vector<Slider*> allSliders;
    Slider sliderAINumSamples(10, 200, 30, "AI Samples");
    Slider sliderNumSimu(1000, 200000, 10000, "AI Num Simu");
	allSliders.push_back(&sliderAINumSamples);
	allSliders.push_back(&sliderNumSimu);

    // Place sliders somewhere in UI (top-right under toggle). We'll set positions every frame before drawing.
    const int bx = 1600;
    const int by = 0;
    const int bw = 320;
    const int bh = 36;

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

            // Key down: AI move (space) OR history navigation (left/right)
            if (e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_SPACE)
                {
                    // When "Only Player1 Mouse" is enabled, allow the space-bar AI action
                    // only when it's player 2's turn. Otherwise space triggers AI for any player.
                    if (onlyPlayer1Mouse && gameController.m_gameState.getCurrentPlayerTurn() != 1)
                    {
                        std::cout << "Space (AI) is disabled for player 1 when Only Player1 Mouse is ON. Press space when it's player 2's turn.\n";
                    }
                    else
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
                                activeAI->m_numSampling = sliderAINumSamples.value;
								activeAI->m_numMoves = sliderNumSimu.value;

                                // Use the active AI to pick the move
                                float score;
                                sevenWD::Move chosen;
                                std::tie(chosen, score) = activeAI->selectMove(gameContext, gameController, moves, aiThreadCtx);

                                // Keep last score for UI rendering
                                lastAIScore = score;
                                lastAIScoreValid = true;
                                {
                                    std::ostringstream oss;
                                    oss << "AI score: " << std::fixed << std::setprecision(3) << score;
                                    lastAIScoreText = oss.str();
                                }

                                std::cout << "AI (" << activeAI->getName() << ") playing move (score=" << std::fixed << std::setprecision(3) << score << "): ";
                                gameController.printMove(std::cout, chosen) << "\n";

                                // Execute move. play(...) returns true if the game ended as a result.
                                bool ended = playMove(chosen);
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
                else if (e.key.key == SDLK_LEFT)
                {
                    // Rewind one state
                    if (historyIndex > 0)
                    {
                        historyIndex--;
                        restoreSnapshot(historyIndex);
                    }
                }
                else if (e.key.key == SDLK_RIGHT)
                {
                    // Restore next state (if available)
                    if (historyIndex + 1 < history.size())
                    {
                        historyIndex++;
                        restoreSnapshot(historyIndex);
                    }
                }
            }

            // Mouse motion: update coordinates (SDL3 provides floats)
            if (e.type == SDL_EVENT_MOUSE_MOTION)
            {
                uiState.mouseX = static_cast<int>(e.motion.x + 0.5f);
                uiState.mouseY = static_cast<int>(e.motion.y + 0.5f);

                // dragging support: update sliders if they are dragging
                for (Slider* s : allSliders) s->onMouseMove(uiState.mouseX, uiState.mouseY);
            }

            // Button down: set transient click flags and update mouse coords
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                uiState.mouseX = static_cast<int>(e.button.x + 0.5f);
                uiState.mouseY = static_cast<int>(e.button.y + 0.5f);

                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    uiState.leftClick = true;

                    // slider interaction: check clicks on sliders (use current positions)
                    for (Slider* s : allSliders) s->onMouseDown(uiState.mouseX, uiState.mouseY);
                }
                else if (e.button.button == SDL_BUTTON_RIGHT)
                    uiState.rightClick = true;
            }

            // Button up: clear dragging state for sliders
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                // update mouse coords on up
                uiState.mouseX = static_cast<int>(e.button.x + 0.5f);
                uiState.mouseY = static_cast<int>(e.button.y + 0.5f);

                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    // release sliders
                    for (Slider* s : allSliders) s->onMouseUp();
                }
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
        ui.draw(&uiState, &uiGameState);

        // --------------------------
        // Draw toggle button + handle click
        // --------------------------
        bool btnHovered = uiState.mouseX >= bx && uiState.mouseX <= bx + bw &&
                          uiState.mouseY >= by && uiState.mouseY <= by + bh;

        // Draw button background using RendererInterface (avoid direct SDL calls)
        SDL_Color bgColor = onlyPlayer1Mouse ? SDL_Color{ 24, 128, 24, 220 } : SDL_Color{ 48, 48, 48, 200 };
        // Fill rect by drawing horizontal lines via RendererInterface::DrawLine
        for (int yy = by; yy < by + bh; ++yy)
        {
            renderer.DrawLine(float(bx), float(yy), float(bx + bw), float(yy), bgColor);
        }

        // Draw border using RendererInterface
        SDL_Color borderColor = btnHovered ? SDL_Color{ 255, 215, 0, 255 } : SDL_Color{ 200, 200, 200, 255 };
        renderer.DrawRect(float(bx), float(by), float(bw), float(bh), borderColor);

        // label
        std::string label = std::string("Only Player1 Mouse: ") + (onlyPlayer1Mouse ? "ON" : "OFF");
        renderer.DrawText(label, float(bx + 10), float(by + 8), SevenWDuelRenderer::Colors::White);

        // Draw last AI score (if available) below the toggle button
        if (lastAIScoreValid) {
            renderer.DrawText(lastAIScoreText, float(bx + 10), float(by + bh + 8), SevenWDuelRenderer::Colors::White);
        }

        // -----------------------------------------------------------
        // Draw sliders (position them relative to toggle area)
        // -----------------------------------------------------------
        // slider visual area start
        float sliderBaseX = float(20);
        float sliderBaseY = float(by + bh + 100.0f);
        float sliderW = float(bw - 24);
        const float sliderSpacing = 40.0f;
        const float sliderH = 20.0f;

        // layout and draw all sliders from the vector
        for (size_t i = 0; i < allSliders.size(); ++i)
        {
            Slider* s = allSliders[i];
            s->x = sliderBaseX;
            s->y = sliderBaseY + float(i) * sliderSpacing;
            s->w = sliderW;
            s->h = sliderH;
            s->draw(renderer, uiState.mouseX, uiState.mouseY);
        }

        // When game is over draw both player final scores using computeVictoryPoint()
        if (isGameOver()) {
            const auto& gs = gameController.m_gameState;
            u32 vp0 = gs.m_playerCity[0].computeVictoryPoint(gs.m_playerCity[1]);
            u32 vp1 = gs.m_playerCity[1].computeVictoryPoint(gs.m_playerCity[0]);

            std::ostringstream oss;
            oss << "Final scores - Player 1: " << vp0 << "   Player 2: " << vp1;

            // Draw near top-left (adjust coordinates as desired)
            renderer.DrawText(oss.str(), 20.0f, 200.0f, SevenWDuelRenderer::Colors::White);

            // Draw winner text below
            std::string winner;
            if (vp0 == vp1) {
                // tie-break by blue cards per rules in GameState::findWinner; show draw
                winner = "Result: Draw (tie-break by blue cards)";
            } else {
                winner = std::string("Winner: Player ") + (vp0 > vp1 ? "1" : "2");
            }
            renderer.DrawText(winner, 20.0f, 150.0f, SevenWDuelRenderer::Colors::White);
        }

        // Handle toggle click — consume the click so it won't trigger game moves
        if (uiState.leftClick && btnHovered)
        {
            onlyPlayer1Mouse = !onlyPlayer1Mouse;
            std::cout << "Only Player1 Mouse set to " << (onlyPlayer1Mouse ? "ON" : "OFF") << "\n";

            // Consume the click so renderer/game doesn't also act on it
            uiState.moveRequested = false;
            uiState.leftClick = false;
            uiState.rightClick = false;
        }

        // If renderer requested a move, validate it against legal moves before executing.
        if (uiState.moveRequested)
        {
            // If option enabled and current player is player 2, ignore mouse moves and inform.
            if (onlyPlayer1Mouse && gameController.m_gameState.getCurrentPlayerTurn() == 1)
            {
                std::cout << "Mouse moves are disabled for player 2 (only player 1 may use the mouse). Ignoring requested move.\n";
                uiState.moveRequested = false;
                uiState.leftClick = false;
                uiState.rightClick = false;
            }
            else
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

                    // Match moves by action/playableCard/wonderIndex.
                    // If UI didn't provide additionalId (== u8(-1)) allow matching a legal move
                    // and use that legal move (it may carry the required additionalId).
                    sevenWD::Move chosenLegalMove{};
                    bool valid = false;

                    for (const auto& m : legalMoves)
                    {
                        if (m.action != uiState.requestedMove.action) continue;
                        if (m.playableCard != uiState.requestedMove.playableCard) continue;
                        if (m.wonderIndex != uiState.requestedMove.wonderIndex) continue;

                        // If additionalId matches exactly, accept immediately.
                        if (m.additionalId == uiState.requestedMove.additionalId)
                        {
                            chosenLegalMove = m;
                            valid = true;
                            break;
                        }

                        // If UI didn't provide additionalId (common for wonder effects),
                        // accept the first matching legal move and use its additionalId.
                        if (uiState.requestedMove.additionalId == u8(-1))
                        {
                            chosenLegalMove = m;
                            valid = true;
                            break;
                        }
                    }

                    if (valid)
                    {
                        bool end = playMove(chosenLegalMove);
                        (void)end;

                        // After a move is played, reset transient UI selection
                        uiState.selectedWonder = -1;
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
        }

        SDL_RenderPresent(renderer.GetSDLRenderer());
    }

    // cleanup AI thread context (works for loaded AI or fallback RandAI)
    activeAI->destroyPerThreadContext(aiThreadCtx);

    SDL_DestroyWindow(gWindow);

    SDL_Quit();

    return 0;
}