#pragma once

#include <string>
#include <sstream>
#include <algorithm>
#include "RendererInterface.h"

struct Slider
{
    int minValue;
    int maxValue;
    int value;

    // visual rect
    float x;
    float y;
    float w;
    float h;

    std::string label;
    bool dragging = false;

    Slider(int minV = 0, int maxV = 100, int init = 0, const std::string& lbl = "")
        : minValue(minV), maxValue(maxV), value(init), x(0), y(0), w(200), h(20), label(lbl)
    {
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
    }

    float knobRadius() const { return std::max(6.0f, h * 0.9f * 0.5f); }

    // compute knob center x from current value
    float knobCenterX() const
    {
        if (maxValue == minValue) return x;
        float t = float(value - minValue) / float(maxValue - minValue);
        return x + t * w;
    }

    void setFromMouseX(int mx)
    {
        if (w <= 0.0f || maxValue == minValue) return;
        float t = (float(mx) - x) / w;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int newVal = minValue + int(std::round(t * float(maxValue - minValue)));
        value = std::clamp(newVal, minValue, maxValue);
    }

    bool hitKnob(int mx, int my) const
    {
        float kx = knobCenterX();
        float ky = y + h * 0.5f;
        float r = knobRadius();
        float dx = mx - kx;
        float dy = my - ky;
        return (dx * dx + dy * dy) <= (r * r);
    }

    bool hitTrack(int mx, int my) const
    {
        return mx >= int(x) && mx <= int(x + w) && my >= int(y) && my <= int(y + h);
    }

    void onMouseDown(int mx, int my)
    {
        if (hitKnob(mx, my) || hitTrack(mx, my))
        {
            dragging = true;
            setFromMouseX(mx);
        }
    }

    void onMouseUp()
    {
        dragging = false;
    }

    void onMouseMove(int mx, int my)
    {
        if (dragging)
            setFromMouseX(mx);
    }

    void draw(RendererInterface& r, int mouseX, int mouseY)
    {
        // track
        SDL_Color trackColor = SDL_Color{ 64, 64, 64, 220 };
        r.DrawRect(x, y + h * 0.5f - 4.0f, w, 8.0f, trackColor);

        // filled portion
        float kx = knobCenterX();
        SDL_Color fillColor = SDL_Color{ 200, 200, 200, 255 };
        r.DrawRect(x, y + h * 0.5f - 4.0f, kx - x, 8.0f, fillColor);

        // knob
        float kr = knobRadius();
        SDL_Renderer* rdr = r.GetSDLRenderer();
        if (rdr) {
            SDL_Color knobFill = dragging ? SDL_Color{ 255, 215, 0, 255 } : SDL_Color{ 220, 220, 220, 255 };
            r.DrawFilledCircle(rdr, int(kx), int(y + h * 0.5f), int(kr), knobFill);
            SDL_Color knobOutline = SDL_Color{ 120, 120, 120, 255 };
            r.DrawCircleOutline(rdr, int(kx), int(y + h * 0.5f), int(kr), knobOutline);
        }
        else {
            // fallback rectangle knob
            SDL_Color kcol = dragging ? SDL_Color{ 255, 215, 0, 255 } : SDL_Color{ 220, 220, 220, 255 };
            r.DrawRect(kx - kr, y + h * 0.5f - kr, kr * 2, kr * 2, kcol);
        }

        // label (left, above track)
        if (!label.empty()) {
            r.DrawText(label, x, y - 18.0f, RendererInterface::Colors::White);
        }

        // numeric value (embedded in slider UI, right of track)
        {
            std::ostringstream voss;
            voss << value;
            // vertically center text roughly on the track
            float textY = y + h * 0.5f - 8.0f;
            r.DrawText(voss.str(), x + w + 8.0f, textY, RendererInterface::Colors::White);
        }

        // optional: show tooltip-like value above knob when hovering or dragging
        if (dragging || hitKnob(mouseX, mouseY)) {
            std::ostringstream tss;
            tss << value;
            float tipX = kx - 12.0f;
            float tipY = y - 30.0f;
            // small background box for readability
            SDL_Color bg = SDL_Color{ 40, 40, 40, 220 };
            r.DrawRect(tipX - 6.0f, tipY - 4.0f, 36.0f, 20.0f, bg);
            r.DrawText(tss.str(), tipX, tipY, RendererInterface::Colors::White);
        }
    }
};