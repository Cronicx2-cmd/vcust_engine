// src/SceneManager.cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "SceneManager.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
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
    if (s.background.empty()) throw std::runtime_error(file.string() + ": brak dyrektywy 'background'");
    return s;
}

bool SceneManager::changed() const {
    if (activePath_.empty()) return false;
    std::error_code ec;
    return fs::last_write_time(activePath_, ec) != activeStamp_;
}