#pragma once

#include <filesystem>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vgi/forward.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/texture.hpp>

namespace vgi::asset::gltf {
    enum struct alpha_mode {
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
        alpha_mode alpha_mode = alpha_mode::opaque;
        /// @brief The alpha cutoff value of the material
        float alpha_cutoff = 1.0f;
        /// @brief Specifies whether the material is double sided
        bool double_sided = false;
        /// @brief The name of the material
        std::string name;
    };

    struct primitive {
        /// @brief Vertex & index data stored on the device
        std::variant<vgi::mesh<uint16_t>, vgi::mesh<uint32_t>> mesh;
        /// @brief The material to apply to this primitive when rendering, if any
        std::shared_ptr<material> material;
        /// @brief The topology type of primitives to render
        vk::PrimitiveTopology topology;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct mesh {
        /// @brief An array of primitives, each defining geometry to be rendered
        std::vector<primitive> primitives;
        /// @brief The name of the mesh
        std::string name;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct texture {
        /// @brief The combined texture and samlers, stored on the device
        texture_sampler texture;
        /// @brief The name of the texture
        std::string name;

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

    struct joint {
        /// @brief Node to which this joint is attached
        size_t skin;
        /// @brief Index of the joint inside the skin
        size_t index;
        /// @brief Inverse bind matrix
        glm::mat4 inv_bind{1.0f};
    };

    struct node {
        /// @brief Transformations of the node relative to it's parent
        math::transf3d local_transform;
        /// @brief The index of the mesh in this node, if any
        std::optional<size_t> mesh;
        /// @brief The index of the skin referenced by this node, if any
        std::optional<size_t> skin;
        /// @brief An array of all the skin joints attached to this node
        std::vector<joint> attachments;
        /// @brief The indices of this node’s children
        std::vector<size_t> children;
        /// @brief The name of the node
        std::string name;
    };

    /// @brief The root nodes of a scene
    struct scene {
        /// @brief The indices of each root node
        std::vector<size_t> roots;
        /// @brief The name of the node
        std::string name;
    };

    struct asset {
        /// @brief An array of all the meshes of the asset
        std::vector<mesh> meshes;
        /// @brief An array of all the textures of the asset
        std::vector<texture> textures;
        /// @brief An array of all the nodes of the asset
        std::vector<node> nodes;
        /// @brief An array of all the scenes of the asset
        std::vector<scene> scenes;
        /// @brief An array of all the names of the skins of the asset
        std::vector<std::string> skins;

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
