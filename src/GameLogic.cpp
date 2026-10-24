#include "GameLogic.hpp"
#include <iostream>

GameLogic::GameLogic(float& health) : health_(health) {}

void GameLogic::laser_func(float damage) {
    health_ -= damage;
    if (health_ <= 0.0f) {
        health_ = 100.0f;
        std::cerr << "Gracz zginął\n";
    }
}

void GameLogic::OnGameUpdate(float /*deltaTime*/) {
    // TUTAJ MOŻESZ WPISAĆ SWÓJ KOD NA INWAZJĘ Z KOSMOSU I LOGIKĘ MISJI
}
