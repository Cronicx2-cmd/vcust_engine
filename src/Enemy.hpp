#pragma once
#include <glm/glm.hpp>
#include "Scene.hpp" // Wymagane dla scene.move()

// Forward declarations, żeby nie robić cyrkularnych include'ów
class OpenGLEngine;
class Camera;

enum class EnemyType {
    Grunt,
    Runner,
    Tank
};

class Enemy {
public:
    Enemy(EnemyType type, const glm::vec3& startPos);

    // Dodano scene, żeby wróg nie przenikał przez ściany
    void update(const glm::vec3& playerPos, float deltaTime, float& playerHealth, const Scene& scene);
    
    void draw(OpenGLEngine& engine, const Camera& camera) const;

    bool isAlive() const { return health_ > 0.0f; }
    glm::vec3 position() const { return position_; }
    float health() const { return health_; }

private:
    EnemyType type_;
    glm::vec3 position_;
    float health_;
    float maxHealth_;
    float speed_;
    float attackDamage_;
    float attackRange_;
    float attackCooldown_;
    float currentCooldown_;

    void initStats();
};