#include "7WDuelRenderer.h"

#include <algorithm>
#include <vector>
#include <string>

SevenWDuelRenderer::SevenWDuelRenderer(const sevenWD::GameState& state, RendererInterface* renderer)
    : m_state(state), m_renderer(renderer)
{
}

void SevenWDuelRenderer::draw()
{
    drawBackground();
    drawPlayers();
    drawMilitaryTrack();
    drawScienceTokens();
    drawCardGraph();
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawBackground()
{
    m_renderer->DrawImage(GetBackgroundPanel(), 0, 0, 1920, 1080);
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawPlayers()
{
    drawPlayerPanel(0, m_uiPos.playerPanelX0, m_uiPos.playerPanelY);
    drawPlayerPanel(1, m_uiPos.playerPanelX1, m_uiPos.playerPanelY);
}

void SevenWDuelRenderer::drawPlayerPanel(int player, float x, float y)
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
void SevenWDuelRenderer::drawMilitaryTrack()
{
    float x0 = m_uiPos.militaryTrackX0;
    float y = m_uiPos.militaryTrackY;
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
void SevenWDuelRenderer::drawScienceTokens()
{
    float x = m_uiPos.scienceTokensX;
    float y = m_uiPos.scienceTokensY;

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

// Find the row for a given node
int SevenWDuelRenderer::findGraphRow(u32 nodeIndex) const
{
    const auto& node = m_state.m_graph[nodeIndex];

    int r0 = 0, r1 = 0;
    if (node.m_parent0 != sevenWD::GameState::CardNode::InvalidNode)
        r0 = 1 + findGraphRow(node.m_parent0);  // Recursively find the row for the first parent
    if (node.m_parent1 != sevenWD::GameState::CardNode::InvalidNode)
        r1 = 1 + findGraphRow(node.m_parent1);  // Recursively find the row for the second parent

    // Return the maximum depth of both parents
    return std::max(r0, r1);
}

// Find the column for a given node in the graph (based on row)
int SevenWDuelRenderer::findGraphColumn(u32 nodeIndex) const
{
    const int row = findGraphRow(nodeIndex);
    int col = 0;

    // Iterate through all nodes and find the correct column for the given row
    for (u32 i = 0; i < m_state.m_graph.size(); ++i)
    {
        if (findGraphRow(i) == row)
        {
            // For each card in the same row, assign a column number based on the order
            if (i == nodeIndex) return col;  // Return the column number when we find the current node
            col++;  // Increment column for every node in the same row
        }
    }

    return col;  // Return the column (it should be assigned correctly now)
}

// Draw the card graph (pyramid)
void SevenWDuelRenderer::drawCardGraph()
{
    // Use the UIPosition values for the pyramid positioning
    const float baseX = m_uiPos.pyramidBaseX; // X position for the base of the pyramid
    const float baseY = m_uiPos.pyramidBaseY; // Y position for the base of the pyramid

    const float dX = m_layout.cardW + 16.0f;  // Space between cards horizontally
    const float dY = m_layout.cardH + 20.0f;  // Space between rows vertically

    const auto& graph = m_state.m_graph;

    // Create a dictionary to store the number of cards in each row
    std::vector<int> rowCardCounts;

    // Count how many cards are in each row (by row depth)
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        const int row = findGraphRow(nodeIndex);
        if (rowCardCounts.size() <= row)
        {
            rowCardCounts.resize(row + 1, 0);
        }
        rowCardCounts[row]++;
    }

    // Loop through all nodes and draw each card in the correct position
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        const auto& node = graph[nodeIndex];

        // Find the row and column for this card
        const int row = findGraphRow(nodeIndex);
        const int col = findGraphColumn(nodeIndex);

        // Calculate total width of all cards in this row
        const float rowWidth = rowCardCounts[row] * dX;

        // Calculate the starting X position to center the row
        const float rowStartX = baseX - (rowWidth / 2.0f);

        // Calculate the X position for this specific card in the row
        const float x = rowStartX + col * dX;

        // Calculate the Y position based on the row
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

            // Draw a highlight if the card is playable
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
        // Hidden: draw card back if not visible
        // ---------------------------------------------------------------
        else
        {
            texture = GetCardBackImage();
        }

        // ---------------------------------------------------------------
        // Draw the final card image
        // ---------------------------------------------------------------
        m_renderer->DrawImage(texture, x, y, m_layout.cardW, m_layout.cardH);
    }
}

// Image loaders (existants)
SDL_Texture* SevenWDuelRenderer::GetCardImage(const sevenWD::Card& card)
{
    return m_renderer->LoadImage(
        // "assets/cards/" + std::to_string(card.getId()) + ".png"
        "assets/cards/card.png"
    );
}

SDL_Texture* SevenWDuelRenderer::GetCardBackImage()
{
    return m_renderer->LoadImage("assets/card_back.png");
}

SDL_Texture* SevenWDuelRenderer::GetWonderImage(sevenWD::Wonders wonder)
{
    return m_renderer->LoadImage(
        // "assets/wonders/" + std::to_string(int(wonder)) + ".png"
        "assets/wonders/wonder.png"
    );
}

SDL_Texture* SevenWDuelRenderer::GetScienceTokenImage(sevenWD::ScienceToken token)
{
    return m_renderer->LoadImage(
        // "assets/tokens/" + std::to_string(int(token)) + ".png"
        "assets/tokens/token.png"
    );
}

SDL_Texture* SevenWDuelRenderer::GetCoinImage()
{
    return m_renderer->LoadImage("assets/ui/coin.png");
}

SDL_Texture* SevenWDuelRenderer::GetMilitaryMarkerImage()
{
    return m_renderer->LoadImage("assets/ui/military.png");
}

SDL_Texture* SevenWDuelRenderer::GetBackgroundPanel()
{
    return m_renderer->LoadImage("assets/ui/panel.png");
}