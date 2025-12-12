#include "RendererInterface.h"
#include "7WDuel/GameEngine.h"
class SevenWDuelRenderer
{
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

        float militaryTrackY = 40;
        float militaryTrackX0 = 200;
        float militaryTrackLength = 700;
    };

private:
    const sevenWD::GameState& m_state;
    RendererInterface* m_renderer;
    Layout m_layout;

public:
    SevenWDuelRenderer(const sevenWD::GameState& state, RendererInterface* renderer)
        : m_state(state), m_renderer(renderer)
    {}


    // =========================
    //    ASSET LOAD HELPERS 
    // =========================

    SDL_Texture* GetCardImage(const sevenWD::Card& card)
    {
        return m_renderer->LoadImage(
            "assets/cards/" + std::to_string(card.getId()) + ".png"
        );
    }

    SDL_Texture* GetCardBackImage()
    {
        return m_renderer->LoadImage("assets/card_back.png");
    }

    SDL_Texture* GetWonderImage(sevenWD::Wonders wonder)
    {
        return m_renderer->LoadImage(
            "assets/wonders/" + std::to_string(int(wonder)) + ".png"
        );
    }

    SDL_Texture* GetScienceTokenImage(sevenWD::ScienceToken token)
    {
        return m_renderer->LoadImage(
            "assets/tokens/" + std::to_string(int(token)) + ".png"
        );
    }

    SDL_Texture* GetCoinImage()
    {
        return m_renderer->LoadImage("assets/ui/coin.png");
    }

    SDL_Texture* GetMilitaryMarkerImage()
    {
        return m_renderer->LoadImage("assets/ui/military.png");
    }

    SDL_Texture* GetBackgroundPanel()
    {
        return m_renderer->LoadImage("assets/ui/panel.png");
    }

    // =============================================================
    //                          DRAW ENTRY
    // =============================================================

    void draw()
    {
        drawBackground();
        drawPlayers();
        drawMilitaryTrack();
        drawScienceTokens();
        drawCardGraph();
    }

private:

    // ---------------------------------------------------------------------
    void drawBackground()
    {
        m_renderer->DrawImage(GetBackgroundPanel(), 0, 0, 1920, 1080);
    }

    // ---------------------------------------------------------------------
    void drawPlayers()
    {
        drawPlayerPanel(0, 50, 820);
        drawPlayerPanel(1, 1470, 820);
    }

    void drawPlayerPanel(int player, float x, float y)
    {
        const sevenWD::PlayerCity& city = m_state.getPlayerCity(player);

        m_renderer->DrawImage(
            GetBackgroundPanel(),
            x - m_layout.padding,
            y - m_layout.padding,
            m_layout.playerPanelW + 2 * m_layout.padding,
            m_layout.playerPanelH + 2 * m_layout.padding
        );

        m_renderer->DrawText("Player " + std::to_string(player + 1),
            x, y, Colors::White);

        float rowY = y + 30;

        // GOLD
        m_renderer->DrawImage(GetCoinImage(), x, rowY, 32, 32);
        m_renderer->DrawText(std::to_string(city.m_gold),
            x + 40, rowY + 8, Colors::Yellow);

        // VP
        m_renderer->DrawText("VP: " + std::to_string(city.m_victoryPoints),
            x + 120, rowY + 8, Colors::White);

        // Wonders
        rowY += 50;
        m_renderer->DrawText("Wonders:", x, rowY, Colors::Cyan);

        float wx = x + 100;

        for (int w = 0; w < city.m_unbuildWonderCount; ++w)
        {
            sevenWD::Wonders wonder = city.m_unbuildWonders[w];

            m_renderer->DrawImage(
                GetWonderImage(wonder),
                wx, rowY - 20,
                m_layout.wonderW,
                m_layout.wonderH
            );

            wx += m_layout.wonderW + 6;
        }

        // Science Symbols
        rowY += 85;
        m_renderer->DrawText("Science:", x, rowY, Colors::Green);

        float sx = x + 100;

        for (int sc = 0; sc < int(sevenWD::ScienceSymbol::Count); ++sc)
        {
            int owned = city.m_ownedScienceSymbol[sc];
            for (int k = 0; k < owned; ++k)
            {
                m_renderer->DrawImage(
                    GetScienceTokenImage(sevenWD::ScienceToken(sc)),
                    sx, rowY - 20,
                    40, 40
                );
                sx += 44;
            }
        }
    }


    // ---------------------------------------------------------------------
    void drawMilitaryTrack()
    {
        float x0 = m_layout.militaryTrackX0;
        float y = m_layout.militaryTrackY;
        float x1 = x0 + m_layout.militaryTrackLength;

        m_renderer->DrawText("MILITARY", x0 - 120, y, Colors::Red);
        m_renderer->DrawLine(x0, y + 15, x1, y + 15, Colors::Red);

        int pos = m_state.getMilitary();   // -9 → +9
        float t = (pos + 9) / 18.0f;

        float markerX = x0 + t * m_layout.militaryTrackLength;

        m_renderer->DrawImage(
            GetMilitaryMarkerImage(),
            markerX - 15, y,
            30, 30
        );
    }


    // ---------------------------------------------------------------------
    void drawScienceTokens()
    {
        float x = 650;
        float y = 100;

        m_renderer->DrawText("Science Tokens", x, y - 20, Colors::Green);

        for (int i = 0; i < m_state.m_numScienceToken; ++i)
        {
            const sevenWD::Card& card = m_state.getPlayableScienceToken(i);
            sevenWD::ScienceToken type = (sevenWD::ScienceToken)card.getSecondaryType();
        
            m_renderer->DrawImage(
                GetScienceTokenImage(type),
                x + i * (m_layout.tokenW + 10),
                y,
                m_layout.tokenW,
                m_layout.tokenH
            );
        }
    }

    int findGraphRow(u32 nodeIndex) const
    {
        const auto& node = m_state.m_graph[nodeIndex];
        int r0 = 0, r1 = 0;

        if (node.m_parent0 != sevenWD::GameState::CardNode::InvalidNode)
            r0 = 1 + findGraphRow(node.m_parent0);

        if (node.m_parent1 != sevenWD::GameState::CardNode::InvalidNode)
            r1 = 1 + findGraphRow(node.m_parent1);

        return std::max(r0, r1);
    }

    int findGraphColumn(u32 nodeIndex) const
    {
        const int row = findGraphRow(nodeIndex);

        int col = 0;
        for (u32 i = 0; i < m_state.m_graph.size(); ++i)
        {
            if (i == nodeIndex)
                return col;

            if (findGraphRow(i) == row)
                col++;
        }
        return col; // fallback
    }

    void drawCardGraph()
    {
        const float baseX = 550.0f;
        const float baseY = 260.0f;

        const float dX = m_layout.cardW + 12.0f;
        const float dY = m_layout.cardH + 16.0f;

        const auto& graph = m_state.m_graph;

        for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
        {
            const auto& node = graph[nodeIndex];

            const int row = findGraphRow(nodeIndex);
            const int col = findGraphColumn(nodeIndex);

            const float x = baseX + col * dX;
            const float y = baseY + row * dY;

            SDL_Texture* texture = nullptr;

            // ---------------------------------------------------------------
            // Visible: draw real card + highlight if playable
            // ---------------------------------------------------------------
            if (node.m_visible)
            {
                const sevenWD::Card& card = m_state.m_context->getCard(node.m_cardId);
                texture = GetCardImage(card);

                // Check if node is playable
                bool isPlayable = false;
                for (int i = 0; i < m_state.m_numPlayableCards; ++i)
                {
                    if (m_state.m_playableCards[i] == nodeIndex)
                    {
                        isPlayable = true;
                        break;
                    }
                }

                // Draw highlight
                if (isPlayable)
                {
                    m_renderer->DrawRect(
                        x - 4.0f,
                        y - 4.0f,
                        m_layout.cardW + 8.0f,
                        m_layout.cardH + 8.0f,
                        Colors::Yellow
                    );
                }
            }
            // ---------------------------------------------------------------
            // Hidden: draw card back
            // ---------------------------------------------------------------
            else
            {
                texture = GetCardBackImage();
            }

            // ---------------------------------------------------------------
            // Final card draw
            // ---------------------------------------------------------------
            m_renderer->DrawImage(texture, x, y, m_layout.cardW, m_layout.cardH);
        }
    }
};
