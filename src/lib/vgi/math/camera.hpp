#pragma once

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vgi/forward.hpp>

namespace vgi::math {
    /// @brief Helper structure that manages both the view and projection matrices
    struct camera {
        /// @brief Field of view (in radians)
        float fovy = glm::radians(60.0f);
        /// @brief Near plane
        float z_near = 0.01f;
        /// @brief Far plane
        float z_far = 1000.0f;

        /// @brief Translates the camera
        /// @param offset Offset by which the camera is moved
        inline void translate(const glm::vec3& offset) noexcept {
            this->view_ = glm::translate(this->view_, -offset);
        }

        /// @brief Rotates the camera
        /// @param rot Quaternion representing the camera's rotation
        inline void rotate(const glm::quat& rot) noexcept {
            this->view_ = glm::mat4_cast(glm::conjugate(rot)) * this->view_;
        }

        /// @brief Rotates the camera
        /// @param angle Angle of rotation (in radians)
        /// @param axis Axis for rotation
        inline void rotate(float angle, const glm::vec3& axis) noexcept {
            this->rotate(glm::angleAxis(angle, axis));
        }

        /// @brief Makes the camera look at a certain diraction from a specified position
        /// @param origin Position from which the camera is looking
        /// @param target Position/Direction to which the camera is looking
        /// @param up Up direction
        inline void look_at_from(const glm::vec3& origin, const glm::vec3& target,
                                 const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept {
            this->view_ = glm::lookAt(origin, target, up);
        }

        /// @brief Makes the camera look at a certain diraction
        /// @param target Position/Direction to which the camera is looking
        /// @param up Up direction
        inline void look_at(const glm::vec3& target,
                            const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) noexcept {
            this->look_at_from(-this->view_[3], target, up);
        }

        /// @brief Returns the camera's perspective projection matrix
        /// @param aspect Aspect ratio of the projection region
        /// @return The camera's perspective projection matrix
        inline glm::mat4 perspective(float aspect) const noexcept {
            return glm::perspective(this->fovy, aspect, this->z_near, this->z_far);
        }

        /// @brief Returns the view matrix of the camera
        /// @return View matrix of the camera
        inline glm::mat4 view() const noexcept { return this->view_; }

    private:
        glm::mat4 view_{1.0f};
    };
}  // namespace vgi::math
