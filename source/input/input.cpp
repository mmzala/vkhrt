#include "input/input.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

// If the key is found, it returns it's value, otherwise it returns the default value given.
template <typename K, typename V>
V UnorderedMapGetOr(const std::unordered_map<K, V>& map, const K& key, const V& defaultValue)
{
    if (auto it = map.find(key); it != map.end())
    {
        return it->second;
    }
    return defaultValue;
}

void Input::Update()
{
    for (auto& key : _keyboard.inputPressed)
    {
        key.second = false;
    }
    for (auto& key : _keyboard.inputReleased)
    {
        key.second = false;
    }


    for (auto& button : _mouse.inputPressed)
    {
        button.second = false;
    }
    for (auto& button : _mouse.inputReleased)
    {
        button.second = false;
    }
}

void Input::UpdateEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    {
        if (event.key.repeat == 0)
        { // Only process on first keydown, not when holding
            KeyboardCode key = static_cast<KeyboardCode>(event.key.key);
            _keyboard.inputPressed[key] = true;
            _keyboard.inputHeld[key] = true;
        }
        break;
    }
    case SDL_EVENT_KEY_UP:
    {
        KeyboardCode key = static_cast<KeyboardCode>(event.key.key);
        _keyboard.inputHeld[key] = false;
        _keyboard.inputReleased[key] = true;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
        MouseButton button = static_cast<MouseButton>(event.button.button);
        _mouse.inputPressed[button] = true;
        _mouse.inputHeld[button] = true;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        MouseButton button = static_cast<MouseButton>(event.button.button);
        _mouse.inputHeld[button] = false;
        _mouse.inputReleased[button] = true;
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
    {
        _mouse.positionX += event.motion.xrel;
        _mouse.positionY += event.motion.yrel;
        break;
    }
    default:
    {
        break;
    }
    }
}

bool Input::IsKeyPressed(KeyboardCode key) const
{
    return UnorderedMapGetOr(_keyboard.inputPressed, key, false);
}

bool Input::IsKeyHeld(KeyboardCode key) const
{
    return UnorderedMapGetOr(_keyboard.inputHeld, key, false);
}

bool Input::IsKeyReleased(KeyboardCode key) const
{
    return UnorderedMapGetOr(_keyboard.inputReleased, key, false);
}

bool Input::IsMouseButtonPressed(MouseButton button) const
{
    return UnorderedMapGetOr(_mouse.inputPressed, button, false);
}

bool Input::IsMouseButtonHeld(MouseButton button) const
{
    return UnorderedMapGetOr(_mouse.inputHeld, button, false);
}

bool Input::IsMouseButtonReleased(MouseButton button) const
{
    return UnorderedMapGetOr(_mouse.inputReleased, button, false);
}
void Input::SetMousePositionToAbsoluteMousePosition()
{
    SDL_GetMouseState(&_mouse.positionX, &_mouse.positionY);
}

void Input::GetMousePosition(int32_t& x, int32_t& y) const
{
    x = static_cast<int32_t>(_mouse.positionX);
    y = static_cast<int32_t>(_mouse.positionY);
}
