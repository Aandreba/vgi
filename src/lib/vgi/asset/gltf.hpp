#pragma once

#include <filesystem>
#include <unordered_map>
#include <variant>
#include <vgi/resource/mesh.hpp>

namespace vgi::asset::gltf {
    struct primitive {
        std::variant<mesh<uint16_t>, mesh<uint32_t>> mesh;
    };

    struct asset {
        std::unordered_map<std::string, std::vector<primitive>> meshes;
    };

    asset import(const std::filesystem::path& path, const std::filesystem::path& directory);
    asset import(const std::filesystem::path& path) { return import(path, path.parent_path()); }
}  // namespace vgi::asset::gltf
