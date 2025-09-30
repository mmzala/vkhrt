#include "fly_camera.hpp"
#include "input/input.hpp"
#include <glm/gtc/matrix_transform.hpp>

FlyCamera::FlyCamera(const FlyCameraCreation& creation, const std::shared_ptr<Input>& input)
    : _input(input),
    _position(creation.position),
    _movementSpeed(creation.movementSpeed),
    _mouseSensitivity(creation.mouseSensitivity),
    _fov(creation.fov),
    _aspectRatio(creation.aspectRatio),
    _nearPlane(creation.nearPlane),
    _farPlane(creation.farPlane)
{
    UpdateCameraVectors();
}

void FlyCamera::Update(float deltaTime)
{
    UpdateKeyboard(deltaTime);
    UpdateMouse();
    UpdateCameraVectors();
}

glm::mat4 FlyCamera::GetViewMatrix() const
{
    return glm::lookAt(_position, _position + _front, _up);
}

glm::mat4 FlyCamera::GetProjectionMatrix() const
{
    glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(_fov), _aspectRatio, _nearPlane, _farPlane);
    projection[1][1] *= -1; // Inverting Y for Vulkan (not needed with perspectiveVK)
    return projection;
}

void FlyCamera::UpdateKeyboard(float deltaTime)
{
    float velocity = _movementSpeed * deltaTime;
    float forward = static_cast<float>(_input->IsKeyHeld(KeyboardCode::eW)) - static_cast<float>(_input->IsKeyHeld(KeyboardCode::eS));
    float right = static_cast<float>(_input->IsKeyHeld(KeyboardCode::eD)) - static_cast<float>(_input->IsKeyHeld(KeyboardCode::eA));
    float up = static_cast<float>(_input->IsKeyHeld(KeyboardCode::eE)) - static_cast<float>(_input->IsKeyHeld(KeyboardCode::eQ));

    _position += _front * velocity * forward;
    _position += _right * velocity * right;
    _position += _up * velocity * up;
}

void FlyCamera::UpdateMouse()
{
    float deltaX {};
    float deltaY {};
    _input->GetMouseDelta(deltaX, deltaY);

    deltaX *= _mouseSensitivity;
    deltaY *= _mouseSensitivity;

    _yaw += deltaX;
    _pitch += deltaY;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (_pitch > 89.0f)
    {
        _pitch = 89.0f;
    }
    if (_pitch < -89.0f)
    {
        _pitch = -89.0f;
    }
}

void FlyCamera::UpdateCameraVectors()
{
    glm::vec3 front;
    front.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
    front.y = sinf(glm::radians(_pitch));
    front.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
    _front = glm::normalize(front);

    _right = glm::normalize(glm::cross(_front, _worldUp));
    _up = glm::normalize(glm::cross(_right, _front));
}
