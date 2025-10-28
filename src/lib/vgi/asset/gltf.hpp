#pragma once

#include <filesystem>
#include <unordered_map>
#include <variant>
#include <vgi/forward.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/texture.hpp>

namespace vgi::asset::gltf {
    struct primitive {
        std::variant<vgi::mesh<uint16_t>, vgi::mesh<uint32_t>> mesh;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct mesh {
        std::vector<primitive> primitives;
        std::string name;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct texture {
        texture_sampler texture;
        std::string name;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct asset {
        std::vector<mesh> meshes;
        std::vector<texture> textures;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    asset import(window& parent, const std::filesystem::path& path,
                 const std::filesystem::path& directory);

    inline asset import(window& parent, const std::filesystem::path& path) {
        return import(parent, path, path.parent_path());
    }
}  // namespace vgi::asset::gltf
