#include "Model.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"
#include <iostream>
#include <vector>

bool Model::load(const std::string& path) {
    return load(path, {});
}

bool Model::load(const std::string& path, const std::string& objectName) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    const bool ret = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &warn, &err, path.c_str()
    );
    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if (!ret) return false;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (const auto& shape : shapes) {
        if (!objectName.empty() && shape.name != objectName) {
            continue;
        }

        for (const auto& index : shape.mesh.indices) {
            if (index.vertex_index < 0) continue;

            const std::size_t vertexOffset =
                static_cast<std::size_t>(index.vertex_index) * 3;
            if (vertexOffset + 2 >= attrib.vertices.size()) continue;

            vertices.push_back(attrib.vertices[vertexOffset + 0]);
            vertices.push_back(attrib.vertices[vertexOffset + 1]);
            vertices.push_back(attrib.vertices[vertexOffset + 2]);

            if (index.normal_index >= 0) {
                const std::size_t normalOffset =
                    static_cast<std::size_t>(index.normal_index) * 3;
                if (normalOffset + 2 < attrib.normals.size()) {
                    vertices.push_back(attrib.normals[normalOffset + 0]);
                    vertices.push_back(attrib.normals[normalOffset + 1]);
                    vertices.push_back(attrib.normals[normalOffset + 2]);
                } else {
                    vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f});
                }
            } else {
                vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f});
            }

            if (index.texcoord_index >= 0) {
                const std::size_t uvOffset =
                    static_cast<std::size_t>(index.texcoord_index) * 2;
                if (uvOffset + 1 < attrib.texcoords.size()) {
                    vertices.push_back(attrib.texcoords[uvOffset + 0]);
                    vertices.push_back(1.0f - attrib.texcoords[uvOffset + 1]);
                } else {
                    vertices.insert(vertices.end(), {0.0f, 0.0f});
                }
            } else {
                vertices.insert(vertices.end(), {0.0f, 0.0f});
            }

            indices.push_back(static_cast<unsigned int>(indices.size()));
        }
    }

    if (indices.empty()) {
        std::cerr << "OBJ object not found or empty: "
                  << (objectName.empty() ? path : objectName) << '\n';
        return false;
    }

    indexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW
    );

    const GLsizei stride = 8 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(6 * sizeof(float))
    );

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

Model::~Model() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}
