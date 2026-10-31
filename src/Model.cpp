#include "Model.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"
#include <filesystem>
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

    std::string mtlBaseDir = std::filesystem::path(path).parent_path().string();
    if (!mtlBaseDir.empty() &&
        mtlBaseDir.back() != '/' && mtlBaseDir.back() != '\\') {
        mtlBaseDir.push_back(std::filesystem::path::preferred_separator);
    }

    const bool ret = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &warn, &err,
        path.c_str(), mtlBaseDir.empty() ? nullptr : mtlBaseDir.c_str()
    );
    if (!warn.empty()) std::cout << "OBJ/MTL warning: " << warn << std::endl;
    if (!err.empty()) std::cerr << "OBJ/MTL error: " << err << std::endl;
    if (!ret) return false;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    materialBatches.clear();

    auto materialFor = [&](int materialId) {
        ModelMaterialBatch material;
        if (materialId >= 0 &&
            static_cast<std::size_t>(materialId) < materials.size()) {
            const auto& source = materials[static_cast<std::size_t>(materialId)];
            material.ambient = glm::vec3(
                source.ambient[0], source.ambient[1], source.ambient[2]);
            material.diffuse = glm::vec3(
                source.diffuse[0], source.diffuse[1], source.diffuse[2]);
            material.specular = glm::vec3(
                source.specular[0], source.specular[1], source.specular[2]);
            material.shininess = source.shininess;
            material.opacity = source.dissolve;
        }
        return material;
    };

    for (const auto& shape : shapes) {
        if (!objectName.empty() && shape.name != objectName) continue;

        std::size_t indexOffset = 0;
        for (std::size_t face = 0;
             face < shape.mesh.num_face_vertices.size(); ++face) {
            const int materialId =
                face < shape.mesh.material_ids.size()
                    ? shape.mesh.material_ids[face] : -1;

            if (materialBatches.empty() ||
                materialBatches.back().firstIndex +
                    materialBatches.back().indexCount != indices.size() ||
                (face == 0 && !materialBatches.empty())) {
                // A new batch is started below whenever the material changes.
            }

            const bool newBatch =
                materialBatches.empty() ||
                materialBatches.back()._materialId != materialId;
            if (newBatch) {
                ModelMaterialBatch batch = materialFor(materialId);
                batch.firstIndex = static_cast<unsigned int>(indices.size());
                materialBatches.push_back(batch);
                materialBatches.back()._materialId = materialId;
            }

            const unsigned int faceVertexCount =
                shape.mesh.num_face_vertices[face];
            for (unsigned int corner = 0; corner < faceVertexCount; ++corner) {
                if (indexOffset + corner >= shape.mesh.indices.size()) break;
                const auto& index = shape.mesh.indices[indexOffset + corner];
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
                ++materialBatches.back().indexCount;
            }
            indexOffset += faceVertexCount;
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
        GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
        vertices.data(), GL_STATIC_DRAW
    );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW
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
