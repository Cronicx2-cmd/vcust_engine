#include "Enemy.hpp"
#include "OpenGLEngine.hpp"
#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

Enemy::Enemy(EnemyType type, const glm::vec3& startPos)
    : type_(type), position_(startPos), health_(0.0f), maxHealth_(1.0f), currentCooldown_(0.0f) {
    initStats();
    health_ = maxHealth_;
}

void Enemy::initStats() {
    switch (type_) {
        case EnemyType::Grunt:
            maxHealth_ = 50.0f;
            speed_ = 2.0f;
            attackDamage_ = 15.0f;
            attackRange_ = 1.5f;
            attackCooldown_ = 1.0f;
            break;
        case EnemyType::Runner:
            maxHealth_ = 25.0f;
            speed_ = 4.5f;
            attackDamage_ = 8.0f;
            attackRange_ = 1.2f;
            attackCooldown_ = 0.6f;
            break;
        case EnemyType::Tank:
            maxHealth_ = 150.0f;
            speed_ = 1.0f;
            attackDamage_ = 30.0f;
            attackRange_ = 2.0f;
            attackCooldown_ = 2.0f;
            break;
    }
}

void Enemy::update(const glm::vec3& playerPos, float deltaTime, float& playerHealth, const Scene& scene) {
    if (!isAlive()) return;

    // Zmniejszanie cooldownu ataku
    if (currentCooldown_ > 0.0f) currentCooldown_ -= deltaTime;

    glm::vec3 toPlayer = playerPos - position_;
    toPlayer.y = 0.0f;
    float dist = glm::length(toPlayer);

    if (dist > attackRange_) {
        if (dist > 0.1f) {
            glm::vec3 dir = glm::normalize(toPlayer);
            // Używamy scene.move(), żeby wróg nie wchodził w ściany!
            position_ = scene.move(position_, dir * speed_ * deltaTime, 0.4f);
        }
    } else if (currentCooldown_ <= 0.0f) {
        // Atak
        playerHealth -= attackDamage_;
        currentCooldown_ = attackCooldown_;
    }
}

void Enemy::draw(OpenGLEngine& engine, const Camera& camera) const {
    if (!isAlive()) return;
    
    glm::vec4 color(1.0f, 0.0f, 0.0f, 0.0f); // Czerwony
    glm::vec3 halfSize(0.5f, 0.5f, 0.5f);
    
    if (type_ == EnemyType::Runner) {
        color = glm::vec4(1.0f, 0.5f, 0.0f, 0.0f); // Pomarańczowy
        halfSize = glm::vec3(0.4f, 0.8f, 0.4f);
    } else if (type_ == EnemyType::Tank) {
        color = glm::vec4(0.8f, 0.0f, 0.8f, 0.0f); // Fioletowy
        halfSize = glm::vec3(0.8f, 0.8f, 0.8f);
    }
    
    engine.drawEnemyCube(position_ - halfSize, position_ + halfSize, color, camera);
}