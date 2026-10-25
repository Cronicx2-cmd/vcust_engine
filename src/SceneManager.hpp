// src/SceneManager.hpp
#pragma once
#include "Scene.hpp"
#include <cstdint>
#include <map>

struct Image { int w{}, h{}; std::vector<uint8_t> rgba; };
Image loadImageRGBA(const std::filesystem::path& p);

class SceneManager {
public:
    explicit SceneManager(std::filesystem::path baseDir);
    void scan();
    const std::map<std::string, std::filesystem::path>& maps() const { return maps_; }
    Scene load(const std::string& id);
    bool changed() const;
private:
    std::filesystem::path baseDir_, activePath_;
    std::filesystem::file_time_type activeStamp_{};
    std::map<std::string, std::filesystem::path> maps_;
};
