#pragma once
#include <memory>
#include <glm\glm.hpp>

class Input;

struct FlyCameraCreation
{
    glm::vec3 position {};

    float fov = 90.0f;
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.2f;
};

class FlyCamera
{
public:
    FlyCamera(const FlyCameraCreation& creation, const std::shared_ptr<Input>& input);
    void Update(float deltaTime);
    [[nodiscard]] glm::mat4 ViewMatrix() const;
    [[nodiscard]] glm::mat4 ProjectionMatrix() const;

private:
    void UpdateKeyboard(float deltaTime);
    void UpdateMouse();
    void UpdateCameraVectors();

    std::shared_ptr<Input> _input {};

    glm::vec3 _position {};
    glm::vec3 _front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 _up = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 _right = glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 _worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float _yaw = -90.0f;
    float _pitch = 0.0f;

    float _movementSpeed {};
    float _mouseSensitivity {};
    float _fov {};
    float _aspectRatio {};
    float _nearPlane {};
    float _farPlane {};
};
