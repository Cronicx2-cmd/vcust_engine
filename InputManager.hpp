// src/InputManager.hpp
#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>

// Śledzi klawiaturę/mysz przez callbacki GLFW. Używa user pointera okna,
// więc InputManager jest jedynym właścicielem glfwSetWindowUserPointer.
class InputManager {
public:
    explicit InputManager(GLFWwindow* window) : window_(window) {
        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, onKey);
        glfwSetCursorPosCallback(window_, onCursor);
        glfwSetScrollCallback(window_, onScroll);
        setCursorCaptured(true);
    }
    ~InputManager() {
        glfwSetKeyCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetWindowUserPointer(window_, nullptr);
    }
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    bool keyDown(int key) const { return keys_[std::size_t(key)]; }
    bool keyPressed(int key) const { return pressed_[std::size_t(key)]; } // zbocze narastające w tej klatce
    glm::vec2 consumeMouseDelta() { auto d = delta_; delta_ = {}; return d; }
    float consumeScroll() { float s = scroll_; scroll_ = 0.f; return s; }
    bool captured() const { return captured_; }

    void setCursorCaptured(bool on) {
        captured_ = on;
        hasLast_ = false; // unikamy skoku delty po zmianie trybu kursora
        glfwSetInputMode(window_, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, on ? GLFW_TRUE : GLFW_FALSE);
    }
    void endFrame() { pressed_.fill(false); } // wołać raz na klatkę, po odczycie wejścia

private:
    static InputManager* self(GLFWwindow* w) { return static_cast<InputManager*>(glfwGetWindowUserPointer(w)); }
    static void onKey(GLFWwindow* w, int key, int, int action, int) {
        if (key < 0 || key > GLFW_KEY_LAST) return;
        auto* s = self(w);
        if (action == GLFW_PRESS)        { s->keys_[std::size_t(key)] = true; s->pressed_[std::size_t(key)] = true; }
        else if (action == GLFW_RELEASE) { s->keys_[std::size_t(key)] = false; }
    }
    static void onCursor(GLFWwindow* w, double x, double y) {
        auto* s = self(w);
        const glm::vec2 p{float(x), float(y)};
        if (s->hasLast_ && s->captured_) s->delta_ += p - s->last_;
        s->last_ = p; s->hasLast_ = true;
    }
    static void onScroll(GLFWwindow* w, double, double dy) { self(w)->scroll_ += float(dy); }

    GLFWwindow* window_;
    std::array<bool, GLFW_KEY_LAST + 1> keys_{}, pressed_{};
    glm::vec2 last_{0.f}, delta_{0.f};
    float scroll_{0.f};
    bool hasLast_{false}, captured_{false};
};