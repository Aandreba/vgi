#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vgi::math {
    /// @brief Halper class to handle 3D transformation.
    struct transf3d {
        /// @brief Creates a transform from a basis matrix and an origin
        /// @param basis 3x3 basis matrix
        /// @param origin origin of the transformation
        constexpr explicit transf3d(const glm::mat3& basis,
                                    const glm::vec3& origin = glm::vec3{0.0f}) noexcept :
            basis(basis), origin(origin) {}

        /// @brief Creates a transformation from an origin
        /// @param origin Origin of the transformation
        constexpr explicit transf3d(const glm::vec3& origin) noexcept :
            basis(1.0f), origin(origin) {}

        /// @brief Creates a transformation from an origin and a rotation
        /// @param origin Origin of the transformation
        /// @param quat Quaternion representing the transform's rotation
        inline transf3d(const glm::vec3& origin, const glm::quat& quat) noexcept :
            basis(glm::mat3_cast(quat)), origin(origin) {}

        /// @brief Creates a transformation from an origin and a rotation
        /// @param origin Origin of the transformation
        /// @param angle Angle of rotation (in radians)
        /// @param axis Axis of rotation
        inline transf3d(const glm::vec3& origin, float angle, const glm::vec3& axis) noexcept :
            basis(glm::angleAxis(angle, axis)), origin(origin) {}

        /// @brief Creates a transformation from an origin and a scaling factor
        /// @param origin Origin of the transformation
        /// @param scale Scale factor of the transformation
        constexpr transf3d(const glm::vec3& origin, const glm::vec3& scale) noexcept :
            basis(scale[0], 0.0f, 0.0f, 0.0f, scale[1], 0.0f, 0.0f, 0.0f, scale[2]),
            origin(origin) {}

        /// @brief Creates a transformation from an origin and a scaling factor
        /// @param origin Origin of the transformation
        /// @param scale Scale factor of the transformation. The same scaling is applied on all
        /// axis
        constexpr transf3d(const glm::vec3& origin, float scale) noexcept :
            basis(scale), origin(origin) {}

        /// @brief Creates a transformation from an origin, rotation and scale
        /// @param origin Origin of the transformation
        /// @param quat Quaternion representing the transform's rotation
        /// @param scale Scale factor of the transformation
        inline transf3d(const glm::vec3& origin, const glm::quat& quat,
                        const glm::vec3& scale) noexcept :
            basis(glm::scale(glm::mat4_cast(quat), scale)), origin(origin) {}

        /// @brief Creates a transformation from a rotation
        /// @param quat Quaternion representing the transform's rotation
        inline transf3d(const glm::quat& quat) noexcept :
            basis(glm::mat3_cast(quat)), origin(0.0f) {}

        /// @brief Creates a transformation from a rotation and scaling factor
        /// @param quat Quaternion representing the transform's rotation
        /// @param scale Scale factor of the transformation
        inline transf3d(const glm::quat& quat, const glm::vec3& scale) noexcept :
            basis(glm::scale(glm::mat4_cast(quat), scale)), origin(0.0f) {}

        /// @brief Creates a transformation from a rotation and scaling factor
        /// @param quat Quaternion representing the transform's rotation
        /// @param scale Scale factor of the transformation. The same scaling is applied on all
        /// axis
        inline transf3d(const glm::quat& quat, float scale) noexcept :
            basis(glm::mat3_cast(quat) * scale), origin(0.0f) {}

        /// @brief Translates the origin of the transformation
        /// @param offset Translation offset
        /// @returns The translated transformation
        constexpr transf3d translate(const glm::vec3& offset) const noexcept {
            return transf3d(this->basis, this->origin + offset);
        }

        /// @brief Rotates the transformation arround itself
        /// @param quat Quaternion representing the rotation
        /// @return The rotated transformation
        inline transf3d rotate(const glm::quat& quat) const noexcept {
            return transf3d(glm::mat3_cast(quat) * this->basis, this->origin);
        }

        /// @brief Rotates the transformation arround itself
        /// @param angle Angle of rotation (in radians)
        /// @param axis Axis of rotation
        /// @return The rotated transformation
        inline transf3d rotate(float angle, const glm::vec3& axis) const noexcept {
            return rotate(glm::angleAxis(angle, axis));
        }

        /// @brief Scales the transformation
        /// @param scale Scaling factor
        /// @return The scaled transformation
        constexpr transf3d scale(const glm::vec3& scale) const noexcept {
            return transf3d(glm::mat3(this->basis[0] * scale[0], this->basis[1] * scale[1],
                                      this->basis[2] * scale[2]),
                            this->origin);
        }

        /// @brief Scales the transformation
        /// @param scale Scaling factor. The same scaling factor is applied to all axis
        /// @return The scaled transformation
        constexpr transf3d scale(float scale) const noexcept {
            return transf3d(this->basis * scale, this->origin);
        }

        /// @brief Casts a transformation into it's underlying `glm::mat4`
        constexpr operator glm::mat4() const noexcept {
            return glm::mat4(glm::vec4(this->basis[0], 0.0f), glm::vec4(this->basis[1], 0.0f),
                             glm::vec4(this->basis[2], 0.0f), glm::vec4(this->origin, 1.0f));
        }

        /// @brief Applies the transformation
        /// @param other Transformation to which the left transformation is applied
        /// @return The new transformation
        constexpr transf3d operator*(const transf3d& other) const noexcept {
            return static_cast<glm::mat4>(*this) * static_cast<glm::mat4>(other);
        }

        /// @brief Applies the transformation
        /// @param other Vector to which the transformation is applied
        /// @return The transformed vector
        constexpr glm::vec3 operator*(const glm::vec3& other) const noexcept {
            return (this->basis * other) + this->origin;
        }

    private:
        glm::mat3 basis;
        glm::vec3 origin;

        constexpr transf3d(const glm::mat4& mat) noexcept : basis(mat), origin(mat[3]) {}
    };
}  // namespace vgi::math
