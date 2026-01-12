#include "7WDuelRenderer.h"

#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cctype> // added for sanitizing card names
#include <chrono> // used for double-click timing

// Helper: sanitize file name from card/wonder/token name
static std::string MakeSafeName(const char* cname)
{
    std::string safe;
    if (!cname) return safe;
    std::string name(cname);
    safe.reserve(name.size());
    for (unsigned char ch : name)
    {
        if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '(' || ch == ')')
            safe.push_back(char(ch));
        else if (std::isspace(ch))
            safe.push_back('_');
        // skip other characters (apostrophes, punctuation, etc.)
    }
    return safe;
}

// keep previous constructor
SevenWDuelRenderer::SevenWDuelRenderer(const sevenWD::GameState& state, RendererInterface* renderer)
    : m_state(state), m_renderer(renderer)
{
}

void SevenWDuelRenderer::draw(UIState* ui, UIGameState* uiGameState)
{
    if (ui)
    {
        ui->hoveredNode = -1;
        ui->hoveredPlayableIndex = -1;
        ui->hoveredWonderPlayer = -1;
        ui->hoveredWonderIndex = -1;
        ui->hoveredScienceToken = -1;
        ui->hoveredWonder = -1;
        ui->moveRequested = false;
    }

    drawBackground();
    drawPlayerCityButtons(ui, uiGameState);

    if (uiGameState && uiGameState->viewingPlayerCity && uiGameState->viewedPlayer >= 0)
    {
        drawPlayerCityView(ui, uiGameState);
        return;
    }

    // Draw current player turn in the top-left (always shown, interactive or read-only)
    {
        u32 cur = m_state.getCurrentPlayerTurn();
        std::string turnText = "Current player: " + std::to_string(int(cur) + 1);
        m_renderer->DrawText(turnText, 20.0f, 20.0f, Colors::White);
    }

    if (ui && ui->gameController)
    {
        const sevenWD::GameController* gc = ui->gameController;

        auto stateToStr = [](sevenWD::GameController::State s) -> const char*
        {
            using S = sevenWD::GameController::State;
            switch (s)
            {
            case S::DraftWonder: return "DraftWonder";
            case S::Play: return "Play";
            case S::PickScienceToken: return "PickScienceToken";
            case S::GreatLibraryToken: return "GreatLibraryToken";
            case S::GreatLibraryTokenThenReplay: return "GreatLibraryTokenThenReplay";
            case S::WinPlayer0: return "Win Player 1";
            case S::WinPlayer1: return "Win Player 2";
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

        std::string ctrlText = std::string("Controller: ") + stateToStr(gc->m_gameState.m_state) +
                               "  WinType: " + winToStr(gc->m_winType);
        // draw below the player-turn text
        m_renderer->DrawText(ctrlText, 20.0f, 44.0f, Colors::Yellow);
    }

    drawPlayers(ui);
    drawMilitaryTrack();
    drawScienceTokens(ui);
    if (m_state.isDraftingWonders())
        drawWonderDraft(ui);
    else
    {
        drawCardGraph(ui);
        drawSelectedCard(ui);
    }
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

    // Keep panel wonder sizing unchanged (do not enlarge here)
    const float panelWonderScale = m_layout.wonderPanelScale;
    const float wonderH = m_layout.wonderH * 0.6f * panelWonderScale;
    const float scienceH = 32.0f;

    // Prevent the large wonder icon size from inflating the spacing used for
    // the top rows (player name, gold, prod, etc). Compute a "header" row
    // height used for those rows and reserve a separate height for the
    // wonders row(s) later.
    const float headerRowH = std::max({ headerTextH, coinH, resourceH, chainingH, scienceH }) + 8.0f;
    const float baseRowH = headerRowH; // used for rows before Wonders

    // start Y inside panel (top padding)
    float curY = y + margin;

    // Row 0: Player name
    m_renderer->DrawText("Player " + std::to_string(player + 1), innerX + margin, curY, Colors::White);

    // Keep the first info row in its normal place, but add extra gap AFTER it.
    const float extraAfterRow1 = 12.0f; // pixels to add below the Gold/VP/Yellow row

    curY += baseRowH + spacing;

    // Row 1: Gold + VP + Yellow count
    {
        const float coinSize = coinH;
        float imgY = curY + (baseRowH - coinSize) * 0.5f;
        m_renderer->DrawImage(GetCoinImage(), innerX + margin, imgY, coinSize, coinSize);
        m_renderer->DrawText(std::to_string(city.m_gold), innerX + margin + coinSize + 8.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::Yellow);

        // right-align VP inside panel with margin
        std::string vpStr = "VP: " + std::to_string(city.m_victoryPoints);
        float vpX = innerX + innerW - margin - 80.0f; // reserve width for the VP text
        m_renderer->DrawText(vpStr, vpX, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);

        // Owned yellow cards count (new): show near the VP area
        int yellowCount = int(city.m_numCardPerType[u32(sevenWD::CardType::Yellow)]);
        std::string yellowStr = "Yellow: " + std::to_string(yellowCount);
        m_renderer->DrawText(yellowStr, vpX - 140.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::Yellow);
    }

    // advance Y by the row height + spacing, and apply the extra gap requested
    curY += baseRowH + spacing + extraAfterRow1;

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
            // allow icons up to layout size but don't force an excessively large margin;
            // keep a small room for the number text by subtracting a modest value.
            float iconW = std::min(m_layout.resourceIconW, perCell - 12.0f);
            iconW = std::max(12.0f, iconW);
            float iconH = iconW;

            // check bounds and break if no space
            if (rx + iconW + 24.0f > innerX + innerW - margin) break;

            float imgY = curY + (baseRowH - iconH) * 0.5f;
            m_renderer->DrawImage(tex, rx, imgY, iconW, iconH);

            // highlight resource icon if the player has a discount for this resource
            if (city.m_resourceDiscount[r])
            {
                m_renderer->DrawRect(rx - 3.0f, imgY - 3.0f, iconW + 6.0f, iconH + 6.0f, Colors::Yellow);
            }

            int prodVal = city.m_production[r];
            m_renderer->DrawText(std::to_string(prodVal), rx + iconW + 6.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);

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

        // Slight scale-up for chaining icons to make them more prominent
        const float chainScale = 1.15f; // 15% larger
        const float drawW = m_layout.chainingIconW * chainScale;
        const float drawH = m_layout.chainingIconH * chainScale;
        const float gap = 8.0f;

        // Skip ChainingSymbol::None (value 0)
        for (u8 s = 1; s < u8(sevenWD::ChainingSymbol::Count); ++s)
        {
            if ((city.m_chainingSymbols & (1u << s)) == 0) continue;
            if (cx + drawW > maxX) break;

            sevenWD::ChainingSymbol sym = sevenWD::ChainingSymbol(s);
            SDL_Texture* symTex = GetChainingSymbolImage(sym);

            // If no image exists for this symbol, skip drawing it.
            if (!symTex)
            {
                cx += drawW + gap;
                continue;
            }

            float imgY = curY + (baseRowH - drawH) * 0.5f;
            m_renderer->DrawImage(symTex, cx, imgY, drawW, drawH);
            cx += drawW + gap;
        }
    }
    curY += baseRowH + spacing;

    // Row 5: Owned Guilds — removed (not needed). Advance Y to keep consistent row spacing.
    {
        curY += baseRowH + spacing;
    }

    // Row 6: Wonders - use fixed desired size and wrap to multiple rows rather than shrinking to fit
    {
        m_renderer->DrawText("Wonders:", innerX + margin, curY, Colors::Cyan);
        float wx = innerX + margin + 88.0f;
        int unbuilt = city.m_unbuildWonderCount;
        if (unbuilt > 0)
        {
            float wondersAreaW = (innerX + innerW - margin) - wx;
            // desired item width (from layout + panel scale) - still keep modest for panel
            float desiredW = m_layout.wonderW * 0.6f * panelWonderScale;
            desiredW = std::max(24.0f, desiredW);

            // Compute how many items fit per row at desired size (wrap when needed)
            int perRow = std::max(1, int(std::floor((wondersAreaW + spacing) / (desiredW + spacing))));
            perRow = std::min(perRow, unbuilt); // don't claim more columns than items

            // Compute actual drawW that fits evenly for perRow columns (keeps desired size if possible)
            float drawW = desiredW;
            float totalSpacing = float(perRow - 1) * spacing;
            float maxDrawW = (wondersAreaW - totalSpacing) / float(perRow);
            if (maxDrawW < drawW)
                drawW = std::max(12.0f, maxDrawW); // shrink only if absolutely necessary

            float drawH = drawW * (m_layout.wonderH / m_layout.wonderW);

            // Number of rows required
            int rows = int(std::ceil(float(unbuilt) / float(perRow)));

            // Draw grid row-by-row, centering each row horizontally inside wondersAreaW
            for (int r = 0; r < rows; ++r)
            {
                int startIndex = r * perRow;
                int itemsThisRow = std::min(perRow, unbuilt - startIndex);
                // center this row
                float rowUsedW = itemsThisRow * drawW + (itemsThisRow - 1) * spacing;
                float rowStartX = wx + (wondersAreaW - rowUsedW) * 0.5f;

                for (int c = 0; c < itemsThisRow; ++c)
                {
                    int idx = startIndex + c;
                    float itemX = rowStartX + c * (drawW + spacing);
                    float imgY = curY + ( (rows > 1 ? (drawH/2.0f) : (baseRowH - drawH) * 0.5f) );
                    // For vertical placement ensure rows stack correctly - compute per-row Y offset
                    float rowY = curY + r * (drawH + 6.0f); // small inter-row gap 6

                    sevenWD::Wonders wonder = city.m_unbuildWonders[idx];
                    SDL_Texture* tex = GetWonderImage(wonder);
                    m_renderer->DrawImage(tex ? tex : GetWonderImage(wonder), itemX, rowY, drawW, drawH);

                    // Hit detection and selection mapping unchanged (index is linear)
                    if (ui && ui->mouseX >= int(itemX) && ui->mouseX < int(itemX + drawW) &&
                        ui->mouseY >= int(rowY) && ui->mouseY < int(rowY + drawH))
                    {
                        ui->hoveredWonderPlayer = player;
                        ui->hoveredWonderIndex = idx;
                    }

                    // Allow ANY player's wonder to be clicked to preview (toggle), not only current player's.
                    if (ui)
                    {
                        bool hoveredHere = (ui->hoveredWonderPlayer == player && ui->hoveredWonderIndex == idx);

                        if (ui->leftClick && hoveredHere)
                        {
                            if (ui->selectedWonderPlayer == player && ui->selectedWonderIndex == idx)
                            {
                                ui->selectedWonderPlayer = -1;
                                ui->selectedWonderIndex = -1;
                            }
                            else
                            {
                                ui->selectedWonderPlayer = player;
                                ui->selectedWonderIndex = idx;
                            }
                        }

                        // right-click clears selection if it belongs to that player
                        if (ui->rightClick && ui->selectedWonderPlayer == player && ui->selectedWonderIndex != -1)
                        {
                            ui->selectedWonderPlayer = -1;
                            ui->selectedWonderIndex = -1;
                        }
                    }

                    // outline if selected AND it's current player's selected wonder (keep build hint behavior)
                    if (ui && ui->selectedWonderPlayer == player && ui->selectedWonderIndex == idx && player == (int)m_state.getCurrentPlayerTurn())
                    {
                        m_renderer->DrawRect(itemX - 4.0f, rowY - 4.0f, drawW + 8.0f, drawH + 8.0f, Colors::Green);
                    }
                }
            }

            // advance curY by the height used by the wonder grid
            curY += float(rows) * (drawH + 6.0f) + spacing;
        }
        else
        {
            curY += baseRowH + spacing;
        }
    }

    // Row 7: Science symbols owned — draw dedicated symbol icons + counts.
    // Do NOT draw central science-token images here (those are selectable from
    // the central token area when the controller requests it).
    {
        m_renderer->DrawText("Science:", innerX + margin, curY, Colors::Green);
        float sx = innerX + margin + 88.0f;
        float maxX = innerX + innerW - margin;
        const float symW = m_layout.scienceSymbolW;
        const float symH = m_layout.scienceSymbolH;
        const float gap = 8.0f;

        for (int sc = 0; sc < int(sevenWD::ScienceSymbol::Count); ++sc)
        {
            int owned = city.m_ownedScienceSymbol[sc];
            if (owned <= 0) continue;

            // Wrap to next visual line if needed
            float requiredW = symW + gap + 36.0f; // icon + gap + estimated " xN" text width
            if (sx + requiredW > maxX)
            {
                curY += baseRowH;
                sx = innerX + margin + 88.0f;
                maxX = innerX + innerW - margin;
            }

            // draw symbol icon
            sevenWD::ScienceSymbol symbol = sevenWD::ScienceSymbol(sc);
            SDL_Texture* symTex = GetScienceSymbolImage(symbol);
            float imgY = curY + (baseRowH - symH) * 0.5f;
            if (symTex)
                m_renderer->DrawImage(symTex, sx, imgY, symW, symH);
            else
                m_renderer->DrawText("?", sx, imgY, Colors::White);

            // draw owned count if > 1
            if (owned > 1)
            {
                std::string cnt = "x" + std::to_string(owned);
                m_renderer->DrawText(cnt, sx + symW + 6.0f, curY + (baseRowH * 0.5f) + 6.0f, Colors::White);
            }
            sx += symW + gap + 36.0f;
        }
    }
}

void SevenWDuelRenderer::drawPlayerCityButtons(UIState* ui, UIGameState* uiGameState)
{
    const float buttonW = 150.0f;
    const float buttonH = 44.0f;
    const float topY = 75.0f;
    const float startX = 210.0f;
    const float spacing = 28.0f;

    auto drawButton = [&](float x, float y, const std::string& label, bool hovered, bool active)
    {
        SDL_Texture* panel = GetBackgroundPanel();
        if (panel)
            m_renderer->DrawImage(panel, x, y, buttonW, buttonH);
        SDL_Color borderColor = active ? Colors::Green : (hovered ? Colors::Yellow : Colors::White);
        m_renderer->DrawRect(x, y, buttonW, buttonH, borderColor);
        m_renderer->DrawText(label, x + 14.0f, y + 12.0f, Colors::White);
    };

    for (int player = 0; player < 2; ++player)
    {
        float x = startX + player * (buttonW + spacing);
        bool hovered = ui && ui->mouseX >= int(x) && ui->mouseX <= int(x + buttonW) &&
                        ui->mouseY >= int(topY) && ui->mouseY <= int(topY + buttonH);
        bool active = uiGameState && uiGameState->viewingPlayerCity && uiGameState->viewedPlayer == player;

        drawButton(x, topY, "Player " + std::to_string(player + 1) + " city", hovered, active);

        if (ui && uiGameState && ui->leftClick && hovered)
        {
            uiGameState->viewingPlayerCity = true;
            uiGameState->viewedPlayer = player;
            ui->selectedNode = -1;
            ui->selectedWonderPlayer = -1;
            ui->selectedWonderIndex = -1;
        }
    }

    if (uiGameState && uiGameState->viewingPlayerCity)
    {
        float backX = startX - buttonW - spacing;
        bool hovered = ui && ui->mouseX >= int(backX) && ui->mouseX <= int(backX + buttonW) &&
                        ui->mouseY >= int(topY) && ui->mouseY <= int(topY + buttonH);
        drawButton(backX, topY, "Back", hovered, false);
        if (ui && ui->leftClick && hovered)
        {
            uiGameState->resetView();
        }
    }
}

void SevenWDuelRenderer::drawPlayerCityView(UIState* /*ui*/, UIGameState* uiGameState)
{
    if (!uiGameState || !uiGameState->viewingPlayerCity || uiGameState->viewedPlayer < 0)
        return;

    int player = std::clamp(uiGameState->viewedPlayer, 0, 1);
    const auto& cards = uiGameState->pickedCards[player];

    const float panelX = 120.0f;
    const float panelY = 140.0f;
    const float panelW = 1680.0f;
    const float panelH = 840.0f;
    SDL_Texture* panel = GetBackgroundPanel();
    if (panel)
        m_renderer->DrawImage(panel, panelX, panelY, panelW, panelH);

    std::string header = "Player " + std::to_string(player + 1) + " city (" + std::to_string(cards.size()) + " cards)";
    m_renderer->DrawText(header, panelX + 18.0f, panelY + 18.0f, Colors::Yellow);

    if (cards.empty())
    {
        m_renderer->DrawText("No cards picked yet.", panelX + 18.0f, panelY + 60.0f, Colors::White);
        return;
    }

    const std::array<sevenWD::CardType, 9> order = {
        sevenWD::CardType::Brown,
        sevenWD::CardType::Grey,
        sevenWD::CardType::Yellow,
        sevenWD::CardType::Blue,
        sevenWD::CardType::Military,
        sevenWD::CardType::Science,
        sevenWD::CardType::Guild,
        sevenWD::CardType::Wonder,
        sevenWD::CardType::ScienceToken
    };

    // Layout area inside panel
    const float innerX = panelX + 24.0f;
    const float innerWidth = panelW - 48.0f;
    const float contentStartY = panelY + 60.0f;
    const float contentAvailableH = panelH - (contentStartY - panelY) - 60.0f; // leave bottom margin

    // Presentation constants (match previous values)
    const float sectionTitleH = 28.0f;
    const float sectionSpacingAfter = 20.0f;
    const float defaultCardW = 88.0f;
    const float minCardW = 60.0f; // allow some scaling if needed
    const float columnGap = 24.0f;

    // Build filtered lists per type preserving 'order'
    struct TypeBlock { sevenWD::CardType type; std::vector<const sevenWD::Card*> cards; };
    std::vector<TypeBlock> blocks;
    blocks.reserve(order.size());
    for (auto type : order)
    {
        TypeBlock tb{ type, {} };
        for (const sevenWD::Card* card : cards)
            if (card && card->getType() == type) tb.cards.push_back(card);
        if (!tb.cards.empty()) blocks.push_back(std::move(tb));
    }

    // Helper lambdas: compute height (no draw) and draw a block using variable cardW
    auto calcBlockHeight = [&](const std::vector<const sevenWD::Card*>& bcards, float maxWidth, float cardW) -> float
    {
        if (bcards.empty()) return 0.0f;
        // replicate drawPlayerCityCardGrid sizing logic
        const float spacing = 12.0f;
        const float labelH = 0.0f;
        float cardH = cardW * (m_layout.cardH / m_layout.cardW);

        int perRow = std::max(1, int((maxWidth + spacing) / (cardW + spacing)));
        int rows = int((bcards.size() + perRow - 1) / perRow);
        float rowHeight = cardH + spacing + labelH;
        float used = std::max(0, rows) * rowHeight - spacing;
        return sectionTitleH + used + sectionSpacingAfter;
    };

    auto drawBlock = [&](const std::vector<const sevenWD::Card*>& bcards, float startX, float& yRef, float maxWidth, float cardW)
    {
        if (bcards.empty()) return;
        m_renderer->DrawText(" ", startX, yRef, Colors::Cyan); // placeholder: caller draws proper title
        // drawPlayerCityCardGrid-like rendering with custom cardW
        const float spacing = 12.0f;
        const float labelH = 0.0f;
        float cardH = cardW * (m_layout.cardH / m_layout.cardW);

        int perRow = std::max(1, int((maxWidth + spacing) / (cardW + spacing)));
        float x = startX;
        yRef += sectionTitleH;
        float curYLocal = yRef;
        int col = 0;
        for (const sevenWD::Card* card : bcards)
        {
            drawCityCardSprite(*card, x, curYLocal, cardW, cardH);
            ++col;
            if (col >= perRow)
            {
                col = 0;
                x = startX;
                curYLocal += cardH + spacing + labelH;
            }
            else
            {
                x += cardW + spacing;
            }
        }
        int rows = int((bcards.size() + perRow - 1) / perRow);
        float rowHeight = cardH + spacing + labelH;
        yRef = yRef + std::max(0, rows) * rowHeight - spacing + sectionSpacingAfter;
    };

    // Quick check: single-column layout with per-block card widths (wonders larger)
    float totalSingle = 0.0f;
    for (const TypeBlock& tb : blocks)
    {
        float blockCardW = defaultCardW;
        if (tb.type == sevenWD::CardType::Wonder)
            blockCardW = std::max(defaultCardW, m_layout.wonderW * 1.6f); // larger for wonders here
        totalSingle += calcBlockHeight(tb.cards, innerWidth, blockCardW);
    }

    if (totalSingle <= contentAvailableH)
    {
        // fits in single column -> simple draw in original order
        float y = contentStartY;
        for (const TypeBlock& tb : blocks)
        {
            const char* typeName = cardTypeToString(tb.type);
            m_renderer->DrawText(typeName ? typeName : "Cards", innerX, y, Colors::Cyan);

            // pick block-specific card width (wonders larger)
            float cardWForBlock = defaultCardW;
            if (tb.type == sevenWD::CardType::Wonder)
                cardWForBlock = std::max(defaultCardW, m_layout.wonderW * 1.6f);

            drawBlock(tb.cards, innerX, y, innerWidth, cardWForBlock);

            if (y > panelY + panelH - 60.0f) break;
        }
        return;
    }

    // Need two-column layout. We'll assign blocks to columns greedily:
    float colWidth = (innerWidth - columnGap) * 0.5f;

    // Start with default cardW and attempt to fit by scaling down if needed
    float cardW = defaultCardW;
    bool fits = false;
    for (float scale = 1.0f; scale >= 0.65f; scale -= 0.05f)
    {
        cardW = defaultCardW * scale;
        // compute block heights when rendered in one column of width colWidth
        struct HInfo { int idx; float h; };
        std::vector<HInfo> infos; infos.reserve(blocks.size());
        for (int i = 0; i < (int)blocks.size(); i++)
        {
            float blockCardW = cardW;
            if (blocks[i].type == sevenWD::CardType::Wonder)
                blockCardW = std::max(cardW, m_layout.wonderW * 1.6f);
            infos.push_back({ i, calcBlockHeight(blocks[i].cards, colWidth, blockCardW) });
        }

        // sort indices by block height descending so largest goes first to left column
        std::sort(infos.begin(), infos.end(), [](const HInfo& a, const HInfo& b) { return a.h > b.h; });

        float leftH = 0.0f, rightH = 0.0f;
        std::vector<int> leftIdx, rightIdx;
        for (auto &inf : infos)
        {
            if (leftIdx.empty())
            {
                leftIdx.push_back(inf.idx);
                leftH += inf.h;
            }
            else
            {
                if (leftH <= rightH)
                {
                    leftIdx.push_back(inf.idx);
                    leftH += inf.h;
                }
                else
                {
                    rightIdx.push_back(inf.idx);
                    rightH += inf.h;
                }
            }
        }

        if (std::max(leftH, rightH) <= contentAvailableH)
        {
            // success, render using this cardW and assignment
            fits = true;

            // Build ordered lists in original order but placed into assigned columns
            std::vector<TypeBlock> leftBlocks, rightBlocks;
            std::unordered_set<int> leftSet(leftIdx.begin(), leftIdx.end()), rightSet(rightIdx.begin(), rightIdx.end());
            for (int i = 0; i < (int)blocks.size(); i++)
            {
                if (leftSet.count(i)) leftBlocks.push_back(blocks[i]);
                else rightBlocks.push_back(blocks[i]);
            }

            // Draw columns
            float leftX = innerX;
            float rightX = innerX + colWidth + columnGap;
            float yLeft = contentStartY;
            float yRight = contentStartY;

            // Left column draw
            for (const auto& tb : leftBlocks)
            {
                const char* typeName = cardTypeToString(tb.type);
                m_renderer->DrawText(typeName ? typeName : "Cards", leftX, yLeft, Colors::Cyan);

                float cardWUsed = cardW;
                if (tb.type == sevenWD::CardType::Wonder)
                    cardWUsed = std::max(cardW, m_layout.wonderW * 1.6f);

                drawBlock(tb.cards, leftX, yLeft, colWidth, cardWUsed);
            }

            // Right column draw
            for (const auto& tb : rightBlocks)
            {
                const char* typeName = cardTypeToString(tb.type);
                m_renderer->DrawText(typeName ? typeName : "Cards", rightX, yRight, Colors::Cyan);

                float cardWUsed = cardW;
                if (tb.type == sevenWD::CardType::Wonder)
                    cardWUsed = std::max(cardW, m_layout.wonderW * 1.6f);

                drawBlock(tb.cards, rightX, yRight, colWidth, cardWUsed);
            }

            break;
        }
    }

    if (!fits)
    {
        // As a last resort, fall back to single-column clipped rendering using the smallest allowed cardW
        float y = contentStartY;
        for (const TypeBlock& tb : blocks)
        {
            const char* typeName = cardTypeToString(tb.type);
            m_renderer->DrawText(typeName ? typeName : "Cards", innerX, y, Colors::Cyan);
            y += sectionTitleH;
            // draw with reduced cardW to minimize overflow; use our local drawBlock to ensure consistent sizing
            drawBlock(tb.cards, innerX, y, innerWidth, minCardW);
            if (y > panelY + panelH - 60.0f) break;
        }
    }
}

float SevenWDuelRenderer::drawPlayerCityCardGrid(const std::vector<const sevenWD::Card*>& cards, float startX, float startY, float maxWidth)
{
    if (cards.empty())
        return 0.0f;

    // Scaled down card sizing so more cards fit the player city view.
    // Removed name label under cards to save vertical space.
    const float cardW = 88.0f; // reduced from 110
    const float cardH = cardW * (m_layout.cardH / m_layout.cardW);
    const float spacing = 12.0f; // reduced spacing
    const float labelH = 0.0f;   // no card name printed anymore

    int perRow = std::max(1, int((maxWidth + spacing) / (cardW + spacing)));
    float x = startX;
    float y = startY;
    int col = 0;

    for (const sevenWD::Card* card : cards)
    {
        drawCityCardSprite(*card, x, y, cardW, cardH);

        ++col;
        if (col >= perRow)
        {
            col = 0;
            x = startX;
            y += cardH + spacing + labelH;
        }
        else
        {
            x += cardW + spacing;
        }
    }

    int rows = int((cards.size() + perRow - 1) / perRow);
    float rowHeight = cardH + spacing + labelH;
    return std::max(0, rows) * rowHeight - spacing;
}

const char* SevenWDuelRenderer::cardTypeToString(sevenWD::CardType type) const
{
    switch (type)
    {
    case sevenWD::CardType::Brown: return "Brown";
    case sevenWD::CardType::Grey: return "Grey";
    case sevenWD::CardType::Yellow: return "Yellow";
    case sevenWD::CardType::Blue: return "Blue";
    case sevenWD::CardType::Military: return "Military";
    case sevenWD::CardType::Science: return "Science";
    case sevenWD::CardType::Guild: return "Guild";
    case sevenWD::CardType::Wonder: return "Wonder";
    case sevenWD::CardType::ScienceToken: return "Science Tokens";
    default: return "Cards";
    }
}

void SevenWDuelRenderer::drawCityCardSprite(const sevenWD::Card& card, float x, float y, float w, float h)
{
    SDL_Texture* tex = nullptr;
    bool isWonder = false;
    bool isScienceToken = false;

    if (card.getType() == sevenWD::CardType::Wonder)
    {
        tex = GetWonderImage(static_cast<sevenWD::Wonders>(card.getSecondaryType()));
        isWonder = true;
    }
    else if (card.getType() == sevenWD::CardType::ScienceToken)
    {
        tex = GetScienceTokenImage(static_cast<sevenWD::ScienceToken>(card.getSecondaryType()));
        isScienceToken = true;
    }
    else
    {
        tex = GetCardImage(card);
    }

    if (!tex)
    {
        m_renderer->DrawText("[missing]", x, y, Colors::White);
        return;
    }

    // Preserve appropriate aspect ratio depending on the asset type.
    float aspect = 0.0f;
    if (isWonder)
    {
        aspect = (m_layout.wonderW > 0.0f && m_layout.wonderH > 0.0f)
                 ? (m_layout.wonderW / m_layout.wonderH)
                 : (m_layout.cardW / m_layout.cardH);
    }
    else if (isScienceToken)
    {
        aspect = (m_layout.tokenW > 0.0f && m_layout.tokenH > 0.0f)
                 ? (m_layout.tokenW / m_layout.tokenH)
                 : (m_layout.cardW / m_layout.cardH);
    }
    else
    {
        aspect = (m_layout.cardW > 0.0f && m_layout.cardH > 0.0f)
                 ? (m_layout.cardW / m_layout.cardH)
                 : 1.0f;
    }

    // Fit texture into the provided cell (w x h) while preserving aspect.
    float targetW = w;
    float targetH = (aspect > 0.0f) ? (targetW / aspect) : h;
    if (targetH > h)
    {
        targetH = h;
        targetW = (aspect > 0.0f) ? (targetH * aspect) : w;
    }

    // Center inside cell
    float drawX = x + (w - targetW) * 0.5f;
    float drawY = y + (h - targetH) * 0.5f;

    m_renderer->DrawImage(tex, drawX, drawY, targetW, targetH);
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawScienceTokens(UIState* ui)
{
    float x = m_uiPos.scienceTokensX;
    float y = m_uiPos.scienceTokensY;

    // Move the label further above the tokens using token height for spacing
    float textY = y - (m_layout.tokenH * 0.5f) - 12.0f;
    m_renderer->DrawText("Science Tokens", x, textY, Colors::Green);

    // reset hovered science token each frame
    if (ui)
        ui->hoveredScienceToken = -1;

    const sevenWD::GameController* gc = ui ? ui->gameController : nullptr;

    // detect controller states that change token interaction mode
    bool isPickState = gc && gc->m_gameState.m_state == sevenWD::GameState::State::PickScienceToken;
    bool isGreatLibraryState = gc && (gc->m_gameState.m_state == sevenWD::GameState::State::GreatLibraryToken ||
        gc->m_gameState.m_state == sevenWD::GameState::State::GreatLibraryTokenThenReplay);

    // Normal board tokens (when controller asks for pick from the board)
    if (isPickState || !isGreatLibraryState)
    {
        // Pool is always 0-based for getPlayableScienceToken, "normal" mode.
        for (int i = 0; i < m_state.m_numScienceToken; ++i)
        {
            const sevenWD::Card& card = m_state.getPlayableScienceToken(i, /*isGreatLib*/ false);
            sevenWD::ScienceToken type = (sevenWD::ScienceToken)card.getSecondaryType();

            float tx = x + i * (m_layout.tokenW + 10.0f);
            float tw = m_layout.tokenW;
            float th = m_layout.tokenH;

            // draw token image
            m_renderer->DrawImage(
                GetScienceTokenImage(type),
                tx,
                y,
                tw,
                th
            );

            // interactive handling: hover + pick when controller asks for token selection
            if (ui)
            {
                if (ui->mouseX >= int(tx) && ui->mouseX < int(tx + tw) &&
                    ui->mouseY >= int(y) && ui->mouseY < int(y + th))
                {
                    ui->hoveredScienceToken = i;
                    // highlight hovered token
                    m_renderer->DrawRect(tx - 4.0f, y - 4.0f, tw + 8.0f, th + 8.0f, Colors::Yellow);

                    // If the controller is in a token-picking state allow left click to request a ScienceToken move.
                    if (isPickState && ui->leftClick)
                    {
                        sevenWD::Move mv;
                        // playableCard is the index in the "normal" pool (0-based)
                        mv.playableCard = u8(i);
                        mv.action = sevenWD::Move::Action::ScienceToken;
                        mv.wonderIndex = u8(-1);
                        mv.additionalId = u8(-1);

                        ui->requestedMove = mv;
                        ui->moveRequested = true;
                    }
                }
            }
        }
    }

    // Great Library draft tokens (presented when GreatLibrary wonder grants a choice)
    if (isGreatLibraryState)
    {
        // Great Library pool is also treated as its own 0-based pool.
        // getPlayableScienceToken(idx, true) will interpret idx as "Great Library" offset.
        static constexpr int kGreatLibraryCount = 3;

        const float spacing = 12.0f;
        float totalW = kGreatLibraryCount * m_layout.tokenW + (kGreatLibraryCount - 1) * spacing;
        float startX = x - totalW * 0.5f;

        for (int localIdx = 0; localIdx < kGreatLibraryCount; ++localIdx)
        {
            // Bound check against underlying storage; caller guarantees the pool exists.
            if (localIdx < 0 || localIdx >= m_state.m_numScienceToken)
                continue;

            const sevenWD::Card& tokenCard = m_state.getPlayableScienceToken(localIdx, /*isGreatLib*/ true);
            sevenWD::ScienceToken token = (sevenWD::ScienceToken)tokenCard.getSecondaryType();

            float tx = startX + localIdx * (m_layout.tokenW + spacing);
            float tw = m_layout.tokenW;
            float th = m_layout.tokenH;

            m_renderer->DrawImage(GetScienceTokenImage(token), tx, y, tw, th);

            if (ui)
            {
                if (ui->mouseX >= int(tx) && ui->mouseX < int(tx + tw) &&
                    ui->mouseY >= int(y) && ui->mouseY < int(y + th))
                {
                    // hover index is also in the local Great Library pool coordinates (0..2)
                    ui->hoveredScienceToken = localIdx;
                    m_renderer->DrawRect(tx - 4.0f, y - 4.0f, tw + 8.0f, th + 8.0f, Colors::Yellow);

                    if (ui->leftClick)
                    {
                        sevenWD::Move mv{};
                        mv.action = sevenWD::Move::Action::ScienceToken;
                        // For GreatLibrary* states, playableCard is the index in the Great Library pool (0-based).
                        mv.playableCard = u8(localIdx);
                        mv.wonderIndex = u8(-1);
                        mv.additionalId = u8(-1);

                        ui->requestedMove = mv;
                        ui->moveRequested = true;
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------
int SevenWDuelRenderer::findGraphRow(u32 nodeIndex) const
{
    // access active graph CardNode array
    const auto& nodes = m_state.m_graph.m_graph;

    const auto& node = nodes[nodeIndex];

    int r0 = 0, r1 = 0;
    if (node.m_parent0 != sevenWD::GameState::CardNode::InvalidNode)
        r0 = 1 + findGraphRow(node.m_parent0);
    if (node.m_parent1 != sevenWD::GameState::CardNode::InvalidNode)
        r1 = 1 + findGraphRow(node.m_parent1);

    return std::max(r0, r1);
}

// Find the column for a given node in the graph (based on row)
int SevenWDuelRenderer::findGraphColumn(u32 nodeIndex) const
{
    const int row = findGraphRow(nodeIndex);
    int col = 0;

    // iterate over active graph nodes
    const auto& nodes = m_state.m_graph.m_graph;
    for (u32 i = 0; i < nodes.size(); ++i)
    {
        if (findGraphRow(i) == row)
        {
            if (i == nodeIndex) return col;
            col++;
        }
    }

    return col;
}

// Draw the card graph (pyramid) and update UIState for hover/click.
void SevenWDuelRenderer::drawCardGraph(UIState* ui)
{
    const float baseX = m_uiPos.pyramidBaseX;
    const float baseY = m_uiPos.pyramidBaseY;

    const float dX = m_layout.cardW + 16.0f;
    const float dY = m_layout.cardH + 20.0f;

    // active graph setup and node array
    const auto& setup = m_state.m_graph;
    const auto& graph = setup.m_graph;

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

    // Build quick lookup: nodeIndex -> playableIndex (index into setup.m_playableCards) or -1
    std::vector<int> playableIndexOfNode(graph.size(), -1);
    for (u32 i = 0; i < setup.m_numPlayableCards; ++i)
    {
        u8 nodeIdx = setup.m_playableCards[i];
        if (nodeIdx < playableIndexOfNode.size())
            playableIndexOfNode[nodeIdx] = int(i);
    }

    // Compute X positions for all nodes
    std::vector<float> nodeXPositions(graph.size(), 0.0f);
    
    // Find max row
    int maxRow = 0;
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        maxRow = std::max(maxRow, findGraphRow(nodeIndex));
    }

    // Process row by row from top to bottom
    for (int row = 0; row <= maxRow; ++row)
    {
        // Collect nodes in this row
        std::vector<u32> nodesInRow;
        for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
        {
            if (findGraphRow(nodeIndex) == row)
                nodesInRow.push_back(nodeIndex);
        }

        if (nodesInRow.empty())
            continue;

        if (row == 0)
        {
            // Top row: center all cards with fixed spacing
            const float rowWidth = (nodesInRow.size() - 1) * dX;
            const float rowStartX = baseX - (rowWidth / 2.0f);
            for (size_t i = 0; i < nodesInRow.size(); ++i)
            {
                nodeXPositions[nodesInRow[i]] = rowStartX + i * dX;
            }
        }
        else
        {
            // For each node, compute position based on parent(s)
            for (u32 nodeIndex : nodesInRow)
            {
                const auto& node = graph[nodeIndex];
                
                bool hasParent0 = node.m_parent0 != sevenWD::GameState::CardNode::InvalidNode;
                bool hasParent1 = node.m_parent1 != sevenWD::GameState::CardNode::InvalidNode;

                if (hasParent0 && hasParent1)
                {
                    // Two parents: position exactly between them
                    float p0x = nodeXPositions[node.m_parent0];
                    float p1x = nodeXPositions[node.m_parent1];
                    nodeXPositions[nodeIndex] = (p0x + p1x) / 2.0f;
                }
                else if (hasParent0 || hasParent1)
                {
                    // Single parent: need to determine if we're left or right child
                    u32 parentIdx = hasParent0 ? node.m_parent0 : node.m_parent1;
                    float parentX = nodeXPositions[parentIdx];
                    const auto& parentNode = graph[parentIdx];
                    
                    // Check if this node is the "left" or "right" child of the parent
                    // by looking at which children the parent has
                    // We need to find all children of this parent and see our position
                    
                    // Find all nodes that have this parent
                    std::vector<u32> siblings;
                    for (u32 otherIdx = 0; otherIdx < graph.size(); ++otherIdx)
                    {
                        const auto& otherNode = graph[otherIdx];
                        if (otherNode.m_parent0 == parentIdx || otherNode.m_parent1 == parentIdx)
                        {
                            // This node is a child of our parent
                            if (findGraphRow(otherIdx) == row)
                                siblings.push_back(otherIdx);
                        }
                    }
                    
                    // Sort siblings by their node index to get consistent left/right ordering
                    std::sort(siblings.begin(), siblings.end());
                    
                    if (siblings.size() == 2)
                    {
                        // Two children sharing one parent: left child goes left, right child goes right
                        if (nodeIndex == siblings[0])
                            nodeXPositions[nodeIndex] = parentX - dX / 2.0f;
                        else
                            nodeXPositions[nodeIndex] = parentX + dX / 2.0f;
                    }
                    else
                    {
                        // Single child with single parent (shouldn't happen in initial setup,
                        // but can happen after cards are played)
                        // Default to centering under parent with slight offset based on node index parity
                        if (nodeIndex < parentIdx)
                            nodeXPositions[nodeIndex] = parentX - dX / 2.0f;
                        else
                            nodeXPositions[nodeIndex] = parentX + dX / 2.0f;
                    }
                }
                else
                {
                    // No parents (shouldn't happen), fallback to column-based
                    int col = findGraphColumn(nodeIndex);
                    const float rowWidth = (rowCardCounts[row] - 1) * dX;
                    const float rowStartX = baseX - (rowWidth / 2.0f);
                    nodeXPositions[nodeIndex] = rowStartX + col * dX;
                }
            }
        }
    }

    // Draw nodes using the computed positions
    for (u32 nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex)
    {
        const auto& node = graph[nodeIndex];

        const int row = findGraphRow(nodeIndex);
        const float x = nodeXPositions[nodeIndex];
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

            // draw yellow playable background rect first (acts as border)
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

            // draw the card image (must be drawn before outlines that should appear on top)
            m_renderer->DrawImage(texture, x, y, m_layout.cardW, m_layout.cardH);

            // If this node is selected (single click), draw a red outline on top of the card
            if (ui && ui->selectedNode == int(nodeIndex))
            {
                m_renderer->DrawRect(x - 6.0f, y - 6.0f, m_layout.cardW + 12.0f, m_layout.cardH + 12.0f, Colors::Red);
            }

            // If the current player has selected one of their wonders, highlight playable slots.
            if (ui && ui->selectedWonderIndex >= 0 &&
                ui->selectedWonderPlayer == (int)m_state.getCurrentPlayerTurn() &&
                isPlayable)
            {
                // draw subtle green hint on top
                m_renderer->DrawRect(x - 6.0f, y - 6.0f, m_layout.cardW + 12.0f, m_layout.cardH + 12.0f, Colors::Green);
            }

            // NEW: selection / double-click logic:
            int playableIdx = playableIndexOfNode[nodeIndex];
            if (ui && playableIdx != -1)
            {
                // right click cancels selection
                if (ui->rightClick && ui->selectedNode != -1)
                    ui->selectedNode = -1;

                if (ui->leftClick && ui->hoveredPlayableIndex == playableIdx)
                {
                    // use time-based double-click detection
                    auto now = std::chrono::steady_clock::now();
                    if (ui->selectedNode == int(nodeIndex) &&
                        m_lastClickedNode == int(nodeIndex) &&
                        (now - m_lastClickTime) <= std::chrono::milliseconds(m_doubleClickMs))
                    {
                        // confirmed action: build wonder if the current player's wonder is selected, otherwise pick
                        if (ui->selectedWonderIndex >= 0 && ui->selectedWonderPlayer == (int)m_state.getCurrentPlayerTurn())
                        {
                            sevenWD::Move mv;
                            mv.playableCard = u8(playableIdx);
                            mv.action = sevenWD::Move::Action::BuildWonder;
                            mv.wonderIndex = u8(ui->selectedWonderIndex);
                            mv.additionalId = u8(-1);

                            ui->requestedMove = mv;
                            ui->moveRequested = true;
                            ui->selectedWonderPlayer = -1;
                            ui->selectedWonderIndex = -1;
                        }
                        else
                        {
                            sevenWD::Move mv;
                            mv.playableCard = u8(playableIdx);
                            mv.action = sevenWD::Move::Action::Pick;
                            mv.wonderIndex = u8(-1);
                            mv.additionalId = u8(-1);

                            ui->requestedMove = mv;
                            ui->moveRequested = true;
                        }

                        // clear selection after confirming
                        ui->selectedNode = -1;
                        m_lastClickedNode = -1;
                        m_lastClickTime = std::chrono::steady_clock::time_point::min();
                    }
                    else
                    {
                        // first click -> select node (highlight in red)
                        ui->selectedNode = int(nodeIndex);
                        m_lastClickedNode = int(nodeIndex);
                        m_lastClickTime = now;
                    }
                }
                // allow burning on right-click if not selecting and no wonder selected by current player
                else if (ui->rightClick && ui->hoveredPlayableIndex == playableIdx &&
                    !(ui->selectedWonderIndex >= 0 && ui->selectedWonderPlayer == (int)m_state.getCurrentPlayerTurn()))
                {
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
        // Hidden node: decide back image from node flags + current age (node.m_cardId may be invalid)
        else
        {
            bool isGuild = node.m_isGuildCard != 0;
            u32 age = m_state.getCurrentAge(); // 0..2 (or u32(-1) if uninitialized)
            SDL_Texture* back = GetCardBackImageForNode(isGuild, age);
            m_renderer->DrawImage(back, x, y, m_layout.cardW, m_layout.cardH);
        }
    }
}

void SevenWDuelRenderer::drawWonderDraft(UIState* ui)
{
    u8 count = m_state.getNumDraftableWonders();
    if (count == 0)
        return;

    // Arrange drafted wonders in a 2x2 grid (columns fixed to 2) so each card can be larger.
    const int cols = 2;
    const int rows = (count + cols - 1) / cols;

    // Card scale and spacing come from UIPosition so the grid is adjustable at runtime.
    const float cardW = m_layout.wonderW * m_uiPos.wonderDraftCardScale;
    const float cardH = m_layout.wonderH * m_uiPos.wonderDraftCardScale;
    const float spacing = m_uiPos.wonderDraftSpacing;

    // Compute total grid size and top-left origin
    const float totalWidth = cols * cardW + (cols - 1) * spacing;
    const float totalHeight = rows * cardH + (rows - 1) * spacing;
    const float startX = m_uiPos.wonderDraftBaseX - totalWidth * 0.5f;
    const float startY = m_uiPos.wonderDraftBaseY - totalHeight * 0.5f;

    // Title placed above the grid
    std::string title = "Wonder Draft - Player " + std::to_string(m_state.getCurrentPlayerTurn() + 1);
    m_renderer->DrawText(title, startX, startY - m_uiPos.wonderDraftTitleOffset, Colors::Yellow);
    std::string round = "Round " + std::to_string(m_state.getCurrentWonderDraftRound() + 1) + "/2";
    m_renderer->DrawText(round, startX, startY - m_uiPos.wonderDraftRoundOffset, Colors::White);

    const bool canRequestMove = ui && ui->gameController && ui->gameController->m_gameState.m_state == sevenWD::GameState::State::DraftWonder;

    for (u8 i = 0; i < count; ++i)
    {
        int r = i / cols;
        int c = i % cols;

        float x = startX + c * (cardW + spacing);
        float y = startY + r * (cardH + spacing);
        SDL_Rect rect{ int(x), int(y), int(cardW), int(cardH) };

        bool hovered = ui &&
            ui->mouseX >= rect.x && ui->mouseX <= rect.x + rect.w &&
            ui->mouseY >= rect.y && ui->mouseY <= rect.y + rect.h;

        if (hovered && ui)
        {
            ui->hoveredWonder = i;
            m_renderer->DrawRect(x - 6.0f, y - 6.0f, cardW + 12.0f, cardH + 12.0f, Colors::Yellow);

            if (ui->leftClick && canRequestMove)
            {
                sevenWD::Move mv{};
                mv.action = sevenWD::Move::Action::DraftWonder;
                mv.playableCard = i;
                ui->requestedMove = mv;
                ui->moveRequested = true;
            }
        }

        SDL_Texture* tex = GetWonderImage(m_state.getDraftableWonder(i));
        if (tex)
            m_renderer->DrawImage(tex, x, y, cardW, cardH);
        else
            m_renderer->DrawText("Wonder", x + 12.0f, y + cardH * 0.5f, Colors::White);
    }
}

// ---------------------------------------------------------------------
void SevenWDuelRenderer::drawMilitaryTrack()
{
    float x0 = m_uiPos.militaryTrackX0;
    float y = m_uiPos.militaryTrackY;
    float x1 = x0 + m_layout.militaryTrackLength;

    const float trackY = y + 15.0f;

    // Helper: map military value (-9..+9, integer) or half-integer boundary to X coordinate on track
    auto valueToX = [&](double value) -> float {
        if (value < -9.5) value = -9.5;
        if (value > 9.5) value = 9.5;
        // REVERSED mapping: positive values now move toward the left (player 0 panel),
        // negative toward the right (player 1). This matches gameplay where positive
        // m_military favors player 0.
        double t = (9.5 - value) / 19.0; // flip direction compared to previous implementation
        return x0 + float(t * m_layout.militaryTrackLength);
    };

    // Precompute boundary Xs for positions -9.5, -8.5, ... +9.5 (20 boundaries).
    // The visual "slot" for score V is the horizontal span between boundary[a] and boundary[b+1]
    std::array<float, 20> boundX;
    for (int i = 0; i < 20; ++i)
        boundX[i] = valueToX(-9.5 + i);

    // Colored segments (slot-based). A segment covering scores [a..b] fills from boundary[a] to boundary[b+1]
    struct Segment { int a, b; SDL_Color color; const char* label; };
    const Segment segs[] = {
        { 1, 2, Colors::Green,  "2 VP"  },
        { 3, 5, Colors::Cyan,   "5 VP"  },
        { 6, 8, Colors::Yellow, "10 VP" }
    };

    const int bandHalf = 3; // half thickness in pixels (total thickness = bandHalf*2+1)
    for (const Segment& s : segs)
    {
        int startIndex = s.a + 9;
        int endIndex = s.b + 1 + 9;
        if (startIndex >= 0 && endIndex < int(boundX.size()) && endIndex > startIndex)
        {
            float sx = boundX[startIndex];
            float ex = boundX[endIndex];
            for (int dy = -bandHalf; dy <= bandHalf; ++dy)
                m_renderer->DrawLine(sx, trackY + dy, ex, trackY + dy, s.color);

            float cx = (sx + ex) * 0.5f;
            m_renderer->DrawText(s.label, cx - 18.0f, trackY - 35.0f, Colors::White);
        }

        int nStartIndex = -s.b + 9;
        int nEndIndex = -s.a + 1 + 9;
        if (nStartIndex >= 0 && nEndIndex < int(boundX.size()) && nEndIndex > nStartIndex)
        {
            float sxn = boundX[nStartIndex];
            float exn = boundX[nEndIndex];
            for (int dy = -bandHalf; dy <= bandHalf; ++dy)
                m_renderer->DrawLine(sxn, trackY + dy, exn, trackY + dy, s.color);

            float cxn = (sxn + exn) * 0.5f;
            m_renderer->DrawText(s.label, cxn - 18.0f, trackY - 35.0f, Colors::White);
        }
    }

    // Draw baseline on top of bands
    m_renderer->DrawText("MILITARY", x0 - 120, y, Colors::Red);
    m_renderer->DrawLine(x0, trackY, x1, trackY, Colors::Red);

    // Numeric current military score
    int pos = m_state.getMilitary();   // -9 → +9
    m_renderer->DrawText(std::to_string(pos), x0 - 50.0f, y + 18.0f, Colors::White);

    // Draw vertical separators at boundaries (-9.5..+9.5).
    const float sepHalfH = 12.0f;
    const float smallTop = trackY - sepHalfH * 0.6f;
    const float smallBottom = trackY + sepHalfH * 0.6f;
    for (int i = 0; i < int(boundX.size()); ++i)
    {
        float x = boundX[i];
        m_renderer->DrawLine(x, smallTop, x, smallBottom, Colors::White);
    }

    // Draw current position marker centered in the slot for 'pos' (between boundX[pos+9] and boundX[pos+10])
    float markerX = valueToX(pos); // center of the slot (now reversed)
    m_renderer->DrawImage(GetMilitaryMarkerImage(), markerX - 15, y, 30, 30);

    // Draw two token slots per player at thresholds 3 and 6 (use valueToX to place in slot centers)
    const int thresholds[2] = { 3, 6 };
    SDL_Renderer* rdr = m_renderer->GetSDLRenderer();
    if (!rdr) return;

    const int radius = 8;
    SDL_Color fillColor{ 255, 255, 255, 255 };
    SDL_Color outlineColor{ 255, 215, 0, 255 };

    for (int i = 0; i < 2; ++i)
    {
        int threshold = thresholds[i];
        float tx = valueToX(threshold);
        int cx = int(tx);
        int cy = int(trackY);
        bool taken = pos >= threshold;
        if (taken) m_renderer->DrawCircleOutline(rdr, cx, cy, radius, outlineColor);
        else       m_renderer->DrawFilledCircle(rdr, cx, cy, radius, fillColor);
    }

    for (int i = 0; i < 2; ++i)
    {
        int threshold = thresholds[i];
        float tx = valueToX(-threshold);
        int cx = int(tx);
        int cy = int(trackY);
        bool taken = pos <= -threshold;
        if (taken) m_renderer->DrawCircleOutline(rdr, cx, cy, radius, outlineColor);
        else       m_renderer->DrawFilledCircle(rdr, cx, cy, radius, fillColor);
    }
}

// ---------------------------------------------------------------------
// Draw the currently selected node's magnified view + cost (if any)
// Prioritize a selected wonder preview: if a wonder is selected it stays
// magnified even when a node is selected on top.
void SevenWDuelRenderer::drawSelectedCard(UIState* ui)
{
    if (!ui)
        return;

    // --- Wonder preview (owner+index) ---
    int owner = ui->selectedWonderPlayer;
    int widx = ui->selectedWonderIndex;

    if (owner >= 0 && widx >= 0)
    {
        // validate owner and index
        if (owner == 0 || owner == 1)
        {
            const sevenWD::PlayerCity& city = m_state.getPlayerCity(owner);
            if (widx >= 0 && widx < city.m_unbuildWonderCount)
            {
                sevenWD::Wonders wonder = city.m_unbuildWonders[widx];
                const sevenWD::Card& wonderCard = m_state.m_context->getWonder(wonder);

                // magnified area from UIPosition
                float mx = m_uiPos.magnifiedX;
                float my = m_uiPos.magnifiedY;
                float mw = m_uiPos.magnifiedW;
                float mh = m_uiPos.magnifiedH;

                // enlarge background to leave space for optional title/cost area
                // use persistent preview scale from layout so preview stays enlarged after draft
                const float previewScale = m_layout.wonderPreviewScale;
                const float topPad = 20.0f;
                const float sidePad = 8.0f;
                const float bottomPad = 12.0f;
                // compute an expanded preview rect centered on the original magnified slot
                float pw = mw * previewScale;
                float ph = mh * previewScale;
                float previewX = mx - (pw - mw) * 0.5f;
                float previewY = my - (ph - mh) * 0.5f;
                m_renderer->DrawImage(GetBackgroundPanel(), previewX - sidePad, previewY - topPad - sidePad, pw + sidePad * 2.0f, ph + topPad + bottomPad + sidePad);

                // compute cost FOR THE OWNER of this wonder (not the current player)
                const int ownerIdx = owner;
                const sevenWD::PlayerCity& ownerCity = m_state.getPlayerCity(ownerIdx);
                const sevenWD::PlayerCity& ownerOther = m_state.getPlayerCity((ownerIdx + 1) % 2);
                u32 cost = ownerCity.computeCost(wonderCard, ownerOther);

                // draw cost text above magnified wonder
                std::string costText = std::string("Cost: ") + std::to_string(int(cost));
                // position the cost above the magnified wonder (use the magnified slot origin mx,my)
                m_renderer->DrawText(costText, mx + 8.0f, my - topPad + 8.0f, Colors::Yellow);

                // draw wonder image (use wonder-specific image loader)
                SDL_Texture* wtex = GetWonderImage(wonder);
                if (wtex)
                {
                    // Preserve wonder aspect ratio defined by layout.wonderW / wonderH
                    float aspect = (m_layout.wonderW > 0.0f && m_layout.wonderH > 0.0f)
                        ? (m_layout.wonderW / m_layout.wonderH)
                        : (m_layout.cardW / m_layout.cardH);

                    // fit into expanded preview rect (pw x ph) while preserving aspect
                    float targetW = std::min(pw, ph * aspect);
                    float targetH = targetW / aspect;
                    if (targetH > ph) // fallback if numeric issues
                    {
                        targetH = ph;
                        targetW = targetH * aspect;
                    }

                    // center inside the expanded preview rect
                    float drawX = previewX + (pw - targetW) * 0.5f;
                    float drawY = previewY + (ph - targetH) * 0.5f;

                    m_renderer->DrawImage(wtex, drawX, drawY, targetW, targetH);
                }
                else
                {
                    m_renderer->DrawText("[no wonder image]", mx + 8.0f, my + 8.0f, Colors::White);
                }

                // Done — wonder preview must stay visible even if a node is selected.
                return;
            }
        }
    }

    // --- Card preview (if no wonder preview active) ---
    if (ui->selectedNode < 0)
        return;

    u32 nodeIndex = u32(ui->selectedNode);
    if (nodeIndex >= m_state.m_graph.m_graph.size()) return;

    const auto& node = m_state.m_graph.m_graph[nodeIndex];
    if (!node.m_visible) return;

    const sevenWD::Card& card = m_state.m_context->getCard(node.m_cardId);

    // check if card already played (then nothing to show)
    bool played = false;
    for (u32 i = 0; i < m_state.m_numPlayedAgeCards; ++i)
        if (m_state.m_playedAgeCards[i] == card.getAgeId()) { played = true; break; }
    if (played) return;

    // compute cost for current player to pick that card
    u32 cur = m_state.getCurrentPlayerTurn();
    const sevenWD::PlayerCity& myCity = m_state.getPlayerCity(cur);
    const sevenWD::PlayerCity& other = m_state.getPlayerCity((cur + 1) % 2);
    u32 cost = myCity.computeCost(card, other);

    // magnified area from UIPosition
    float mx = m_uiPos.magnifiedX;
    float my = m_uiPos.magnifiedY;
    float mw = m_uiPos.magnifiedW;
    float mh = m_uiPos.magnifiedH;

    // enlarge background to leave more space for cost text above the card
    const float topPad = 36.0f;   // extra space above card for the cost text
    const float sidePad = 8.0f;
    const float bottomPad = 16.0f;
    m_renderer->DrawImage(GetBackgroundPanel(), mx - sidePad, my - topPad - sidePad, mw + sidePad * 2.0f, mh + topPad + bottomPad + sidePad);

    // draw cost text further above magnified card to avoid overlap
    std::string costText = std::string("Cost: ") + std::to_string(int(cost));
    m_renderer->DrawText(costText, mx + 8.0f, my - topPad + 8.0f, Colors::Yellow);

    // draw the card magnified (no outline/highlight)
    SDL_Texture* tex = GetCardImage(card);
    if (tex)
        m_renderer->DrawImage(tex, mx, my, mw, mh);
    else
        m_renderer->DrawText("[no image]", mx + 8.0f, my + 8.0f, Colors::White);

    // Intentionally no outline or additional highlight for the magnified card.
}

// Image loaders (updated)
SDL_Texture* SevenWDuelRenderer::GetCardImage(const sevenWD::Card& card)
{
    const char* cname = card.getName();
    if (!cname || cname[0] == '\0')
        return m_renderer->LoadImage("assets/cards/card.png");

    // Build a filesystem-safe name from the card name:
    // - keep alnum and a few safe punctuation characters
    // - convert whitespace to underscore
    std::string name(cname);
    std::string safe;
    safe.reserve(name.size());
    for (unsigned char ch : name)
    {
        if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '(' || ch == ')')
            safe.push_back(char(ch));
        else if (std::isspace(ch))
            safe.push_back('_');
        // skip other characters (apostrophes, punctuation, etc.)
    }

    std::string path = std::string("assets/cards/") + safe + ".png";
    SDL_Texture* tex = m_renderer->LoadImage(path.c_str());

    // Fallback to generic card image if specific file not found
    if (!tex)
        tex = m_renderer->LoadImage("assets/cards/card.png");

    return tex;
}

SDL_Texture* SevenWDuelRenderer::GetCardBackImage(const sevenWD::Card& card)
{
    // Prefer a guild-specific back for guild cards.
    if (card.getType() == sevenWD::CardType::Guild)
    {
        SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_guild.png");
        if (tex) return tex;
        // fallback to age/generic if guild-specific missing
    }

    // If we have a GameContext, determine which age range this card belongs to using card.getId()
    // and counts provided by GameContext. This is robust regardless of how ids were assigned.
    if (m_state.m_context)
    {
        u8 gid = card.getId(); // global id in m_allCards
        u32 n1 = m_state.m_context->getAge1CardCount();
        u32 n2 = m_state.m_context->getAge2CardCount();
        u32 n3 = m_state.m_context->getAge3CardCount();

        if (gid < n1)
        {
            SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age1.png");
            if (tex) return tex;
        }
        else if (gid < n1 + n2)
        {
            SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age2.png");
            if (tex) return tex;
        }
        else if (gid < n1 + n2 + n3)
        {
            SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age3.png");
            if (tex) return tex;
        }
        // if not in age ranges, fall through to generic
    }

    // final fallback
    SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back.png");
    return tex ? tex : nullptr;
}
SDL_Texture* SevenWDuelRenderer::GetWonderImage(sevenWD::Wonders wonder)
{
    if (!m_state.m_context)
        return m_renderer->LoadImage("assets/wonders/wonder.png");

    const sevenWD::Card& wonderCard = m_state.m_context->getWonder(wonder);
    const char* cname = wonderCard.getName();
    if (!cname || cname[0] == '\0')
        return m_renderer->LoadImage("assets/wonders/wonder.png");

    std::string safe = MakeSafeName(cname);
    std::string path = std::string("assets/wonders/") + safe + ".png";
    SDL_Texture* tex = m_renderer->LoadImage(path.c_str());

    if (!tex)
        tex = m_renderer->LoadImage("assets/wonders/wonder.png");

    return tex;
}

SDL_Texture* SevenWDuelRenderer::GetScienceTokenImage(sevenWD::ScienceToken token)
{
    if (!m_state.m_context)
        return m_renderer->LoadImage("assets/tokens/token.png");

    const sevenWD::Card& tokenCard = m_state.m_context->getScienceToken(token);
    const char* cname = tokenCard.getName();
    if (!cname || cname[0] == '\0')
        return m_renderer->LoadImage("assets/tokens/token.png");

    std::string safe = MakeSafeName(cname);
    std::string path = std::string("assets/tokens/") + safe + ".png";
    SDL_Texture* tex = m_renderer->LoadImage(path.c_str());

    if (!tex)
        tex = m_renderer->LoadImage("assets/tokens/token.png");

    return tex;
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
    switch(resource)
    {
    case sevenWD::ResourceType::Wood:
        return m_renderer->LoadImage("assets/resources/wood.png");
    case sevenWD::ResourceType::Stone:
        return m_renderer->LoadImage("assets/resources/stone.png");
    case sevenWD::ResourceType::Clay:
        return m_renderer->LoadImage("assets/resources/clay.png");
    case sevenWD::ResourceType::Glass:
        return m_renderer->LoadImage("assets/resources/glass.png");
    case sevenWD::ResourceType::Papyrus:
        return m_renderer->LoadImage("assets/resources/papyrus.png");
    default:
        return m_renderer->LoadImage("assets/resources/resource.png");
    }
}

SDL_Texture* SevenWDuelRenderer::GetChainingSymbolImage(sevenWD::ChainingSymbol symbol)
{
    // Do not provide a default/generic image for ChainingSymbol::None.
    if (symbol == sevenWD::ChainingSymbol::None)
        return nullptr;

    // Map enum -> asset name (names correspond to enum values starting at Jar)
    static const char* names[] = {
        "Jar", "Barrel", "Mask", "Bank", "Sun", "WaterDrop", "GreekPillar",
        "Moon", "Target", "Helmet", "Horseshoe", "Sword", "Tower", "Harp", "Gear",
        "Book", "Lamp"
    };

    int idx = int(symbol) - 1; // subtract 1 because array starts at Jar
    if (idx < 0 || idx >= int(sevenWD::ChainingSymbol::Count) - 1)
        return nullptr;

    std::string path = std::string("assets/chaining/") + names[idx] + ".png";
    SDL_Texture* tex = m_renderer->LoadImage(path.c_str());
    return tex;
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

SDL_Texture* SevenWDuelRenderer::GetCardBackImageForNode(bool isGuild, u32 age)
{
    // Guild-specific back preferred when flagged
    if (isGuild)
    {
        SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_guild.png");
        if (tex) return tex;
        // fallthrough to age-specific/generic if missing
    }

    // Use current age from GameState when node doesn't carry a concrete card id yet.
    // Age values: 0 => Age I, 1 => Age II, 2 => Age III
    switch (age)
    {
    case 0:
    {
        SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age1.png");
        if (tex) return tex;
        break;
    }
    case 1:
    {
        SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age2.png");
        if (tex) return tex;
        break;
    }
    case 2:
    {
        SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back_age3.png");
        if (tex) return tex;
        break;
    }
    default:
        break;
    }

    // Final generic fallback
    SDL_Texture* tex = m_renderer->LoadImage("assets/cards/card_back.png");
    return tex ? tex : nullptr;
}

SDL_Texture* SevenWDuelRenderer::GetScienceSymbolImage(sevenWD::ScienceSymbol symbol)
{
    // Map enum -> asset name (must match files in assets/science/)
    static const char* names[] = {
        "Wheel", "Script", "Triangle", "Bowl", "SolarClock", "Globe", "Law"
    };
    int idx = int(symbol);
    if (idx < 0 || idx >= int(sevenWD::ScienceSymbol::Count))
        return m_renderer->LoadImage("assets/science/symbol.png");

    std::string path = std::string("assets/science/") + names[idx] + ".png";
    SDL_Texture* tex = m_renderer->LoadImage(path.c_str());
    if (!tex)
        tex = m_renderer->LoadImage("assets/science/symbol.png");
    return tex;
}