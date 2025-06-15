#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect_ratio) const;
    glm::vec3 getPosition() const { return position_; }

    void processInput(float delta_time, bool move_forward, bool move_backward, 
                     bool move_left, bool move_right);
    void processMouseMovement(float x_offset, float y_offset, 
                             bool constrain_pitch = true);
    void processMouseScroll(float y_offset);

private:
    void updateCameraVectors();

    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 world_up_;

    float yaw_;
    float pitch_;
    float movement_speed_ = 2.5f;
    float mouse_sensitivity_ = 0.1f;
    float zoom_ = 45.0f;
};
