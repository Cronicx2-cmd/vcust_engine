// src/SceneManager.cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "SceneManager.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <sstream>
#include <utility>
#include <stdexcept>

namespace fs = std::filesystem;

// Odczyt przez ifstream + stbi_load_from_memory: działa też ze ścieżkami nie-ASCII na Windows
Image loadImageRGBA(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("Nie można otworzyć obrazu: " + p.string());
    const std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    int w = 0, h = 0, n = 0;
    uint8_t* px = stbi_load_from_memory(raw.data(), int(raw.size()), &w, &h, &n, 4);
    if (!px) throw std::runtime_error("stb_image nie zdekodował: " + p.string());
    Image img{w, h, std::vector<uint8_t>(px, px + std::size_t(w) * std::size_t(h) * 4)};
    stbi_image_free(px);
    return img;
}

SceneManager::SceneManager(fs::path baseDir) : baseDir_(std::move(baseDir)) { scan(); }

void SceneManager::scan() {
    maps_.clear();
    std::vector<fs::path> roots{baseDir_ / "content"};
    if (fs::is_directory(baseDir_ / "mods")) {
        std::vector<fs::path> mods;
        for (const auto& e : fs::directory_iterator(baseDir_ / "mods"))
            if (e.is_directory()) mods.push_back(e.path());
        std::sort(mods.begin(), mods.end()); // deterministyczna kolejność ładowania
        roots.insert(roots.end(), mods.begin(), mods.end());
    }
    for (const fs::path& root : roots) {
        const fs::path dir = root / "maps";
        if (!fs::is_directory(dir)) continue;
        for (const auto& e : fs::directory_iterator(dir))
            if (e.is_directory() && fs::exists(e.path() / "map.txt"))
                maps_[e.path().filename().string()] = e.path() / "map.txt"; // późniejszy root nadpisuje
    }
}

namespace {
struct PendingBrushSide {
    BrushMaterial material{BrushMaterial::Wall};
    std::array<glm::vec3, 3> points{};
    int line{0};
};

BrushMaterial parseBrushMaterial(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name == "wall" || name == "concrete" || name == "default") return BrushMaterial::Wall;
    if (name == "floor" || name == "ground") return BrushMaterial::Floor;
    if (name == "metal" || name == "metallic") return BrushMaterial::Metal;
    if (name == "panel" || name == "white") return BrushMaterial::Panel;
    throw std::invalid_argument("nieznany materiał; użyj wall, concrete, floor, metal albo panel");
}

bool intersectPlanes(const BrushPlane& a, const BrushPlane& b, const BrushPlane& c, glm::vec3& point) {
    const glm::vec3 bc = glm::cross(b.normal, c.normal);
    const float determinant = glm::dot(a.normal, bc);
    if (std::abs(determinant) < 1e-5f) return false;
    point = (a.distance * bc +
             b.distance * glm::cross(c.normal, a.normal) +
             c.distance * glm::cross(a.normal, b.normal)) / determinant;
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

void addUniquePoint(std::vector<glm::vec3>& points, const glm::vec3& point) {
    for (const glm::vec3& existing : points) {
        const glm::vec3 delta = existing - point;
        if (glm::dot(delta, delta) < 1e-6f) return;
    }
    points.push_back(point);
}

void appendBrushGeometry(const std::vector<PendingBrushSide>& sides, Scene& scene) {
    constexpr float insideEpsilon = 1e-3f;
    if (sides.size() < 4) throw std::runtime_error("brush musi mieć co najmniej 4 dyrektywy side");
    if (sides.size() > 128) throw std::runtime_error("brush przekracza limit 128 ścian");

    // A point averaged from the defining points gives us a reference point inside
    // ordinary convex brushes and lets us normalize side winding automatically.
    glm::vec3 reference(0.f);
    std::size_t pointCount = 0;
    for (const PendingBrushSide& side : sides) {
        for (const glm::vec3& point : side.points) {
            reference += point;
            ++pointCount;
        }
    }
    reference /= static_cast<float>(pointCount);

    ConvexBrush brush;
    brush.planes.reserve(sides.size());
    for (const PendingBrushSide& side : sides) {
        const glm::vec3 rawNormal = glm::cross(side.points[1] - side.points[0],
                                               side.points[2] - side.points[0]);
        const float normalLength = glm::length(rawNormal);
        if (normalLength < 1e-5f) throw std::runtime_error("side ma trzy współliniowe albo identyczne punkty");
        glm::vec3 normal = rawNormal / normalLength;
        float distance = glm::dot(normal, side.points[0]);
        if (glm::dot(normal, reference) > distance) {
            normal = -normal;
            distance = -distance;
        }
        brush.planes.push_back({normal, distance, side.material});
    }

    std::vector<std::vector<glm::vec3>> facePoints(brush.planes.size());
    // Every corner of a convex polyhedron is the intersection of three planes.
    for (std::size_t i = 0; i < brush.planes.size(); ++i) {
        for (std::size_t j = i + 1; j < brush.planes.size(); ++j) {
            for (std::size_t k = j + 1; k < brush.planes.size(); ++k) {
                glm::vec3 p;
                if (!intersectPlanes(brush.planes[i], brush.planes[j], brush.planes[k], p)) continue;
                bool inside = true;
                for (const BrushPlane& plane : brush.planes) {
                    if (glm::dot(plane.normal, p) > plane.distance + insideEpsilon) {
                        inside = false;
                        break;
                    }
                }
                if (!inside) continue;
                for (std::size_t f = 0; f < brush.planes.size(); ++f) {
                    const BrushPlane& plane = brush.planes[f];
                    if (std::abs(glm::dot(plane.normal, p) - plane.distance) <= 2e-3f)
                        addUniquePoint(facePoints[f], p);
                }
            }
        }
    }

    const float hi = std::numeric_limits<float>::max();
    brush.min = glm::vec3(hi);
    brush.max = glm::vec3(-hi);
    std::vector<SceneVertex> generated;

    for (std::size_t face = 0; face < brush.planes.size(); ++face) {
        auto& polygon = facePoints[face];
        if (polygon.size() < 3) throw std::runtime_error("side nie tworzy powierzchni zamkniętego, wypukłego brusha");
        const BrushPlane& plane = brush.planes[face];
        if (plane.material == BrushMaterial::Floor && plane.normal.y > 0.7f) {
            scene.hasBrushFloorSurface = true;
        }
        glm::vec3 center(0.f);
        for (const glm::vec3& p : polygon) center += p;
        center /= static_cast<float>(polygon.size());

        const glm::vec3 referenceAxis = std::abs(plane.normal.y) < 0.9f
            ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);
        const glm::vec3 u = glm::normalize(glm::cross(referenceAxis, plane.normal));
        const glm::vec3 v = glm::cross(plane.normal, u);
        std::sort(polygon.begin(), polygon.end(), [&](const glm::vec3& a, const glm::vec3& b) {
            const glm::vec3 da = a - center;
            const glm::vec3 db = b - center;
            return std::atan2(glm::dot(da, v), glm::dot(da, u)) <
                   std::atan2(glm::dot(db, v), glm::dot(db, u));
        });

        for (const glm::vec3& p : polygon) {
            brush.min = glm::min(brush.min, p);
            brush.max = glm::max(brush.max, p);
        }
        const float material = static_cast<float>(static_cast<std::uint8_t>(plane.material));
        auto emit = [&](const glm::vec3& p) {
            const glm::vec2 uv(glm::dot(p, u) * 0.25f, glm::dot(p, v) * 0.25f);
            generated.push_back({p, plane.normal, uv, material});
        };
        // Triangle fan, with winding aligned to the outward-facing plane normal.
        for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
            emit(polygon[0]);
            emit(polygon[i]);
            emit(polygon[i + 1]);
        }
    }

    if (generated.empty()) throw std::runtime_error("brush nie wygenerował żadnych trójkątów");
    scene.brushes.push_back(std::move(brush));
    scene.brushVertices.insert(scene.brushVertices.end(), generated.begin(), generated.end());
}

void appendBrushBox(BrushMaterial material, const glm::vec3& endpointA,
                    const glm::vec3& endpointB, Scene& scene) {
    const glm::vec3 mn = glm::min(endpointA, endpointB);
    const glm::vec3 mx = glm::max(endpointA, endpointB);
    if (mx.x - mn.x < 1e-4f || mx.y - mn.y < 1e-4f || mx.z - mn.z < 1e-4f)
        throw std::runtime_error("brush_box musi mieć niezerową szerokość, wysokość i głębokość");

    auto point = [](float x, float y, float z) { return glm::vec3(x, y, z); };
    std::vector<PendingBrushSide> sides;
    sides.reserve(6);
    auto addSide = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        PendingBrushSide side;
        side.material = material;
        side.points = {a, b, c};
        sides.push_back(side);
    };
    addSide(point(mn.x, mn.y, mn.z), point(mn.x, mn.y, mx.z), point(mn.x, mx.y, mx.z)); // -X
    addSide(point(mx.x, mn.y, mx.z), point(mx.x, mn.y, mn.z), point(mx.x, mx.y, mn.z)); // +X
    addSide(point(mn.x, mn.y, mn.z), point(mx.x, mn.y, mn.z), point(mx.x, mn.y, mx.z)); // -Y
    addSide(point(mn.x, mx.y, mx.z), point(mx.x, mx.y, mx.z), point(mx.x, mx.y, mn.z)); // +Y
    addSide(point(mx.x, mn.y, mn.z), point(mn.x, mn.y, mn.z), point(mn.x, mx.y, mn.z)); // -Z
    addSide(point(mn.x, mn.y, mx.z), point(mx.x, mn.y, mx.z), point(mx.x, mx.y, mx.z)); // +Z
    appendBrushGeometry(sides, scene);
}

int buildBrushBspNode(Scene& scene, const std::vector<std::size_t>& indices, int depth) {
    if (indices.empty()) return -1;
    const std::size_t nodeIndex = scene.brushBsp.size();
    BrushBspNode node;
    node.split = scene.brushes[indices.front()].planes.front();
    scene.brushBsp.push_back(std::move(node));

    std::vector<std::size_t> front;
    std::vector<std::size_t> back;
    front.reserve(indices.size());
    back.reserve(indices.size());

    const BrushPlane splitter = scene.brushBsp[nodeIndex].split;
    for (const std::size_t index : indices) {
        const ConvexBrush& brush = scene.brushes[index];
        const glm::vec3 center = (brush.min + brush.max) * 0.5f;
        const glm::vec3 extent = (brush.max - brush.min) * 0.5f;
        const float centerProjection = glm::dot(splitter.normal, center);
        const float radius = std::abs(splitter.normal.x) * extent.x +
                             std::abs(splitter.normal.y) * extent.y +
                             std::abs(splitter.normal.z) * extent.z;
        const float minimum = centerProjection - radius;
        const float maximum = centerProjection + radius;

        if (minimum > splitter.distance + 1e-3f) front.push_back(index);
        else if (maximum < splitter.distance - 1e-3f) back.push_back(index);
        else scene.brushBsp[nodeIndex].brushIndices.push_back(index);
    }

    // Guard against degenerate partitions and cap recursion for very large maps.
    if (depth >= 32 || front.size() == indices.size() || back.size() == indices.size() ||
        (front.empty() && back.empty())) {
        scene.brushBsp[nodeIndex].brushIndices = indices;
        return static_cast<int>(nodeIndex);
    }
    scene.brushBsp[nodeIndex].front = buildBrushBspNode(scene, front, depth + 1);
    scene.brushBsp[nodeIndex].back = buildBrushBspNode(scene, back, depth + 1);
    return static_cast<int>(nodeIndex);
}

void buildBrushBsp(Scene& scene) {
    scene.brushBsp.clear();
    scene.brushBspRoot = -1;
    if (scene.brushes.empty()) return;
    std::vector<std::size_t> indices(scene.brushes.size());
    std::iota(indices.begin(), indices.end(), std::size_t{0});
    scene.brushBspRoot = buildBrushBspNode(scene, indices, 0);
}

std::uint64_t nextBrushMeshRevision() {
    static std::uint64_t revision = 1;
    return revision++;
}
} // namespace

Scene SceneManager::load(const std::string& id) {
    const auto it = maps_.find(id);
    if (it == maps_.end()) throw std::runtime_error("Nieznana mapa: " + id);
    const fs::path file = it->second;
    std::ifstream in(file);
    if (!in) throw std::runtime_error("Nie można otworzyć: " + file.string());
    activePath_ = file;                                  // stamp przed parsowaniem: zły plik nie zapętla hot-reloadu
    activeStamp_ = fs::last_write_time(file);

    Scene s;
    s.id = id;
    bool blender = false;
    auto conv = [&](glm::vec3 v) { return blender ? glm::vec3(v.x, v.z, -v.y) : v; };

    std::string line;
    int no = 0;
    bool inBrush = false;
    int brushStartLine = 0;
    std::vector<PendingBrushSide> pendingBrush;
    while (std::getline(in, line)) {
        ++no;
        if (const auto h = line.find('#'); h != std::string::npos) line.erase(h);
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key)) continue;
        auto fail = [&](const char* msg) {
            throw std::runtime_error(file.string() + ":" + std::to_string(no) + ": " + msg);
        };
        auto vec3 = [&] {
            glm::vec3 v;
            if (!(ss >> v.x >> v.y >> v.z)) fail("oczekiwano 3 liczb");
            return conv(v);
        };

        if (inBrush) {
            if (key == "side") {
                std::string materialName;
                if (!(ss >> materialName)) fail("side wymaga materiału i 9 współrzędnych");
                PendingBrushSide side;
                try {
                    side.material = parseBrushMaterial(materialName);
                } catch (const std::exception&) {
                    fail("nieznany materiał brusha; użyj wall, concrete, floor, metal albo panel");
                }
                side.points = {vec3(), vec3(), vec3()};
                side.line = no;
                std::string extra;
                if (ss >> extra) fail("nadmiarowe dane po punktach side");
                if (pendingBrush.size() >= 128) fail("brush przekracza limit 128 ścian");
                pendingBrush.push_back(side);
                continue;
            }
            if (key == "endbrush") {
                try {
                    appendBrushGeometry(pendingBrush, s);
                } catch (const std::exception& e) {
                    fail(e.what());
                }
                pendingBrush.clear();
                inBrush = false;
                continue;
            }
            fail("wewnątrz brush dozwolone są tylko dyrektywy side i endbrush");
        }

        if (key == "brush") {
            std::string extra;
            if (ss >> extra) fail("dyrektywa brush nie przyjmuje argumentów");
            inBrush = true;
            brushStartLine = no;
            pendingBrush.clear();
            continue;
        }
        if (key == "endbrush") fail("endbrush bez otwartego brush");
        if (key == "brush_box") {
            std::string materialName;
            if (!(ss >> materialName)) fail("brush_box wymaga materiału i dwóch punktów 3D");
            BrushMaterial material;
            try {
                material = parseBrushMaterial(materialName);
            } catch (const std::exception&) {
                fail("nieznany materiał brusha; użyj wall, concrete, floor, metal albo panel");
            }
            const glm::vec3 a = vec3();
            const glm::vec3 b = vec3();
            std::string extra;
            if (ss >> extra) fail("nadmiarowe dane po brush_box");
            try {
                appendBrushBox(material, a, b, s);
            } catch (const std::exception& e) {
                fail(e.what());
            }
            continue;
        }

        if      (key == "coords")       { std::string c; ss >> c; blender = (c == "blender"); }
        else if (key == "background")   { std::string p; ss >> p; s.background = file.parent_path() / p; }
        else if (key == "floor_tex")    { std::string p; ss >> p; s.floorTexture = file.parent_path() / p; }
        else if (key == "wall_tex")     { std::string p; ss >> p; s.wallTexture = file.parent_path() / p; }
        else if (key == "camera_pos")   s.camPos = vec3();
        else if (key == "camera_look")  s.camLook = vec3();
        else if (key == "camera_fov")   { if (!(ss >> s.fovH) || s.fovH <= 0.f) fail("camera_fov > 0"); }
        else if (key == "player_start") s.playerStart = vec3();
        else if (key == "bounds" || key == "wall" || key == "floor") { // DODANO "floor"
            const glm::vec3 a = vec3(), b = vec3();
            const Aabb box{glm::min(a, b), glm::max(a, b)};
            if (key == "bounds") s.bounds = box;
            else if (key == "wall") s.walls.push_back(box);
            else s.floors.push_back(box); // NOWE
        }
        else fail("nieznana dyrektywa");
    }
    if (inBrush) {
        throw std::runtime_error(file.string() + ":" + std::to_string(brushStartLine) + ": brush bez endbrush");
    }
    if (s.background.empty()) throw std::runtime_error(file.string() + ": brak dyrektywy 'background'");
    buildBrushBsp(s);
    s.brushMeshRevision = nextBrushMeshRevision();
    return s;
}

bool SceneManager::changed() const {
    if (activePath_.empty()) return false;
    std::error_code ec;
    return fs::last_write_time(activePath_, ec) != activeStamp_;
}