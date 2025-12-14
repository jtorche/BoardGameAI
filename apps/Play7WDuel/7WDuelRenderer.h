#pragma once

#include "RendererInterface.h"
#include "7WDuel/GameEngine.h"
#include "7WDuel/GameController.h"
#include <chrono>
#include <array>
#include <vector>

struct UIGameState
{
    std::array<std::vector<const sevenWD::Card*>, 2> pickedCards;
    bool viewingPlayerCity = false;
    int viewedPlayer = -1;

    void resetView()
    {
        viewingPlayerCity = false;
        viewedPlayer = -1;
    }
};

class SevenWDuelRenderer
{
public:
    struct UIPosition
    {
        float playerPanelX0 = 50.0f; // Player 1 panel X position
        float playerPanelY = 400.0f; // Player panel Y position
        float playerPanelX1 = 1270.0f; // Player 2 panel X position

        float pyramidBaseX = 900.0f; // Screen center X for pyramid (adjustable)
        float pyramidBaseY = 260.0f; // Y position of pyramid

        float militaryTrackX0 = 720.0f; // X position of military track start
        float militaryTrackY = 40.0f;  // Y position of military track

        float scienceTokensX = 920.0f; // X position for science tokens
        float scienceTokensY = 130.0f; // Y position for science tokens
        // region where a selected card is shown magnified (controlled by app)
        float magnifiedX = 1400.0f;
        float magnifiedY = 40.0f;
        float magnifiedW = 320.0f;
        float magnifiedH = 464.0f;

        // --- Wonder draft specific positions (new) ---
        // Allows independent placement/scaling of wonder draft UI without
        // affecting the pyramid/graph layout.
        float wonderDraftBaseX = 860.0f;         // center X for wonder draft row
        float wonderDraftBaseY = 500.0f;         // center Y for wonder draft row
        float wonderDraftCardScale = 3.0f;       // multiplier applied to layout.wonderW/H
        float wonderDraftSpacing = 30.0f;        // spacing between draft cards
        float wonderDraftTitleOffset = 60.0f;    // offset for title above cards
        float wonderDraftRoundOffset = 40.0f;    // offset for round text above cards
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
    // NOTE: UIState is passed as a pointer. If `ui` is nullptr the renderer
    // will only display the current game state and will not read or write any
    // interactive fields (no hover/click handling). The application can also
    // pass a pointer to the active GameController in `gameController` so the
    // renderer can display controller state (read-only). The renderer
    // will never mutate the controller through this pointer.
    struct UIState
    {
        int mouseX = 0;
        int mouseY = 0;
        bool leftClick = false;
        bool rightClick = false;

        // Hover / selection helpers (written by renderer each draw)
        int hoveredNode = -1;           // node index in the graph the mouse is over
        int hoveredPlayableIndex = -1;  // playable index (index into m_playableCards) if hovered
        // wonder index in current player's unbuilt wonders
        int hoveredWonderPlayer = -1;    // which player panel the hovered wonder belongs to
        int hoveredWonderIndex = -1;     // index inside that player's m_unbuildWonders
        int hoveredScienceToken = -1;   // index of hovered science token on the science token area
        int hoveredWonder = -1;
        int selectedNode = -1;          // node index selected by first click (requires double-click to confirm)

        // When user clicked a wonder to build we remember both owner and index.
        // This prevents ambiguous indices across both player panels.
        int selectedWonderPlayer = -1;  // player owning the selected wonder, -1 if none
        int selectedWonderIndex = -1;   // index inside that player's m_unbuildWonders, -1 if none
        int selectedWonder = -1;

        // If renderer detected a move selection it sets this and leaves it to
        // caller to execute and reset (moveRequested -> true -> caller executes move).
        bool moveRequested = false;
        sevenWD::Move requestedMove{};

        // Optional pointer to the GameController (read-only). If set the renderer
        // will display controller state (mode, win type, etc.). The renderer
        // will never mutate the controller through this pointer.
        const sevenWD::GameController* gameController = nullptr;
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

        // Increased wonder preview size by ~25%
        float wonderW = 120.0f;
        float wonderH = 70.0f;
        // Per-purpose wonder scale factors so scaling is stable outside the draft
        // - wonderPanelScale: applied to wonders shown inside the player panels
        // - wonderPreviewScale: applied to the magnified wonder preview
        float wonderPanelScale = 1.3f;    // ~30% bigger in player panel
        float wonderPreviewScale = 1.25f; // ~25% larger magnified preview

        float tokenW = 60.0f;
        float tokenH = 60.0f;

        float playerPanelW = 360.0f;
        float playerPanelH = 200.0f;
        float padding = 10.0f;

        float resourceIconW = 28.0f;
        float resourceIconH = 28.0f;

        float chainingIconW = 20.0f;
        float chainingIconH = 20.0f;

        float yellowCardIconW = 20.0f;
        float yellowCardIconH = 28.0f;

        // weak-production icons
        float weakIconW = 28.0f;
        float weakIconH = 28.0f;
        // science symbol icon sizes used in player panels (distinct from science tokens)
        float scienceSymbolW = 28.0f;
        float scienceSymbolH = 28.0f;

        float militaryTrackLength = 700.0f;
    };

private:
    const sevenWD::GameState& m_state;
    RendererInterface* m_renderer;
    Layout m_layout;
    UIPosition m_uiPos; // Instance of UIPosition for global UI adjustments
    // double-click / click-tracking (time-based)
    int m_lastClickedNode = -1;
    std::chrono::steady_clock::time_point m_lastClickTime = std::chrono::steady_clock::time_point::min();
    int m_doubleClickMs = 400; // ms within which two clicks count as a double-click

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
    void draw(UIState* ui, UIGameState* uiGameState);

private:
    void drawBackground();
    void drawPlayers(UIState* ui);
    void drawPlayerPanel(int player, float x, float y, UIState* ui);
    void drawMilitaryTrack();
    void drawScienceTokens(UIState* ui);
    void drawSelectedCard(UIState* ui);
    void drawPlayerCityButtons(UIState* ui, UIGameState* uiGameState);
    void drawPlayerCityView(UIState* ui, UIGameState* uiGameState);
    float drawPlayerCityCardGrid(const std::vector<const sevenWD::Card*>& cards, float startX, float startY, float maxWidth);
    void drawCityCardSprite(const sevenWD::Card& card, float x, float y, float w, float h);
    void drawWonderDraft(UIState* ui);
    const char* cardTypeToString(sevenWD::CardType type) const;

    // Graph layout helpers
    int findGraphRow(u32 nodeIndex) const;
    int findGraphColumn(u32 nodeIndex) const;

    // Draw the card graph (pyramid) and update UI hover/click via UIState
    void drawCardGraph(UIState* ui);

    SDL_Texture* GetCardImage(const sevenWD::Card& card);
    // New overload: back image chosen per-card (age-specific or guild-specific).
    SDL_Texture* GetCardBackImage(const sevenWD::Card& card);
    SDL_Texture* GetCardBackImageForNode(bool isGuild, u32 age);
    // Keep a generic fallback overload for existing call sites if needed.
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
    SDL_Texture* GetScienceSymbolImage(sevenWD::ScienceSymbol symbol);
};