#pragma once

#include <chrono>
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

    enum struct interpolation {
        linear,
        step,
        cubic_spline,
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

    /// @brief Combines timestamps with a sequence of output values and defines an interpolation
    /// algorithm
    struct animation_sampler {
        using duration_type = std::chrono::duration<float>;

        interpolation interpolation;
        unique_span<const float> keyframes;
        unique_span<const float> values;

        /// @brief Returns the duration of the sampler
        /// @returns Duration of the sampler
        /// @warning Note that the sampler may not start at time zero, so this value may not equal
        /// `keyframes.back()`
        inline duration_type duration() const noexcept {
            VGI_ASSERT(this->keyframes.size() > 0);
            return duration_type{this->keyframes.back() - this->keyframes.front()};
        }

        /// @brief Samples the animation at the specified time.
        /// @param t Time at which to sample
        /// @returns The sampled value
        /// @details If the value `t` is out of range, the returned sample is clamped to the edges
        /// of the sampler. For example, if `t` is greater than the sampler's duration, the last
        /// value of the sampler is returned.
        template<class T>
        T sample(duration_type t) const;

        /// @brief Samples the animation at the specified time.
        /// @param t Time at which to sample
        /// @returns The sampled value
        /// @details If the value `t` is out of range, the returned sample is clamped to the edges
        /// of the sampler. For example, if `t` is greater than the sampler's duration, the last
        /// value of the sampler is returned.
        template<class T, class Rep, class Period>
        inline T sample(const std::chrono::duration<Rep, Period>& t) const {
            return this->template sample<T>(std::chrono::duration_cast<duration_type>(t));
        }
    };

    /// @brief A keyframe animation
    struct animation {
        /// @brief An array with all the samplers of the animation
        std::vector<animation_sampler> samplers;
        /// @brief The name of the animation
        std::string name;
    };

    /// @brief Properties of the attachment between an animation and a joint
    struct node_animation {
        /// @brief Index of the sampler used for the translation, if any
        std::optional<size_t> origin = std::nullopt;
        /// @brief Index of the sampler used for the rotation, if any
        std::optional<size_t> rotation = std::nullopt;
        /// @brief Index of the sampler used for the scale, if any
        std::optional<size_t> scale = std::nullopt;
    };

    struct node {
        /// @brief Translation of the node relative to it's parent
        glm::vec3 local_origin;
        /// @brief Rotation of the node relative to it's parent
        glm::quat local_rotation;
        /// @brief Scale of the node relative to it's parent
        glm::vec3 local_scale;
        /// @brief Animations attached to this node, and the properties of the attachment.
        std::unordered_map<size_t, node_animation> animations;
        /// @brief The index of the mesh in this node, if any
        std::optional<size_t> mesh;
        /// @brief The index of the skin referenced by this node, if any
        std::optional<size_t> skin;
        /// @brief An array of all the skin joints attached to this node
        std::vector<joint> attachments;
        /// @brief The indices of this node's children
        std::vector<size_t> children;
        /// @brief The name of the node
        std::string name;
    };

    /// @brief The root nodes of a scene
    struct scene {
        /// @brief The indices of each root node
        std::vector<size_t> roots;
        /// @brief The name of the scene
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
        /// @brief An array of all the animations of the asset
        std::vector<animation> animations;

        asset() = default;
        asset(window& parent, const std::filesystem::path& path,
              const std::filesystem::path& directory);

        inline asset(window& parent, const std::filesystem::path& path) :
            asset(parent, path, path.parent_path()) {}

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(window& parent) &&;
    };

}  // namespace vgi::asset::gltf
