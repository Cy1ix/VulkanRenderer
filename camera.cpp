#include "camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : position_(position), world_up_(up), yaw_(yaw), pitch_(pitch) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 Camera::getProjectionMatrix(float aspect_ratio) const {
    return glm::perspective(glm::radians(zoom_), aspect_ratio, 0.1f, 100.0f);
}

void Camera::processInput(float delta_time, bool move_forward, bool move_backward,
                         bool move_left, bool move_right) {
    float velocity = movement_speed_ * delta_time;
    
    if (move_forward)
        position_ += front_ * velocity;
    if (move_backward)
        position_ -= front_ * velocity;
    if (move_left)
        position_ -= right_ * velocity;
    if (move_right)
        position_ += right_ * velocity;
}

void Camera::processMouseMovement(float x_offset, float y_offset, bool constrain_pitch) {
    x_offset *= mouse_sensitivity_;
    y_offset *= mouse_sensitivity_;

    yaw_ += x_offset;
    pitch_ += y_offset;

    if (constrain_pitch) {
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
    }

    updateCameraVectors();
}

void Camera::processMouseScroll(float y_offset) {
    zoom_ = std::clamp(zoom_ - y_offset, 1.0f, 45.0f);
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front);

    right_ = glm::normalize(glm::cross(front_, world_up_));
    up_ = glm::normalize(glm::cross(right_, front_));
}
