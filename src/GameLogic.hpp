#pragma once

// Izolacja logiki gry od rdzenia renderowania (OpenGLEngine).
class GameLogic {
public:
    explicit GameLogic(float& health);

    // Odejmuje punkty życia; przy śmierci resetuje health do 100.
    void laser_func(float damage);

    // Wywoływane co klatkę z głównej pętli gry.
    void OnGameUpdate(float deltaTime);

private:
    float& health_;
};
