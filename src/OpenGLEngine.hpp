#pragma once

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "external/miniaudio.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include "Scene.hpp"
#include "Minimap.hpp"
#include "Camera.hpp"
#include "Model.hpp"
#include <vector>

class Enemy;
struct GLFWwindow;
struct MenuButton {
    std::string text;
    glm::vec2 center;
    glm::vec2 halfSize;
    glm::vec4 color;
    std::string action;
    std::string texturePath; // NOWE: ścieżka do tekstury przycisku
    uint32_t textureID = 0;  // NOWE: ID tekstury w OpenGL
};

class OpenGLEngine {
public:
    OpenGLEngine(const std::string& title, uint32_t w, uint32_t h, std::filesystem::path shaderDir);
    ~OpenGLEngine();

    GLFWwindow* window() const { return window_; }
    bool shouldClose() const;

    // Funkcje publiczne
    void setSceneAssets(const Image& background, const Image& minimapTex, const Image& floorTex, const Image& wallTex);
    void setGunAssets(const std::string& objPath, const Image& texImage);
    void setEnemyAssets(const std::string& objPath);
    void drawEnemyModel(const std::string& objectName, const glm::vec3& position,
                        const Camera& camera);
    void drawMenu();
    void drawEnemyCube(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color, const Camera& camera);
    void loadMenu(const std::string& path); // <--- PRZENIESIONE TUTAJ DO SEKCJI PUBLICZNEJ

    void drawFrame(const Scene& scene, const glm::vec3& playerPos, float playerYaw,
                   const Minimap& minimap, const Camera& camera, bool isShooting,
                   const std::vector<Enemy>& enemies);  

    const std::vector<MenuButton>& getMenuButtons() const { return menuButtons_; }

    // Pasek życia gracza (modyfikowany m.in. przez GameLogic::laser_func)
    float health = 100.0f;

private:
    void setupGL();
    bool ensureSsaoTargets(int width, int height);
    void destroySsaoTargets();
    void drawSkybox(const Camera& camera, int w, int h);
    void drawScene(const Scene& scene, const Camera& camera, const glm::vec3& playerPos);
    void drawMinimap(const Scene& scene, const Minimap& minimap, const UiPush& radar);
    void renderHealthbar(int w, int h);
    void drawGun(
        const Camera& camera,
        int w,
        int h,
        bool isShooting,
        const Scene& scene,
        const glm::vec3& playerPos
    );

    Image floorLightImage_{};
    float floorAverageLuminance_{0.45f};
    float gunWorldLight_{1.0f};

    std::vector<MenuButton> menuButtons_;
    uint32_t menuBgTex_{0};

    float currentRecoil_ = 0.0f;
    float fireCooldown_ = 0.0f;
    float shotPulse_ = 0.0f;
    glm::vec3 lastHitPos_{0.f};

    uint32_t muzzleVAO_{0};
    uint32_t muzzleShader_{0};

    std::filesystem::path shaderDir_;
    GLFWwindow* window_{nullptr};

    uint32_t skyboxShader_{0}, sceneShader_{0}, uiShader_{0};
    uint32_t ssaoShader_{0}, compositeShader_{0};
    uint32_t gunShader_{0};
    uint32_t menuShader_{0};
    
    uint32_t skyboxVAO_{0}, cubeVAO_{0}, quadVAO_{0};
    uint32_t brushVAO_{0}, brushVBO_{0};

    // Screen-space ambient occlusion render targets.
    GLuint sceneFbo_{0}, sceneColorTex_{0}, sceneNormalTex_{0}, sceneDepthTex_{0};
    GLuint ssaoFbo_{0}, ssaoColorTex_{0};
    int ssaoWidth_{0}, ssaoHeight_{0};
    std::uint64_t brushMeshRevision_{0};

    // Bufory wierzchołków
    uint32_t skyboxVBO_{0}, cubeVBO_{0}, quadVBO_{0}, muzzleVBO_{0};
    uint32_t bgTex_{0}, mapTex_{0}, floorTex_{0}, wallTex_{0};
    
    Model gunModel_;
    Model enemyGruntModel_;
    Model enemyRunnerModel_;
    Model enemyTankModel_;
    uint32_t gunTexture_{0};

    ma_engine audioEngine_;
    ma_sound shotSound_;
    bool audioInitialized_ = false;
    bool shotSoundInitialized_ = false;

    glm::vec3 calculateEnvironmentLight(const glm::vec3& playerPos, const Scene& scene);
    float muzzleLightPulse_{0.0f}; // NOWE: timer dla światła błysku

    double lastFrameTime_{0.0};
};