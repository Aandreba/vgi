#pragma once

#include <filesystem>
#include <unordered_map>
#include <variant>
#include <vgi/forward.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/texture.hpp>

namespace vgi::asset::gltf {
    enum struct material_alpha_mode {
        opaque,
        mask,
        blend,
    };

    struct normal_texture {
        /// @brief The index of the texture
        size_t texture;
        /// @brief The scalar parameter applied to each normal vector of the normal texture
        float scale = 1.0f;
    };

    struct occlusion_texture {
        /// @brief The index of the texture
        size_t texture;
        /// @brief A scalar multiplier controlling the amount of occlusion applied
        float strength = 1.0f;
    };

    struct emissive_texture {
        /// @brief The index of the texture
        size_t texture;
        /// @brief The factors for the emissive color of the material
        glm::vec3 factor{0.0f};
    };

    struct material {
        /// @brief The tangent space normal texture
        std::optional<normal_texture> normal = std::nullopt;
        /// @brief The occlusion texture
        std::optional<occlusion_texture> occlusion = std::nullopt;
        /// @brief The emissive texture
        std::optional<emissive_texture> emissive = std::nullopt;
        /// @brief The alpha rendering mode of the material
        material_alpha_mode alpha_mode = material_alpha_mode::opaque;
        /// @brief The alpha cutoff value of the material
        float alpha_cutoff = 1.0f;
        /// @brief Specifies whether the material is double sided
        bool double_sided = false;
        /// @brief The name of the material
        std::string name;
    };

    struct primitive {
        std::variant<vgi::mesh<uint16_t>, vgi::mesh<uint32_t>> mesh;
        std::optional<material> material;
        vk::PrimitiveTopology topology;

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
