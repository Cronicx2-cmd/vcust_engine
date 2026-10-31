#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct ModelMaterialBatch {
    unsigned int firstIndex = 0;
    unsigned int indexCount = 0;
    glm::vec3 ambient{0.06f};
    glm::vec3 diffuse{0.8f};
    glm::vec3 specular{0.18f};
    float shininess = 24.0f;
    float opacity = 1.0f;
    int materialId = -1;
};

class Model {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    int indexCount = 0;
    std::vector<ModelMaterialBatch> materialBatches;

    Model() = default;
    ~Model();

    bool load(const std::string& path);
    bool load(const std::string& path, const std::string& objectName);
};
