// src/Scene.hpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

struct Aabb { glm::vec3 min{0.f}, max{0.f}; };

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