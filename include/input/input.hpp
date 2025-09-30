#pragma once

#include "input_codes.hpp"
#include <unordered_map>

union SDL_Event;

class Input
{
public:
    Input() = default;

    void Update();
    void UpdateEvent(const SDL_Event& event);

    [[nodiscard]] bool IsKeyPressed(KeyboardCode key) const;
    [[nodiscard]] bool IsKeyHeld(KeyboardCode key) const;
    [[nodiscard]] bool IsKeyReleased(KeyboardCode key) const;

    [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonHeld(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonReleased(MouseButton button) const;

    void SetMousePositionToAbsoluteMousePosition();
    void GetMousePosition(int32_t& x, int32_t& y) const;

private:
    template <typename T>
    struct InputDevice
    {
        std::unordered_map<T, bool> inputPressed {};
        std::unordered_map<T, bool> inputHeld {};
        std::unordered_map<T, bool> inputReleased {};
    };

    struct Mouse : InputDevice<MouseButton>
    {
        float positionX {}, positionY {};
    } _mouse {};

    InputDevice<KeyboardCode> _keyboard {};
};
