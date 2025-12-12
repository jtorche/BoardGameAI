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

    // Layout constants
    const float margin = 10.0f;
    const float innerX = x;
    const float innerW = m_layout.playerPanelW;
    const float spacing = 8.0f;

    // compute row height from the largest icon we'll draw to avoid overlap
    const float headerTextH = 20.0f;
    const float coinH = std::min(32.0f, std::max(16.0f, m_layout.resourceIconH + 4.0f));
    const float resourceH = m_layout.resourceIconH;
    const float chainingH = m_layout.chainingIconH;
    const float wonderH = m_layout.wonderH * 0.6f;
    const float scienceH = 32.0f;

    const float baseRowH = std::max({ headerTextH, coinH, resourceH, chainingH, wonderH, scienceH }) + 8.0f;

    // start Y inside panel (top padding)
    float curY = y + margin;

    // Row 0: Player name
    m_renderer->DrawText("Player " + std::to_string(player + 1), innerX + margin, curY, Colors::White);
    curY += baseRowH + spacing;

    // Row 1: Gold + VP
    {
        const float coinSize = coinH;
        float imgY = curY + (baseRowH - coinSize) * 0.5f;
        m_renderer->DrawImage(GetCoinImage(), innerX + margin, imgY, coinSize, coinSize);
        m_renderer->DrawText(std::to_string(city.m_gold), innerX + margin + coinSize + 8.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::Yellow);

        // right-align VP inside panel with margin
        std::string vpStr = "VP: " + std::to_string(city.m_victoryPoints);
        float vpX = innerX + innerW - margin - 80.0f; // reserve width for the VP text
        m_renderer->DrawText(vpStr, vpX, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);
    }
    curY += baseRowH + spacing;

    // Row 2: Production - icons + counts (show weak production per resource category inline)
    {
        m_renderer->DrawText("Prod:", innerX + margin, curY, Colors::Cyan);

        float rx = innerX + margin + 68.0f; // start after label
        const int numResources = int(sevenWD::ResourceType::Count);
        // available width for all resources
        float availableW = innerW - (rx - innerX) - margin;
        float perCell = availableW / std::max(1, numResources);
        perCell = std::max(perCell, 44.0f); // ensure some spacing
        for (int r = 0; r < numResources; ++r)
        {
            sevenWD::ResourceType res = sevenWD::ResourceType(r);
            SDL_Texture* tex = GetResourceImage(res);

            // calculate icon width that fits perCell and looks reasonable
            float iconW = std::min(m_layout.resourceIconW, perCell - 40.0f);
            iconW = std::max(12.0f, iconW);
            float iconH = iconW;

            // check bounds and break if no space
            if (rx + iconW + 48.0f > innerX + innerW - margin) break;

            float imgY = curY + (baseRowH - iconH) * 0.5f;
            m_renderer->DrawImage(tex, rx, imgY, iconW, iconH);

            // primary production value
            int prodVal = city.m_production[r];

            // determine weak-production applicable to this resource:
            // - normal weak production applies to Wood/Clay/Stone (first)
            // - rare weak production applies to Glass/Papyrus (second)
            int weakVal = 0;
            if (res == sevenWD::ResourceType::Wood || res == sevenWD::ResourceType::Clay || res == sevenWD::ResourceType::Stone)
                weakVal = city.m_weakProduction.first;
            else if (res == sevenWD::ResourceType::Glass || res == sevenWD::ResourceType::Papyrus)
                weakVal = city.m_weakProduction.second;

            // Compose display: primary production and optional weak suffix "(+N)" to match regular resource layout
            std::string prodText = std::to_string(prodVal);
            if (weakVal > 0)
            {
                prodText += " (+" + std::to_string(weakVal) + ")";
            }

            // value text vertically centered relative to row
            m_renderer->DrawText(prodText, rx + iconW + 6.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);

            rx += perCell;
        }
    }
    curY += baseRowH + spacing;

    // Row 3: Weak production - explicit icons + numbers (normal / rare)
    {
        m_renderer->DrawText("Weak:", innerX + margin, curY, Colors::Cyan);

        // Normal (applies to Wood / Clay / Stone)
        float wx = innerX + margin + 68.0f;
        SDL_Texture* normalTex = GetWeakNormalImage();
        float nW = m_layout.weakIconW;
        float nH = m_layout.weakIconH;
        float imgYN = curY + (baseRowH - nH) * 0.5f;
        m_renderer->DrawImage(normalTex, wx, imgYN, nW, nH);
        m_renderer->DrawText(std::to_string(city.m_weakProduction.first), wx + nW + 6.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);

        // Rare
        float rxRare = wx + nW + 36.0f;
        SDL_Texture* rareTex = GetWeakRareImage();
        float rW = m_layout.weakIconW;
        float rH = m_layout.weakIconH;
        float imgYR = curY + (baseRowH - rH) * 0.5f;
        m_renderer->DrawImage(rareTex, rxRare, imgYR, rW, rH);
        m_renderer->DrawText(std::to_string(city.m_weakProduction.second), rxRare + rW + 6.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);
    }
    curY += baseRowH + spacing;

    // Row 4: Chaining symbols - icons only
    {
        m_renderer->DrawText("Chain:", innerX + margin, curY, Colors::Cyan);
        float cx = innerX + margin + 68.0f;
        float maxX = innerX + innerW - margin;
        for (u8 s = 0; s < u8(sevenWD::ChainingSymbol::Count); ++s)
        {
            if ((city.m_chainingSymbols & (1u << s)) != 0)
            {
                if (cx + m_layout.chainingIconW > maxX) break;
                SDL_Texture* symTex = GetChainingSymbolImage(sevenWD::ChainingSymbol(s));
                float imgY = curY + (baseRowH - m_layout.chainingIconH) * 0.5f;
                m_renderer->DrawImage(symTex, cx, imgY, m_layout.chainingIconW, m_layout.chainingIconH);
                cx += m_layout.chainingIconW + 8.0f;
            }
        }
    }
    curY += baseRowH + spacing;

    // Row 5: Wonders - scaled to available width
    {
        m_renderer->DrawText("Wonders:", innerX + margin, curY, Colors::Cyan);
        float wx = innerX + margin + 88.0f;
        int unbuilt = city.m_unbuildWonderCount;
        if (unbuilt > 0)
        {
            float wondersAreaW = (innerX + innerW - margin) - wx;
            float perW = (wondersAreaW - (unbuilt - 1) * 8.0f) / float(unbuilt);
            float drawW = std::min(perW, m_layout.wonderW * 0.6f);
            drawW = std::max(24.0f, drawW);
            float drawH = drawW * (m_layout.wonderH / m_layout.wonderW);
            for (int w = 0; w < unbuilt; ++w)
            {
                if (wx + drawW > innerX + innerW - margin) break;
                sevenWD::Wonders wonder = city.m_unbuildWonders[w];
                float imgY = curY + (baseRowH - drawH) * 0.5f;
                m_renderer->DrawImage(GetWonderImage(wonder), wx, imgY, drawW, drawH);
                wx += drawW + 8.0f;
            }
        }
    }
    curY += baseRowH + spacing;

    // Row 6: Science tokens owned (icons)
    {
        m_renderer->DrawText("Science:", innerX + margin, curY, Colors::Green);
        float sx = innerX + margin + 88.0f;
        const float scienceIcon = 28.0f;
        float maxX = innerX + innerW - margin;
        for (int sc = 0; sc < int(sevenWD::ScienceSymbol::Count); ++sc)
        {
            int owned = city.m_ownedScienceSymbol[sc];
            for (int k = 0; k < owned; ++k)
            {
                if (sx + scienceIcon > maxX) break;
                float imgY = curY + (baseRowH - scienceIcon) * 0.5f;
                m_renderer->DrawImage(GetScienceTokenImage(sevenWD::ScienceToken(sc)), sx, imgY, scienceIcon, scienceIcon);
                sx += scienceIcon + 6.0f;
            }
            if (sx > maxX) break;
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

    // Move the label further above the tokens using token height for spacing
    float textY = y - (m_layout.tokenH * 0.5f) - 12.0f;
    m_renderer->DrawText("Science Tokens", x, textY, Colors::Green);

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

    // Helper: check if a card (age-local id) is already played
    auto isAgeCardPlayed = [&](u8 ageId) -> bool {
        for (u32 i = 0; i < m_state.m_numPlayedAgeCards; ++i)
            if (m_state.m_playedAgeCards[i] == ageId) return true;
        return false;
    };

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

SDL_Texture* SevenWDuelRenderer::GetResourceImage(sevenWD::ResourceType resource)
{
    return m_renderer->LoadImage("assets/resources/resource.png");
}

SDL_Texture* SevenWDuelRenderer::GetChainingSymbolImage(sevenWD::ChainingSymbol symbol)
{
    return m_renderer->LoadImage("assets/chaining/symbol.png");
}

// New weak icons
SDL_Texture* SevenWDuelRenderer::GetWeakNormalImage()
{
    return m_renderer->LoadImage("assets/resources/weak_normal.png");
}

SDL_Texture* SevenWDuelRenderer::GetWeakRareImage()
{
    return m_renderer->LoadImage("assets/resources/weak_rare.png");
}