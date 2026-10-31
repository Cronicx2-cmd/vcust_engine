#pragma once
#include "Scene.hpp"
#include "SceneManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

struct UiPush {
    glm::vec2 center{0.f}, halfSize{0.f};
    glm::vec2 playerXZ{0.f};
    float mapCos{1.f}, mapSin{0.f};
    float worldRadius{6.f};
    float mode{0.f};
    float arrowCos{1.f}, arrowSin{0.f};
    glm::vec2 mapOrigin{0.f};
    float mapWorldSize{1.f};
};
static_assert(sizeof(UiPush) == 60);

class Minimap {
public:
    float diameterPx{200.f}, marginPx{24.f}, worldRadius{6.f};

    Image bake(const Scene& s, int N = 256) {
        struct Rgba { uint8_t r, g, b, a; };
        const glm::vec2 lo{s.bounds.min.x, s.bounds.min.z}, hi{s.bounds.max.x, s.bounds.max.z};
        worldSize_ = std::max(hi.x - lo.x, hi.y - lo.y) + 2.f;
        origin_ = (lo + hi) * 0.5f - glm::vec2(worldSize_ * 0.5f);

        Image img{N, N, std::vector<uint8_t>(std::size_t(N) * std::size_t(N) * 4, 0)};
        auto put = [&](int x, int y, Rgba c) {
            if (x >= 0 && y >= 0 && x < N && y < N)
                std::memcpy(&img.rgba[(std::size_t(y) * std::size_t(N) + std::size_t(x)) * 4], &c, 4);
        };
        auto toPx = [&](float w, float o) { return int(std::floor((w - o) / worldSize_ * float(N))); };

        const float line = 1.5f * worldSize_ / float(N);
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            const glm::vec2 w = origin_ + (glm::vec2(float(x), float(y)) + 0.5f) / float(N) * worldSize_;
            if (w.x < lo.x || w.x > hi.x || w.y < lo.y || w.y > hi.y) continue;
            const glm::vec2 f = glm::fract(w);
            put(x, y, (f.x < line || f.y < line) ? Rgba{70, 110, 70, 255} : Rgba{40, 70, 40, 255});
        }   
        
        // Draw wall brushes as separate outlined rectangles.
        for (const ConvexBrush& b : s.brushes) {
            bool hasWallSurface = false;

            for (const BrushPlane& plane : b.planes) {
                if (plane.material != BrushMaterial::Floor) {
                    hasWallSurface = true;
                    break;
                }
            }

            if (!hasWallSurface) continue;

            const int x0 = toPx(b.min.x, origin_.x);
            const int x1 = toPx(b.max.x, origin_.x);
            const int y0 = toPx(b.min.z, origin_.y);
            const int y1 = toPx(b.max.z, origin_.y);

            const int left   = std::max(0, std::min(x0, x1));
            const int right  = std::min(N - 1, std::max(x0, x1));
            const int top    = std::max(0, std::min(y0, y1));
            const int bottom = std::min(N - 1, std::max(y0, y1));

            if (left > right || top > bottom) continue;

            const Rgba wallColor{230, 90, 60, 255};

            for (int x = left; x <= right; ++x) {
                put(x, top, wallColor);
                put(x, bottom, wallColor);
            }

            for (int y = top; y <= bottom; ++y) {
                put(left, y, wallColor);
                put(right, y, wallColor);
            }
        }
        return img;
    }

    // Stara wersja (używa scene.cameraYaw — dla kompatybilności)
    UiPush build(const Scene& s, const glm::vec3& playerPos, float playerYaw,
                 uint32_t fbW, uint32_t fbH) const {
        return build(s, playerPos, playerYaw, s.cameraYaw(), fbW, fbH);
    }

    // Nowa wersja z jawnym cameraYaw (FPP)
    UiPush build(const Scene& s, const glm::vec3& playerPos, float playerYaw,
                float cameraYaw, uint32_t fbW, uint32_t fbH) const {
        (void)s;
        const float w = float(fbW), h = float(fbH);
        UiPush p;
        p.halfSize = {diameterPx / w, diameterPx / h};
        p.center = {1.f - 2.f * marginPx / w - p.halfSize.x,
                    1.f - 2.f * marginPx / h - p.halfSize.y};
        p.playerXZ = {playerPos.x, playerPos.z};
        p.mapCos = std::cos(cameraYaw);  p.mapSin = std::sin(cameraYaw);
        p.worldRadius = worldRadius;
        const float d = playerYaw - cameraYaw;
        p.arrowCos = std::cos(d);     p.arrowSin = std::sin(d);
        p.mapOrigin = origin_;        p.mapWorldSize = worldSize_;
        return p;
    }

private:
    glm::vec2 origin_{0.f};
    float worldSize_{1.f};
};