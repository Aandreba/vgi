#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace vgi::math {
    /// @brief A 3 dimensional transformation
    struct transf3d {
        constexpr transf3d() noexcept : inner(1.0f) {}

        /// @brief Creates a translated transform
        /// @param origin Position of the transform
        constexpr transf3d(const glm::vec3& origin) noexcept :
            inner(glm::translate(glm::mat4(1.0f), origin)) {}

        /// @brief Creates a translated and rotated transform
        /// @param origin Position of the transform
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        transf3d(const glm::vec3& origin, float angle, const glm::vec3& axis) noexcept :
            inner(glm::translate(glm::rotate(glm::mat4(1.0f), angle, axis), origin)) {}

        /// @brief Creates a translated, rotated and scaled transform
        /// @param origin Position of the transform
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        /// @param scale Ratio of scaling for each axis.
        transf3d(const glm::vec3& origin, float angle, const glm::vec3& axis,
                 const glm::vec3& scale) noexcept :
            inner(glm::translate(glm::rotate(glm::scale(glm::mat4(1.0f), scale), angle, axis),
                                 origin)) {}

        /// @brief Creates a translated, rotated and scaled transform
        /// @param origin Position of the transform
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        /// @param scale Ratio of scaling.
        transf3d(const glm::vec3& origin, float angle, const glm::vec3& axis, float scale) noexcept
            : inner(glm::translate(glm::rotate(glm::mat4(scale), angle, axis), origin)) {}

        /// @brief Builds the translated version of the transform
        /// @param offset Coordinates of a translation vector
        /// @returns Translated transform
        constexpr transf3d translated(const glm::vec3& offset) const noexcept {
            return glm::translate(this->inner, offset);
        }

        /// @brief Rotates the current transform arround itself
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        inline transf3d rotate(float angle, const glm::vec3& axis) noexcept {
            return glm::rotate(this->inner, angle, axis);
        }

        /// @brief Orbits the current transform arround the coordinate origin
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        inline transf3d orbit(float angle, const glm::vec3& axis) noexcept {
            return glm::rotate(glm::mat4(1.0f), angle, axis) * this->inner;
        }

        /// @brief Orbits the current transform arround a point
        /// @param point Point to rotate arround
        /// @param angle Rotation angle expressed in radians.
        /// @param axis Rotation axis, recommended to be normalized.
        inline transf3d orbit(const glm::vec3& point, float angle, const glm::vec3& axis) noexcept {
            return glm::translate(
                    glm::rotate(glm::mat4(1.0f), angle, axis) * glm::translate(this->inner, -point),
                    point);
        }

        /// @brief Returns the 4x4 matrix that represents the 3D transform
        inline operator glm::mat4() const noexcept { return this->inner; }

    private:
        glm::mat4 inner;

        constexpr transf3d(const glm::mat4& inner) noexcept : inner(inner) {}
    };
}  // namespace vgi::math
