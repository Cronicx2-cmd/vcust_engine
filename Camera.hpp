// src/Camera.hpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

class Camera {
public:
    glm::vec3 target{0.f};
    float yaw{0.f};
    float pitch{glm::radians(25.f)};
    float distance{6.f};
    float fovY{glm::radians(60.f)}, nearZ{0.1f}, farZ{200.f};

    // ── FPP ──
    bool firstPerson{false};
    float eyeHeight{1.7f};

    // TPP: orbita
    void orbit(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, glm::radians(-10.f), glm::radians(85.f));
    }
    void zoom(float delta) { distance = std::clamp(distance - delta, 2.f, 20.f); }

    glm::vec3 position() const {
        const float cp = std::cos(pitch);
        return target + distance * glm::vec3(cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw));
    }

    // Kierunki poziome
    glm::vec3 forwardFlat() const { return {-std::sin(yaw), 0.f, -std::cos(yaw)}; }
    glm::vec3 rightFlat()   const { return { std::cos(yaw), 0.f, -std::sin(yaw)}; }

    // ── FPP ──
    void setFirstPerson(const glm::vec3& feetPos, float yawRad, float pitchRad) {
        firstPerson = true;
        target = feetPos + glm::vec3(0.f, eyeHeight, 0.f);
        yaw = yawRad;
        pitch = std::clamp(pitchRad, glm::radians(-89.f), glm::radians(89.f));
        distance = 0.f;
    }

    glm::vec3 eyePosition() const { return firstPerson ? target : position(); }

    // Kierunek patrzenia 3D (yaw=0 => patrzymy w -Z)
    glm::vec3 forward3D() const {
        const float cp = std::cos(pitch);
        return glm::vec3(-cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw));
    }

    glm::mat4 viewFPP() const {
        return glm::lookAt(eyePosition(), eyePosition() + forward3D(), glm::vec3(0.f, 1.f, 0.f));
    }

    // ── view / projection ──
    glm::mat4 view() const {
        return firstPerson ? viewFPP()
                           : glm::lookAt(position(), target, glm::vec3(0.f, 1.f, 0.f));
    }

    glm::mat4 projection(float aspect) const {
        glm::mat4 p = glm::perspective(fovY, aspect, nearZ, farZ);
#ifndef USE_OPENGL
        p[1][1] *= -1.f; // Tylko dla Vulkana! (OpenGL ma Y do góry)
#endif
        return p;
    }
};