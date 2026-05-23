#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 简单自由飞行/轨道观察相机。
// 本项目在主循环中会把它约束成“围绕星球中心观察”的相机，但类本身仍保留
// 常规 FPS/Fly Camera 的移动、旋转和视图矩阵计算能力。
class FlyCamera
{
public:
    // 统一描述键盘移动方向，调用方负责把输入映射成这些语义方向。
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
    float movementSpeed = 80.0f;
    float mouseSensitivity = 0.10f;
    float fieldOfView = 60.0f;

    explicit FlyCamera(glm::vec3 startPosition = glm::vec3(0.0f, 3.0f, 6.0f))
        : position(startPosition), worldUp(0.0f, 1.0f, 0.0f)
    {
        updateOrientationVectors();
    }

    // 生成 OpenGL 常用的 view matrix。
    glm::mat4 viewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    // 按当前朝向移动；deltaSeconds 让移动速度与帧率无关。
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

    // 根据鼠标增量更新 yaw/pitch，再重建 front/right/up 三个方向向量。
    void rotate(float deltaX, float deltaY)
    {
        yaw += deltaX * mouseSensitivity;
        pitch += deltaY * mouseSensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
        updateOrientationVectors();
    }

    // 直接让相机朝向某个目标点。轨道相机模式会频繁使用类似逻辑。
    void lookAt(const glm::vec3& target)
    {
        front = glm::normalize(target - position);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }

    // 通过滚轮改变 FOV，保留最小/最大范围，避免透视过度畸变。
    void zoom(float scrollDelta)
    {
        fieldOfView -= scrollDelta;
        fieldOfView = glm::clamp(fieldOfView, 10.0f, 90.0f);
    }

private:
    // 从 yaw/pitch 反推相机局部坐标系。
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
