#pragma once

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vgi/forward.hpp>
#include <vgi/vulkan.hpp>

namespace vgi::math {
    /// @brief Helper structure that manages both the view and projection matrices
    struct camera {
        /// @brief Position of the camera
        glm::vec3 origin{0.0f, 0.0f, 0.0f};
        /// @brief Direction at which the camera is looking at
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
        /// @brief The camera's upward direction
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        /// @brief Near plane
        float z_near = 1e-3f;
        /// @brief Far plane
        float z_far = 1e3f;

        /// @brief Translates the camera
        /// @param offset Offset by which the camera is moved
        constexpr void translate(const glm::vec3& offset) noexcept { this->origin += offset; }

        /// @brief Rotates the camera
        /// @param rot Quaternion representing the camera's rotation
        inline void rotate(const glm::quat& rot) noexcept {
            glm::mat3 mat = glm::mat3_cast(rot);
            glm::vec3 rotated = mat * this->direction;
            this->direction = glm::normalize(rotated);
        }

        /// @brief Rotates the camera
        /// @param angle Angle of rotation (in radians)
        /// @param axis Axis for rotation
        inline void rotate(float angle, const glm::vec3& axis) noexcept {
            this->rotate(glm::angleAxis(angle, axis));
        }

        /// @brief Makes the camera look at a certain diraction
        /// @param target Position to which the camera is looking
        /// @param up Upward direction
        inline void look_at(const glm::vec3& target,
                            const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept {
            this->direction = glm::normalize(target - this->origin);
            this->up = up;
        }

        /// @brief Returns the view matrix of the camera
        /// @return View matrix of the camera
        inline glm::mat4 view() const noexcept {
            const glm::vec3 eye = this->origin;
            const glm::vec3 up = this->up;
            const glm::vec3 f = this->direction;
            const glm::vec3 s = glm::normalize(glm::cross(f, up));
            const glm::vec3 u = glm::cross(s, f);

            glm::mat4 Result(1.0f);
            Result[0][0] = s.x;
            Result[1][0] = s.y;
            Result[2][0] = s.z;
            Result[0][1] = u.x;
            Result[1][1] = u.y;
            Result[2][1] = u.z;
            Result[0][2] = -f.x;
            Result[1][2] = -f.y;
            Result[2][2] = -f.z;
            Result[3][0] = -glm::dot(s, eye);
            Result[3][1] = -glm::dot(u, eye);
            Result[3][2] = glm::dot(f, eye);

            return Result;
        }

    protected:
        /// @brief Default constructor
        constexpr camera() = default;
    };

    /// @brief A camera with perspective projection
    struct perspective_camera : public camera {
        /// @brief Field of view (in radians)
        float fovy = glm::radians(60.0f);

        /// @brief Default constructor
        constexpr perspective_camera() = default;

        /// @brief Returns the camera's perspective projection matrix
        /// @param aspect Aspect ratio of the projection region
        /// @return The camera's perspective projection matrix
        inline glm::mat4 projection(float aspect) const noexcept {
            glm::mat4 proj = glm::perspective(this->fovy, aspect, this->z_near, this->z_far);
            proj[1] = -proj[1];
            return proj;
        }

        /// @brief Returns the camera's perspective projection matrix
        /// @param width Width of the projection region
        /// @param height Height of the projection region
        /// @return The camera's perspective projection matrix
        inline glm::mat4 projection(uint32_t width, uint32_t height) const noexcept {
            return this->projection(static_cast<float>(width) / static_cast<float>(height));
        }

        /// @brief Returns the camera's perspective projection matrix
        /// @param extent Extent of the projection region
        /// @return The camera's perspective projection matrix
        inline glm::mat4 projection(const vk::Extent2D& extent) const noexcept {
            return this->projection(extent.width, extent.height);
        }
    };

    /// @brief A camera with orthographic projection
    struct ortho_camera : public camera {
        /// @brief Left coordinate of the frustum
        float left;
        /// @brief Right coordinate of the frustum
        float right;
        /// @brief Top coordinate of the frustum
        float top;
        /// @brief Bottom coordinate of the frustum
        float bottom;

        /// @brief Returns the camera's perspective projection matrix
        /// @param aspect Aspect ratio of the projection region
        /// @return The camera's perspective projection matrix
        inline glm::mat4 projection() const noexcept {
            return glm::ortho(this->left, this->right, this->top, this->bottom, this->z_near,
                              this->z_far);
        }
    };
}  // namespace vgi::math
