#include "7WDuelRenderer.h"

#include <algorithm>
#include <vector>
#include <string>
#include <cmath>

// keep previous constructor
SevenWDuelRenderer::SevenWDuelRenderer(const sevenWD::GameState& state, RendererInterface* renderer)
    : m_state(state), m_renderer(renderer)
{
}

// Updated entry point: consumes UIState pointer and updates it (hover, selection, requestedMove)
// If ui == nullptr the renderer runs in read-only display mode (no interactions).
void SevenWDuelRenderer::draw(UIState* ui)
{
    // If interactive UIState provided, reset transient hover fields each frame
    if (ui)
    {
        ui->hoveredNode = -1;
        ui->hoveredPlayableIndex = -1;
        ui->hoveredWonder = -1;
        ui->moveRequested = false;
    }

    drawBackground();

    // Draw current player turn in the top-left (always shown, interactive or read-only)
    {
        u32 cur = m_state.getCurrentPlayerTurn();
        std::string turnText = "Current player: " + std::to_string(int(cur) + 1);
        m_renderer->DrawText(turnText, 20.0f, 20.0f, Colors::White);
    }

    // If application provided a GameController pointer in UIState, render controller info.
    if (ui && ui->gameController)
    {
        const sevenWD::GameController* gc = ui->gameController;

        auto stateToStr = [](sevenWD::GameController::State s) -> const char*
        {
            using S = sevenWD::GameController::State;
            switch (s)
            {
            case S::Play: return "Play";
            case S::PickScienceToken: return "PickScienceToken";
            case S::GreatLibraryToken: return "GreatLibraryToken";
            case S::GreatLibraryTokenThenReplay: return "GreatLibraryTokenThenReplay";
            case S::WinPlayer0: return "WinPlayer 1";
            case S::WinPlayer1: return "WinPlayer 2";
            default: return "Unknown";
            }
        };

        auto winToStr = [](sevenWD::WinType w) -> const char*
        {
            switch (w)
            {
            case sevenWD::WinType::None: return "None";
            case sevenWD::WinType::Civil: return "Civil";
            case sevenWD::WinType::Military: return "Military";
            case sevenWD::WinType::Science: return "Science";
            default: return "Unknown";
            }
        };

        std::string ctrlText = std::string("Controller: ") + stateToStr(gc->m_state) +
                               "  WinType: " + winToStr(gc->m_winType);
        // draw below the player-turn text
        m_renderer->DrawText(ctrlText, 20.0f, 44.0f, Colors::Yellow);
    }

    drawPlayers(ui);
    drawMilitaryTrack();
    drawScienceTokens();
    drawCardGraph(ui);
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawBackground()
{
    m_renderer->DrawImage(GetBackgroundPanel(), 0, 0, 1920, 1080);
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawPlayers(UIState* ui)
{
    drawPlayerPanel(0, m_uiPos.playerPanelX0, m_uiPos.playerPanelY, ui);
    drawPlayerPanel(1, m_uiPos.playerPanelX1, m_uiPos.playerPanelY, ui);
}

void SevenWDuelRenderer::drawPlayerPanel(int player, float x, float y, UIState* ui)
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

    // Row 5: Owned Guilds - show one representative card for each owned guild-type
    {
        m_renderer->DrawText("Guilds:", innerX + margin, curY, Colors::Yellow);
        float gx = innerX + margin + 88.0f;
        float maxX = innerX + innerW - margin;
        const float guildW = std::min(m_layout.cardW * 0.8f, 48.0f);
        const float guildH = guildW * (m_layout.cardH / m_layout.cardW);

        // owned guilds are stored as bitfield in PlayerCity::m_ownedGuildCards
        // find a representative guild card texture for each set bit
        if (m_state.m_context)
        {
            const auto& allGuilds = m_state.m_context->getAllGuildCards();
            // We will iterate guild types (CardType) and draw if owned.
            for (u8 type = 0; type < u8(sevenWD::CardType::Count); ++type)
            {
                if ((city.m_ownedGuildCards & (1u << type)) == 0) continue;

                // find first guild card whose secondaryType matches this "type"
                const sevenWD::Card* rep = nullptr;
                for (const sevenWD::Card& c : allGuilds)
                {
                    if (c.getSecondaryType() == type)
                    {
                        rep = &c;
                        break;
                    }
                }

                // fallback: if no representative found, skip
                if (!rep) continue;

                if (gx + guildW > maxX) break;
                float imgY = curY + (baseRowH - guildH) * 0.5f;
                SDL_Texture* tex = GetCardImage(*rep);
                if (tex)
                    m_renderer->DrawImage(tex, gx, imgY, guildW, guildH);
                else
                    m_renderer->DrawText("G", gx, imgY, Colors::Yellow);

                gx += guildW + 8.0f;
            }
        }
    }
    curY += baseRowH + spacing;

    // Row 6: Wonders - scaled to available width
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
                SDL_Texture* tex = GetWonderImage(wonder);
                m_renderer->DrawImage(GetWonderImage(wonder), wx, imgY, drawW, drawH);

                // Hit detection: if mouse is over this wonder record that in UIState (only if ui provided)
                if (ui && ui->mouseX >= int(wx) && ui->mouseX < int(wx + drawW) &&
                    ui->mouseY >= int(imgY) && ui->mouseY < int(imgY + drawH))
                {
                    ui->hoveredWonder = w;
                }

                // If user clicked the wonder -> toggle selection (only if it's the current player's panel)
                // We set selection only for the current player (m_state.getCurrentPlayerTurn())
                if (player == (int)m_state.getCurrentPlayerTurn() && ui)
                {
                    if (ui->leftClick && ui->hoveredWonder == w)
                    {
                        // toggle selection
                        if (ui->selectedWonder == w)
                            ui->selectedWonder = -1;
                        else
                            ui->selectedWonder = w;
                    }

                    // Right click cancels selection
                    if (ui->rightClick && ui->selectedWonder != -1)
                    {
                        ui->selectedWonder = -1;
                    }
                }

                // If this wonder is selected draw an outline (only highlight visually; selection is stored in UIState if interactive)
                if (ui && ui->selectedWonder == w && player == (int)m_state.getCurrentPlayerTurn())
                {
                    m_renderer->DrawRect(wx - 4.0f, imgY - 4.0f, drawW + 8.0f, drawH + 8.0f, Colors::Green);
                }

                wx += drawW + 8.0f;
            }
        }
    }
    curY += baseRowH + spacing;

    // Row 7: Science tokens owned (icons)
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

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawMilitaryTrack()
{
    float x0 = m_uiPos.militaryTrackX0;
    float y = m_uiPos.militaryTrackY;
    float x1 = x0 + m_layout.militaryTrackLength;

    m_renderer->DrawText("MILITARY", x0 - 120, y, Colors::Red);
    m_renderer->DrawLine(x0, y + 15, x1, y + 15, Colors::Red);

    // Show numeric military score next to the label
    int pos = m_state.getMilitary();   // -9 → +9
    m_renderer->DrawText(std::to_string(pos), x0 - 50.0f, y + 18.0f, Colors::White);

    float t = (pos + 9) / 18.0f;
    float markerX = x0 + t * m_layout.militaryTrackLength;

    m_renderer->DrawImage(
        GetMilitaryMarkerImage(),
        markerX - 15, y,
        30, 30
    );

    // Draw three token slots per player at thresholds 1,3,6.
    const int thresholds[3] = { 1, 3, 6 };
    SDL_Renderer* rdr = m_renderer->GetSDLRenderer();
    if (!rdr) return;

    const int radius = 8;
    // Colors: filled = available (white), outlined = taken (yellow)
    SDL_Color fillColor{ 255, 255, 255, 255 };
    SDL_Color outlineColor{ 255, 215, 0, 255 }; // gold-ish for taken

    // Player 0 tokens (positive side)
    for (int i = 0; i < 3; ++i)
    {
        int threshold = thresholds[i];
        float tx = x0 + (threshold + 9) / 18.0f * m_layout.militaryTrackLength;
        int cx = int(tx);
        int cy = int(y + 15);

        bool taken = pos >= threshold;
        if (taken) {
            m_renderer->DrawCircleOutline(rdr, cx, cy, radius, outlineColor);
        } else {
            m_renderer->DrawFilledCircle(rdr, cx, cy, radius, fillColor);
        }
    }

    // Player 1 tokens (negative side)
    for (int i = 0; i < 3; ++i)
    {
        int threshold = thresholds[i];
        float tx = x0 + ((-threshold) + 9) / 18.0f * m_layout.militaryTrackLength;
        int cx = int(tx);
        int cy = int(y + 15);

        bool taken = pos <= -threshold;
        if (taken) {
            m_renderer->DrawCircleOutline(rdr, cx, cy, radius, outlineColor);
        } else {
            m_renderer->DrawFilledCircle(rdr, cx, cy, radius, fillColor);
        }
    }
}

// ---------------------------------------------------------------------
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

// Draw the card graph (pyramid) and update UIState for hover/click.
void SevenWDuelRenderer::drawCardGraph(UIState* ui)
{
    const float baseX = m_uiPos.pyramidBaseX;
    const float baseY = m_uiPos.pyramidBaseY;

    const float dX = m_layout.cardW + 16.0f;
    const float dY = m_layout.cardH + 20.0f;

    const auto& graph = m_state.m_graph;

    // Count nodes per row (keep slots stable)
    std::vector<int> rowCardCounts;
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        const int row = findGraphRow(nodeIndex);
        if (rowCardCounts.size() <= row)
            rowCardCounts.resize(row + 1, 0);
        rowCardCounts[row]++;
    }

    // Helper: check if a card (age-local id) has been played
    auto isAgeCardPlayed = [&](u8 ageId) -> bool {
        for (u32 i = 0; i < m_state.m_numPlayedAgeCards; ++i)
            if (m_state.m_playedAgeCards[i] == ageId) return true;
        return false;
    };

    // Build quick lookup: nodeIndex -> playableIndex (index into m_playableCards) or -1
    std::vector<int> playableIndexOfNode(graph.size(), -1);
    for (u32 i = 0; i < m_state.m_numPlayableCards; ++i)
    {
        u8 nodeIdx = m_state.m_playableCards[i];
        if (nodeIdx < playableIndexOfNode.size())
            playableIndexOfNode[nodeIdx] = int(i);
    }

    // Draw nodes in their fixed slots. If a visible card was picked, do NOT draw it.
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        const auto& node = graph[nodeIndex];

        const int row = findGraphRow(nodeIndex);
        const int col = findGraphColumn(nodeIndex);

        if (row >= int(rowCardCounts.size()) || rowCardCounts[row] == 0)
            continue;

        const float rowWidth = rowCardCounts[row] * dX;
        const float rowStartX = baseX - (rowWidth / 2.0f);
        const float x = rowStartX + col * dX;
        const float y = baseY + row * dY;

        // compute rect for hit detection
        const float rx = x;
        const float ry = y;
        const float rw = m_layout.cardW;
        const float rh = m_layout.cardH;

        // Hover detection: store node index and playable index in UIState if mouse is over rect (only if ui provided)
        if (ui && ui->mouseX >= int(rx) && ui->mouseX < int(rx + rw) &&
            ui->mouseY >= int(ry) && ui->mouseY < int(ry + rh))
        {
            ui->hoveredNode = int(nodeIndex);
            int pidx = playableIndexOfNode[nodeIndex];
            ui->hoveredPlayableIndex = pidx;
        }

        // Visible node: real card (unless it's been played -> skip)
        if (node.m_visible)
        {
            const sevenWD::Card& card = m_state.m_context->getCard(node.m_cardId);

            // If this card (by age id) has been played, skip drawing it (keeps empty slot)
            if (isAgeCardPlayed(card.getAgeId()))
                continue;

            SDL_Texture* texture = GetCardImage(card);

            bool isPlayable = playableIndexOfNode[nodeIndex] != -1;

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

            // If user has selected a wonder (waiting for a card to use), highlight playable slots
            if (ui && ui->selectedWonder >= 0 && isPlayable && m_state.getCurrentPlayerTurn() == 0 /* selected wonder only valid for current player - renderer cannot modify game turn, but we highlight anyway */)
            {
                // draw subtle green hint
                m_renderer->DrawRect(x - 6.0f, y - 6.0f, m_layout.cardW + 12.0f, m_layout.cardH + 12.0f, Colors::Green);
            }

            m_renderer->DrawImage(texture, x, y, m_layout.cardW, m_layout.cardH);

            // If the mouse is hovering over this playable card and a click occurred, set requested move in UIState (only if ui provided)
            int playableIdx = playableIndexOfNode[nodeIndex];
            if (playableIdx != -1 && ui)
            {
                if (ui->leftClick && ui->hoveredPlayableIndex == playableIdx)
                {
                    if (ui->selectedWonder >= 0)
                    {
                        // Build wonder using this playable card
                        sevenWD::Move mv;
                        mv.playableCard = u8(playableIdx);
                        mv.action = sevenWD::Move::Action::BuildWonder;
                        mv.wonderIndex = u8(ui->selectedWonder);
                        mv.additionalId = u8(-1);

                        ui->requestedMove = mv;
                        ui->moveRequested = true;
                        ui->selectedWonder = -1; // clear selection after issuing move
                    }
                    else
                    {
                        // Regular pick
                        sevenWD::Move mv;
                        mv.playableCard = u8(playableIdx);
                        mv.action = sevenWD::Move::Action::Pick;
                        mv.wonderIndex = u8(-1);
                        mv.additionalId = u8(-1);

                        ui->requestedMove = mv;
                        ui->moveRequested = true;
                    }
                }
                else if (ui->rightClick && ui->hoveredPlayableIndex == playableIdx)
                {
                    if (ui->selectedWonder >= 0)
                    {
                        // Right click cancels wonder selection
                        ui->selectedWonder = -1;
                    }
                    else
                    {
                        // Burn
                        sevenWD::Move mv;
                        mv.playableCard = u8(playableIdx);
                        mv.action = sevenWD::Move::Action::Burn;
                        mv.wonderIndex = u8(-1);
                        mv.additionalId = u8(-1);

                        ui->requestedMove = mv;
                        ui->moveRequested = true;
                    }
                }
            }
        }
        // Hidden node: draw card back (slot reserved)
        else
        {
            SDL_Texture* back = GetCardBackImage();
            m_renderer->DrawImage(back, x, y, m_layout.cardW, m_layout.cardH);
        }
    }
}

// Image loaders (existants)
SDL_Texture* SevenWDuelRenderer::GetCardImage(const sevenWD::Card& card)
{
    return m_renderer->LoadImage("assets/cards/card.png");
}

SDL_Texture* SevenWDuelRenderer::GetCardBackImage()
{
    return m_renderer->LoadImage("assets/card_back.png");
}

SDL_Texture* SevenWDuelRenderer::GetWonderImage(sevenWD::Wonders wonder)
{
    return m_renderer->LoadImage("assets/wonders/wonder.png");
}

SDL_Texture* SevenWDuelRenderer::GetScienceTokenImage(sevenWD::ScienceToken token)
{
    return m_renderer->LoadImage("assets/tokens/token.png");
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