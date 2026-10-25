// src/Scene.hpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Aabb { glm::vec3 min{0.f}, max{0.f}; };

enum class BrushMaterial : std::uint8_t {
    Wall = 0,       // wall_tex, optionally grayscale according to the current shader
    Floor = 1,      // floor_tex
    Metal = 2,      // wall_tex with a cool metallic tint
    Panel = 3       // wall_tex with a light panel tint
};

struct BrushPlane {
    glm::vec3 normal{0.f, 1.f, 0.f}; // outward normal; solid brush is n·x <= distance
    float distance{0.f};
    BrushMaterial material{BrushMaterial::Wall};
};

struct ConvexBrush {
    std::vector<BrushPlane> planes;
    glm::vec3 min{0.f};
    glm::vec3 max{0.f};
};

// A brush-space BSP node. Brushes crossing the split plane stay in this node;
// completely front/back brushes are stored in child subtrees.
struct BrushBspNode {
    BrushPlane split;
    std::vector<std::size_t> brushIndices;
    int front{-1};
    int back{-1};
};

// CPU-side triangle vertex stream produced from brush side planes.
struct SceneVertex {
    glm::vec3 position{0.f};
    glm::vec3 normal{0.f, 1.f, 0.f};
    glm::vec2 uv{0.f};
    float material{0.f};
};

struct Scene {
    std::string id;
    std::filesystem::path background;
    std::filesystem::path floorTexture;
    std::filesystem::path wallTexture;
    glm::ivec2 renderSize{1920, 1080};
    glm::vec3 camPos{0.f, 3.f, 8.f}, camLook{0.f, 1.f, 0.f};
    float fovH{50.f};
    glm::vec3 playerStart{0.f};
    Aabb bounds{{-8.f, -1.f, -6.f}, {8.f, 3.f, 6.f}};
    
    std::vector<Aabb> walls;
    std::vector<Aabb> floors; // NOWE: Wielokrotne podłogi

    // New convex brush world geometry. Legacy AABB maps remain supported.
    std::vector<ConvexBrush> brushes;
    std::vector<SceneVertex> brushVertices;
    std::vector<BrushBspNode> brushBsp;
    int brushBspRoot{-1};
    bool hasBrushFloorSurface{false};
    std::uint64_t brushMeshRevision{0};

    float aspect() const { return float(renderSize.x) / float(renderSize.y); }

    glm::mat4 view() const { return glm::lookAt(camPos, camLook, glm::vec3(0.f, 1.f, 0.f)); }
    glm::mat4 projection() const {
        const float vfov = 2.f * std::atan(std::tan(glm::radians(fovH) * 0.5f) / aspect());
        glm::mat4 p = glm::perspective(vfov, aspect(), 0.1f, 100.f);
        p[1][1] *= -1.f;
        return p;
    }
    glm::vec3 forwardFlat() const { glm::vec3 f = camLook - camPos; f.y = 0.f; return glm::normalize(f); }
    glm::vec3 rightFlat() const { const glm::vec3 f = forwardFlat(); return {-f.z, 0.f, f.x}; }
    float cameraYaw() const { const glm::vec3 f = forwardFlat(); return std::atan2(-f.x, -f.z); }

    bool blocked(const glm::vec3& p, float r, float height = 1.8f) const {
        if (p.x - r < bounds.min.x || p.x + r > bounds.max.x ||
            p.z - r < bounds.min.z || p.z + r > bounds.max.z) return true;
        auto blocksBrush = [&](const ConvexBrush& brush) {
            // Cheap broad phase before testing the brush half-spaces.
            if (p.x + r < brush.min.x || p.x - r > brush.max.x ||
                p.y + height < brush.min.y || p.y > brush.max.y ||
                p.z + r < brush.min.z || p.z - r > brush.max.z) return false;

            for (const BrushPlane& plane : brush.planes) {
                // Minimum projection of a vertical player cylinder (radius r, height h).
                const float horizontalRadius = r * std::sqrt(
                    plane.normal.x * plane.normal.x + plane.normal.z * plane.normal.z);
                float minProjection = glm::dot(plane.normal, p) - horizontalRadius;
                if (plane.normal.y < 0.f) minProjection += plane.normal.y * height;
                // Touching a surface is allowed; only actual penetration blocks movement.
                if (minProjection >= plane.distance - 1e-4f) return false;
            }
            return true;
        };

        if (brushBspRoot >= 0 && !brushBsp.empty()) {
            std::vector<int> pendingNodes{brushBspRoot};
            while (!pendingNodes.empty()) {
                const int nodeIndex = pendingNodes.back();
                pendingNodes.pop_back();
                if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= brushBsp.size()) continue;
                const BrushBspNode& node = brushBsp[static_cast<std::size_t>(nodeIndex)];
                for (const std::size_t brushIndex : node.brushIndices) {
                    if (brushIndex < brushes.size() && blocksBrush(brushes[brushIndex])) return true;
                }

                const glm::vec3 center = p + glm::vec3(0.f, height * 0.5f, 0.f);
                const float signedDistance = glm::dot(node.split.normal, center) - node.split.distance;
                const float support = r * std::sqrt(
                    node.split.normal.x * node.split.normal.x + node.split.normal.z * node.split.normal.z) +
                    std::abs(node.split.normal.y) * height * 0.5f;
                if (signedDistance - support > 0.f) {
                    if (node.front >= 0) pendingNodes.push_back(node.front);
                } else if (signedDistance + support < 0.f) {
                    if (node.back >= 0) pendingNodes.push_back(node.back);
                } else {
                    if (node.front >= 0) pendingNodes.push_back(node.front);
                    if (node.back >= 0) pendingNodes.push_back(node.back);
                }
            }
        } else {
            for (const ConvexBrush& brush : brushes) {
                if (blocksBrush(brush)) return true;
            }
        }

        for (const Aabb& w : walls) {
            if (p.y + height < w.min.y || p.y > w.max.y) continue;
            const float dx = p.x - std::clamp(p.x, w.min.x, w.max.x);
            const float dz = p.z - std::clamp(p.z, w.min.z, w.max.z);
            if (dx * dx + dz * dz < r * r) return true;
        }
        return false;
    }

    glm::vec3 move(glm::vec3 p, const glm::vec3& d, float r) const {
        glm::vec3 t = p; t.x += d.x; if (!blocked(t, r)) p = t;
        t = p;           t.z += d.z; if (!blocked(t, r)) p = t;
        return p;
    }

    float floorHeight() const { return bounds.min.y; }
};
