#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class FlyCamera
{
public:
    enum class MovementDirection {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down
    };

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw = -90.0f;
    float pitch = -25.0f;
    float movementSpeed = 8.0f;
    float mouseSensitivity = 0.10f;
    float fieldOfView = 60.0f;

    explicit FlyCamera(glm::vec3 startPosition = glm::vec3(0.0f, 3.0f, 6.0f))
        : position(startPosition), worldUp(0.0f, 1.0f, 0.0f)
    {
        updateOrientationVectors();
    }

    glm::mat4 viewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    void move(MovementDirection direction, float deltaSeconds)
    {
        const float step = movementSpeed * deltaSeconds;

        if (direction == MovementDirection::Forward)  position += front * step;
        if (direction == MovementDirection::Backward) position -= front * step;
        if (direction == MovementDirection::Left)     position -= right * step;
        if (direction == MovementDirection::Right)    position += right * step;
        if (direction == MovementDirection::Up)       position += worldUp * step;
        if (direction == MovementDirection::Down)     position -= worldUp * step;
    }

    void rotate(float deltaX, float deltaY)
    {
        yaw += deltaX * mouseSensitivity;
        pitch += deltaY * mouseSensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
        updateOrientationVectors();
    }

    void zoom(float scrollDelta)
    {
        fieldOfView -= scrollDelta;
        fieldOfView = glm::clamp(fieldOfView, 10.0f, 90.0f);
    }

private:
    void updateOrientationVectors()
    {
        glm::vec3 forward;
        forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward.y = sin(glm::radians(pitch));
        forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        front = glm::normalize(forward);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};
