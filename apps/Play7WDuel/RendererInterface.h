#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <iostream>

struct TextKey
{
    std::string text;
    SDL_Color color;

    bool operator==(const TextKey& other) const
    {
        return text == other.text
            && color.r == other.color.r
            && color.g == other.color.g
            && color.b == other.color.b
            && color.a == other.color.a;
    }
};

namespace std
{
    template <>
    struct hash<TextKey>
    {
        std::size_t operator()(const TextKey& k) const
        {
            std::size_t h1 = std::hash<std::string>()(k.text);
            std::size_t h2 = std::hash<uint32_t>()(k.color.r | (k.color.g << 8) | (k.color.b << 16) | (k.color.a << 24));
            return h1 ^ (h2 << 1);
        }
    };
}

class RendererInterface
{
public:
    RendererInterface(SDL_Window* window)
    {
        m_renderer = SDL_CreateRenderer(window, nullptr);
        if (!m_renderer)
        {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        }

        TTF_Init();

        m_font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
        if (!m_font) {
            std::cerr << "Failed to load font: " << SDL_GetError() << "\n";
        }
    }

    ~RendererInterface()
    {
        // Destroy all cached textures
        for (auto& pair : m_textures) {
            if (pair.second) {
                SDL_DestroyTexture(pair.second);
            }
        }
        m_textures.clear();

        // Destroy text-cache textures
        for (auto& pair : m_textCache) {
            if (pair.second) {
                SDL_DestroyTexture(pair.second);
            }
        }
        m_textCache.clear();

        // Close font
        if (m_font) {
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }

        // Destroy renderer
        if (m_renderer) {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }

        // Quit SDL_ttf AFTER destroying fonts
        TTF_Quit();
    }

    // Load a texture (caches textures)
    SDL_Texture* LoadImage(const std::string& file)
    {
        if (m_textures.count(file))
            return m_textures[file];

        SDL_Surface* surf = IMG_Load(file.c_str());
        if (!surf)
        {
            std::cerr << "IMG_Load failed: " << file << " : " << SDL_GetError() << "\n";
            return nullptr;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
        SDL_DestroySurface(surf);

        if (!tex)
        {
            std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << "\n";
            return nullptr;
        }

        m_textures[file] = tex;
        return tex;
    }

    // Drawing utilities
    void DrawImage(SDL_Texture* img, float x, float y, float w, float h)
    {
        SDL_FRect dst{ x, y, w, h };
        SDL_RenderTexture(m_renderer, img, nullptr, &dst);
    }

    void DrawLine(float x0, float y0, float x1, float y1, SDL_Color color)
    {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_RenderLine(m_renderer, x0, y0, x1, y1);
    }

    void DrawRect(float x, float y, float w, float h, SDL_Color color)
    {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_FRect r{ x, y, w, h };
        SDL_RenderRect(m_renderer, &r);
    }

    void DrawFilledCircle(SDL_Renderer* rdr, int cx, int cy, int r, SDL_Color color)
    {
        SDL_SetRenderDrawColor(rdr, color.r, color.g, color.b, color.a);
        for (int dy = -r; dy <= r; ++dy) {
            int dx = (int)std::floor(std::sqrt(float(r * r - dy * dy)));
            SDL_RenderLine(rdr, (float)cx - dx, (float)cy + dy, (float)cx + dx, (float)cy + dy);
        }
    }

    void DrawCircleOutline(SDL_Renderer* rdr, int cx, int cy, int r, SDL_Color color)
    {
        SDL_SetRenderDrawColor(rdr, color.r, color.g, color.b, color.a);
        int x = r;
        int y = 0;
        int err = 0;

        while (x >= y) {
            SDL_RenderPoint(rdr, (float)cx + x, (float)cy + y);
            SDL_RenderPoint(rdr, (float)cx + y, (float)cy + x);
            SDL_RenderPoint(rdr, (float)cx - y, (float)cy + x);
            SDL_RenderPoint(rdr, (float)cx - x, (float)cy + y);
            SDL_RenderPoint(rdr, (float)cx - x, (float)cy - y);
            SDL_RenderPoint(rdr, (float)cx - y, (float)cy - x);
            SDL_RenderPoint(rdr, (float)cx + y, (float)cy - x);
            SDL_RenderPoint(rdr, (float)cx + x, (float)cy - y);

            y += 1;
            if (err <= 0) {
                err += 2 * y + 1;
            }
            else {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
    }


    void DrawText(const std::string& str, float x, float y, SDL_Color color)
    {
        if (!m_font || str.empty())
            return;

        TextKey key{ str, color };
        SDL_Texture* texture = nullptr;

        auto it = m_textCache.find(key);
        if (it != m_textCache.end())
        {
            texture = it->second;
        }
        else
        {
            SDL_Surface* surface = TTF_RenderText_Blended(m_font, str.c_str(), 0, color);
            if (!surface)
            {
                SDL_Log("TTF_RenderUTF8_Blended failed: %s", SDL_GetError());
                return;
            }

            texture = SDL_CreateTextureFromSurface(m_renderer, surface);
            SDL_DestroySurface(surface);

            if (!texture)
            {
                SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
                return;
            }

            m_textCache[key] = texture;
        }

        float w = 0, h = 0;
        SDL_GetTextureSize(texture, &w, &h);
        SDL_FRect dst{ x, y, w, h };
        SDL_RenderTexture(m_renderer, texture, nullptr, &dst);
    }

    void ClearTextCache()
    {
        for (auto& kv : m_textCache)
            SDL_DestroyTexture(kv.second);
        m_textCache.clear();
    }


    SDL_Renderer* GetSDLRenderer() { return m_renderer; }

private:
    SDL_Renderer* m_renderer = nullptr;
    std::unordered_map<std::string, SDL_Texture*> m_textures;

    TTF_Font* m_font = nullptr;
    std::unordered_map<TextKey, SDL_Texture*> m_textCache;
};