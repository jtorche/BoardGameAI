#pragma once

#include "RendererInterface.h"
#include "7WDuel/GameEngine.h"

class SevenWDuelRenderer
{
    // Global positions of major UI elements
    struct UIPosition
    {
        float playerPanelX0 = 50.0f; // Player 1 panel X position
        float playerPanelY = 1020.0f; // Player panel Y position
        float playerPanelX1 = 1270.0f; // Player 2 panel X position

        float pyramidBaseX = 960.0f; // Screen center X for pyramid (adjustable)
        float pyramidBaseY = 260.0f; // Y position of pyramid

        float militaryTrackX0 = 200.0f; // X position of military track start
        float militaryTrackY = 40.0f;  // Y position of military track

        float scienceTokensX = 650.0f; // X position for science tokens
        float scienceTokensY = 100.0f; // Y position for science tokens
    };

    SDL_Texture* GetCardImage(const sevenWD::Card& card);
    SDL_Texture* GetCardBackImage();
    SDL_Texture* GetWonderImage(sevenWD::Wonders wonder);
    SDL_Texture* GetScienceTokenImage(sevenWD::ScienceToken token);
    SDL_Texture* GetCoinImage();
    SDL_Texture* GetMilitaryMarkerImage();
    SDL_Texture* GetBackgroundPanel();

public:
    // ============================
    //  COLOR CONSTANTS
    // ============================
    struct Colors
    {
        static constexpr SDL_Color White{ 255, 255, 255, 255 };
        static constexpr SDL_Color Yellow{ 255, 255,   0, 255 };
        static constexpr SDL_Color Cyan{ 0, 255, 255, 255 };
        static constexpr SDL_Color Green{ 0, 255,   0, 255 };
        static constexpr SDL_Color Red{ 255,   0,   0, 255 };
    };

    struct Layout
    {
        float cardW = 90;
        float cardH = 130;

        float wonderW = 120;
        float wonderH = 70;

        float tokenW = 60;
        float tokenH = 60;

        float playerPanelW = 400;
        float playerPanelH = 160;
        float padding = 8;

        float militaryTrackLength = 700;
    };

private:
    const sevenWD::GameState& m_state;
    RendererInterface* m_renderer;
    Layout m_layout;
    UIPosition m_uiPos; // Instance of UIPosition for global UI adjustments

public:
    SevenWDuelRenderer(const sevenWD::GameState& state, RendererInterface* renderer);

    // =============================================================
    //                          DRAW ENTRY
    // =============================================================
    void draw();

private:
    void drawBackground();
    void drawPlayers();
    void drawPlayerPanel(int player, float x, float y);
    void drawMilitaryTrack();
    void drawScienceTokens();

    // Graph layout helpers
    int findGraphRow(u32 nodeIndex) const;
    int findGraphColumn(u32 nodeIndex) const;

    // Draw the card graph (pyramid)
    void drawCardGraph();
};
