#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw   = -90.0f;
    float Pitch = -25.0f;
    float Speed      = 8.0f;
    float Sensitivity= 0.10f;
    float Fov        = 60.0f;

    Camera(glm::vec3 pos = glm::vec3(0, 3, 6))
        : Position(pos), WorldUp(0,1,0)
    {
        updateVectors();
    }

    glm::mat4 getView() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    enum class Dir { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

    void move(Dir dir, float dt)
    {
        float v = Speed * dt;
        if (dir == Dir::FORWARD)  Position += Front * v;
        if (dir == Dir::BACKWARD) Position -= Front * v;
        if (dir == Dir::LEFT)     Position -= Right * v;
        if (dir == Dir::RIGHT)    Position += Right * v;
        if (dir == Dir::UP)       Position += WorldUp * v;
        if (dir == Dir::DOWN)     Position -= WorldUp * v;
    }

    void rotate(float dx, float dy)
    {
        Yaw   += dx * Sensitivity;
        Pitch += dy * Sensitivity;
        Pitch  = glm::clamp(Pitch, -89.0f, 89.0f);
        updateVectors();
    }

    void zoom(float offset)
    {
        Fov -= offset;
        Fov  = glm::clamp(Fov, 10.0f, 90.0f);
    }

private:
    void updateVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        f.y = sin(glm::radians(Pitch));
        f.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(f);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
