#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Model {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    int indexCount = 0;

    Model() = default;
    ~Model();

    bool load(const std::string& path);
};