#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vgi::math {
    struct transf3d {
        constexpr explicit transf3d(const glm::mat3& basis,
                                    const glm::vec3& origin = glm::vec3{0.0f}) noexcept :
            basis(basis), origin(origin) {}

        constexpr explicit transf3d(const glm::vec3& origin) noexcept :
            basis(1.0f), origin(origin) {}

        inline transf3d(const glm::vec3& origin, const glm::quat& quat) noexcept :
            basis(glm::mat3_cast(quat)), origin(origin) {}

        inline transf3d(const glm::vec3& origin, float angle, const glm::vec3& axis) noexcept :
            basis(glm::angleAxis(angle, axis)), origin(origin) {}

        constexpr transf3d(const glm::vec3& origin, const glm::vec3& scale) noexcept :
            basis(scale[0], 0.0f, 0.0f, 0.0f, scale[1], 0.0f, 0.0f, 0.0f, scale[2]),
            origin(origin) {}

        constexpr transf3d(const glm::vec3& origin, float scale) noexcept :
            basis(scale), origin(origin) {}

        inline transf3d(const glm::quat& quat) noexcept :
            basis(glm::mat3_cast(quat)), origin(0.0f) {}

        inline transf3d(const glm::quat& quat, const glm::vec3& scale) noexcept :
            transf3d(glm::scale(glm::mat4_cast(quat), scale)) {}

        inline transf3d(const glm::quat& quat, float scale) noexcept :
            basis(glm::mat3_cast(quat) * scale), origin(0.0f) {}

        constexpr transf3d translate(const glm::vec3& offset) const noexcept {
            return transf3d(this->basis, this->origin + offset);
        }

        inline transf3d rotate(const glm::quat& quat) const noexcept {
            return transf3d(glm::mat3_cast(quat) * this->basis, this->origin);
        }

        inline transf3d rotate(float angle, const glm::vec3& axis) const noexcept {
            return rotate(glm::angleAxis(angle, axis));
        }

        constexpr transf3d scale(const glm::vec3 scale) const noexcept {
            return transf3d(glm::mat3(this->basis[0] * scale[0], this->basis[1] * scale[1],
                                      this->basis[2] * scale[2]),
                            this->origin);
        }

        constexpr transf3d scale(float scale) const noexcept {
            return transf3d(this->basis * scale, this->origin);
        }

        constexpr operator glm::mat4() const noexcept {
            return glm::mat4(glm::vec4(this->basis[0], 0.0f), glm::vec4(this->basis[1], 0.0f),
                             glm::vec4(this->basis[2], 0.0f), glm::vec4(this->origin, 1.0f));
        }

        constexpr transf3d operator*(const transf3d& other) const noexcept {
            return static_cast<glm::mat4>(*this) * static_cast<glm::mat4>(other);
        }

        constexpr glm::vec3 operator*(const glm::vec3& other) const noexcept {
            return (this->basis * other) + this->origin;
        }

    private:
        glm::mat3 basis;
        glm::vec3 origin;

        constexpr transf3d(const glm::mat4& mat) noexcept : basis(mat), origin(mat[3]) {}
    };
}  // namespace vgi::math
