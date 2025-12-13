#pragma once

#include "RendererInterface.h"
#include "7WDuel/GameEngine.h"
#include "7WDuel/GameController.h"

class SevenWDuelRenderer
{
public:
    struct UIPosition
    {
        float playerPanelX0 = 50.0f; // Player 1 panel X position
        float playerPanelY = 500.0f; // Player panel Y position
        float playerPanelX1 = 1270.0f; // Player 2 panel X position

        float pyramidBaseX = 960.0f; // Screen center X for pyramid (adjustable)
        float pyramidBaseY = 260.0f; // Y position of pyramid

        float militaryTrackX0 = 200.0f; // X position of military track start
        float militaryTrackY = 40.0f;  // Y position of military track

        float scienceTokensX = 650.0f; // X position for science tokens
        float scienceTokensY = 100.0f; // Y position for science tokens
    };

    // UI state is stored outside the renderer. The application must fill mouse
    // coordinates and click events each frame and call draw(ui).
    // Semantics:
    //  - `leftClick` / `rightClick` are transient events that are true only for
    //    the frame where the button was clicked. The caller is responsible for
    //    clearing them after passing the UIState to `draw`.
    //  - If the renderer detects a user action it sets `moveRequested` to true
    //    and fills `requestedMove`. The application should consume that move and
    //    then call GameController::play(...) or otherwise handle it.
    // NOTE: UIState is now passed as a pointer. If `ui` is nullptr the renderer
    // will only display the current game state and will not read or write any
    // interactive fields (no hover/click handling).
    struct UIState
    {
        int mouseX = 0;
        int mouseY = 0;
        bool leftClick = false;
        bool rightClick = false;

        // Hover / selection helpers (written by renderer each draw)
        int hoveredNode = -1;           // node index in the graph the mouse is over
        int hoveredPlayableIndex = -1;  // playable index (index into m_playableCards) if hovered
        int hoveredWonder = -1;         // wonder index in current player's unbuilt wonders

        // When user clicked a wonder to build, this becomes the wonder index
        // (index into PlayerCity::m_unbuildWonders). If >=0 the renderer will
        // highlight the wonder and expect the player to pick a playable card to
        // build the wonder with. Right click cancels selection.
        int selectedWonder = -1;

        // If renderer detected a move selection it sets this and leaves it to
        // caller to execute and reset (moveRequested -> true -> caller executes move).
        bool moveRequested = false;
        sevenWD::Move requestedMove {};
    };

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
        // Scaled down UI to free space for graph center
        float cardW = 72.0f;
        float cardH = 104.0f;

        float wonderW = 96.0f;
        float wonderH = 56.0f;

        float tokenW = 48.0f;
        float tokenH = 48.0f;

        float playerPanelW = 360.0f;
        float playerPanelH = 200.0f;
        float padding = 10.0f;

        float resourceIconW = 24.0f;
        float resourceIconH = 24.0f;

        float chainingIconW = 20.0f;
        float chainingIconH = 20.0f;

        float yellowCardIconW = 20.0f;
        float yellowCardIconH = 28.0f;

        // weak-production icons
        float weakIconW = 20.0f;
        float weakIconH = 20.0f;

        float militaryTrackLength = 700.0f;
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
    // NOTE: UIState is passed as a pointer so the renderer may be invoked in
    // a read-only display mode by passing nullptr. When `ui` is non-null the
    // renderer will read mouse events and write hover/selection information
    // and (if a move was chosen) populates UIState::requestedMove + sets
    // UIState::moveRequested = true.
    void draw(UIState* ui);

private:
    void drawBackground();
    void drawPlayers(UIState* ui);
    void drawPlayerPanel(int player, float x, float y, UIState* ui);
    void drawMilitaryTrack();
    void drawScienceTokens();

    // Graph layout helpers
    int findGraphRow(u32 nodeIndex) const;
    int findGraphColumn(u32 nodeIndex) const;

    // Draw the card graph (pyramid) and update UI hover/click via UIState
    void drawCardGraph(UIState* ui);

    SDL_Texture* GetCardImage(const sevenWD::Card& card);
    SDL_Texture* GetCardBackImage();
    SDL_Texture* GetWonderImage(sevenWD::Wonders wonder);
    SDL_Texture* GetScienceTokenImage(sevenWD::ScienceToken token);
    SDL_Texture* GetCoinImage();
    SDL_Texture* GetMilitaryMarkerImage();
    SDL_Texture* GetBackgroundPanel();
    SDL_Texture* GetResourceImage(sevenWD::ResourceType resource);
    SDL_Texture* GetChainingSymbolImage(sevenWD::ChainingSymbol symbol);
    SDL_Texture* GetWeakNormalImage();
    SDL_Texture* GetWeakRareImage();
};
