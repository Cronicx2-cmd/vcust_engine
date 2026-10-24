// src/main.cpp

#define GLFW_INCLUDE_NONE

#include "Minimap.hpp"
#include "InputManager.hpp"
#include "Camera.hpp"
#include "OpenGLEngine.hpp"
#include "GameLogic.hpp"
#include "Enemy.hpp"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <vector>

int main(int argc, char** argv) {
    try {
        const auto exeDir = std::filesystem::absolute(argv[0]).parent_path();
        OpenGLEngine engine("VcustEngine", 1280, 720, exeDir / "shaders");
        
        InputManager input(engine.window());
        input.setCursorCaptured(false);

        SceneManager scenes(exeDir);
        if (scenes.maps().empty()) throw std::runtime_error("Brak map w content/ i mods/");
        Minimap minimap;
        Scene scene;
        glm::vec3 player{0.f};
        float playerYaw = 0.f;

        engine.loadMenu((exeDir / "content" / "ui" / "menu.txt").string());

        auto applyScene = [&](const std::string& id, bool resetPlayer) {
            Scene s = scenes.load(id);
            const Image bg = loadImageRGBA(s.background);
            s.renderSize = {bg.w, bg.h};
            const Image map = minimap.bake(s);
            
            Image floorTex, wallTex;
            if (!s.floorTexture.empty() && std::filesystem::exists(s.floorTexture)) floorTex = loadImageRGBA(s.floorTexture);
            if (!s.wallTexture.empty()  && std::filesystem::exists(s.wallTexture))  wallTex = loadImageRGBA(s.wallTexture);
            
            engine.setSceneAssets(bg, map, floorTex, wallTex);
            
            const auto gunObjPath = exeDir / "content" / "models" / "gun" / "model.obj";
            const auto gunTexPath = exeDir / "content" / "models" / "gun" / "diffuse.jpg";
            
            if (std::filesystem::exists(gunObjPath) && std::filesystem::exists(gunTexPath)) {
                const Image gunTex = loadImageRGBA(gunTexPath);
                engine.setGunAssets(gunObjPath.string(), gunTex);
            }
            
            scene = std::move(s);
            if (resetPlayer) player = scene.playerStart;
        };
        
        applyScene(argc > 1 ? argv[1] : scenes.maps().begin()->first, true);

        constexpr float kWalk = 3.f, kSprint = 6.f, kRadius = 0.3f;
        double prev = glfwGetTime(), reloadAt = 0.0;

        Camera camera;
        camera.fovY = glm::radians(70.f);
        camera.nearZ = 0.05f;

        float camYaw = 0.f;
        float camPitch = 0.f;

        float playerY = scene.playerStart.y;
        float velocityY = 0.f;
        bool isGrounded = true;
        constexpr float kGravity = -20.f;
        constexpr float kJumpForce = 6.f;

        bool inMenu = true; 
        bool mouseWasPressed = false;
        GameLogic gameLogic(engine.health);

        std::vector<Enemy> enemies;
        enemies.emplace_back(EnemyType::Grunt, glm::vec3(5.0f, 0.5f, 5.0f));
        enemies.emplace_back(EnemyType::Runner, glm::vec3(-5.0f, 0.5f, -5.0f));
        enemies.emplace_back(EnemyType::Tank, glm::vec3(8.0f, 0.5f, -8.0f));

        while (!engine.shouldClose()) {
            const double now = glfwGetTime();
            const float dt = std::min(float(now - prev), 0.05f);
            prev = now;

            glfwPollEvents();

            if (inMenu) {
                bool mouseIsPressed = (glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
                if (mouseIsPressed && !mouseWasPressed) {
                    double mx, my;
                    glfwGetCursorPos(engine.window(), &mx, &my);
                    int w, h;
                    glfwGetWindowSize(engine.window(), &w, &h);
                    
                    if (w > 0 && h > 0) {
                        float ndcX = (2.0f * float(mx)) / float(w) - 1.0f;
                        float ndcY = 1.0f - (2.0f * float(my)) / float(h);
                        const float aspect = float(w) / float(h);
                        const float xScale = (16.0f / 9.0f) / aspect;

                        for (const auto& btn : engine.getMenuButtons()) {
                            const float cx = btn.center.x * xScale;
                            const float hx = btn.halfSize.x * xScale;
                            if (ndcX >= (cx - hx) && ndcX <= (cx + hx) &&
                                ndcY >= (btn.center.y - btn.halfSize.y) && ndcY <= (btn.center.y + btn.halfSize.y)) {
                                
                                if (btn.action == "new_game") {
                                    inMenu = false;
                                    input.setCursorCaptured(true);
                                } else if (btn.action == "quit") {
                                    glfwSetWindowShouldClose(engine.window(), GLFW_TRUE);
                                }
                            }
                        }
                    }
                }
                mouseWasPressed = mouseIsPressed;
            } 
            else {
                gameLogic.OnGameUpdate(dt);
                if (input.keyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(engine.window(), GLFW_TRUE);
                if (input.keyPressed(GLFW_KEY_TAB)) input.setCursorCaptured(!input.captured());

                if (now >= reloadAt) {
                    reloadAt = now + 0.5;
                    if (scenes.changed()) {
                        try { applyScene(scene.id, false); }
                        catch (const std::exception& e) { std::cerr << "Hot-reload: " << e.what() << '\n'; }
                    }
                }

                if (input.captured()) {
                    const glm::vec2 md = input.consumeMouseDelta();
                    if (md.x != 0.f || md.y != 0.f) {
                        camYaw   -= md.x * 0.003f;
                        camPitch -= md.y * 0.003f;
                        camPitch  = glm::clamp(camPitch, glm::radians(-89.f), glm::radians(89.f));
                    }
                } else {
                    input.consumeMouseDelta();
                }

                glm::vec3 dir(0.f);
                const glm::vec3 fwd   = glm::vec3(-std::sin(camYaw), 0.f, -std::cos(camYaw));
                const glm::vec3 right = glm::vec3( std::cos(camYaw), 0.f, -std::sin(camYaw));
                if (input.keyDown(GLFW_KEY_W)) dir += fwd;
                if (input.keyDown(GLFW_KEY_S)) dir -= fwd;
                if (input.keyDown(GLFW_KEY_D)) dir += right;
                if (input.keyDown(GLFW_KEY_A)) dir -= right;

                if (glm::dot(dir, dir) > 0.f) {
                    dir = glm::normalize(dir);
                    const float speed = input.keyDown(GLFW_KEY_LEFT_SHIFT) ? kSprint : kWalk;
                    
                    glm::vec3 newPos = scene.move(player, dir * speed * dt, kRadius);
                    player.x = newPos.x;
                    player.z = newPos.z;
                    playerYaw = std::atan2(-dir.x, -dir.z);
                }

                if (isGrounded && input.keyDown(GLFW_KEY_SPACE)) {
                    velocityY = kJumpForce;
                    isGrounded = false;
                }

                velocityY += kGravity * dt;
                playerY += velocityY * dt;

                const float groundY = scene.floorHeight();
                if (playerY <= groundY) {
                    playerY = groundY;
                    velocityY = 0.f;
                    isGrounded = true;
                } else {
                    isGrounded = false;
                }

                player.y = playerY;
                camera.setFirstPerson(player, camYaw, camPitch);

                // Aktualizacja wrogów (przekazujemy scene!)
                for (auto& enemy : enemies) {
                    enemy.update(player, dt, engine.health, scene);
                }

                input.endFrame();
            }

            // ── RYSOWANIE ──
            int w, h;
            glfwGetFramebufferSize(engine.window(), &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (inMenu) {
                engine.drawMenu(); 
            } else {
                bool isShooting = input.captured() && glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                
                // Przekazujemy listę wrogów do silnika (usunęliśmy ręczne rysowanie)
                engine.drawFrame(scene, player, playerYaw, minimap, camera, isShooting, enemies);
            }

            glfwSwapBuffers(engine.window());
        }
    } catch (const std::exception& e) {
        std::cerr << "Błąd krytyczny: " << e.what() << '\n';
        return 1;
    }
    return 0;
}