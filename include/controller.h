#pragma once

#include <functional>
#include <unordered_map>
#include <deque>
#include <bitset>
#include <tuple>
#include <cstdint>
#include <optional>
#include "keyboard.h"
#include "mouse.h"
#include <glm/vec2.hpp>

namespace Nothofagus
{

enum class DiscreteTrigger: std::uint8_t {Press, Release};

struct KeyboardTrigger
{
    Key key;
    DiscreteTrigger trigger;
};

struct KeyboardTriggerHash
{
    std::size_t operator()(const KeyboardTrigger& trigger) const noexcept
    {
        using KeyBitset = std::bitset<8>;
        using TriggerBitset = std::bitset<1>;
        std::size_t keyHash = std::hash<KeyBitset>{}(KeyBitset(static_cast<std::uint8_t>(trigger.key)));
        std::size_t triggerHash = std::hash<TriggerBitset>{}(TriggerBitset(static_cast<std::uint8_t>(trigger.trigger)));
        return (keyHash << 1) + (triggerHash);
    }
};

struct KeyboardTriggerEqual
{
    bool operator()(const KeyboardTrigger& lhs, const KeyboardTrigger& rhs) const noexcept
    {
        auto asTuple = [](const KeyboardTrigger& kt)
        {
            return std::make_tuple(kt.key, kt.trigger);
        };
        return asTuple(lhs) == asTuple(rhs);
    }
};

struct MouseButtonTrigger
{
    MouseButton button;
    DiscreteTrigger trigger;
};

struct MouseButtonTriggerHash
{
    std::size_t operator()(const MouseButtonTrigger& trigger) const noexcept
    {
        using ButtonBitset = std::bitset<4>;
        using TriggerBitset = std::bitset<1>;
        std::size_t buttonHash = std::hash<ButtonBitset>{}(ButtonBitset(static_cast<std::uint8_t>(trigger.button)));
        std::size_t triggerHash = std::hash<TriggerBitset>{}(TriggerBitset(static_cast<std::uint8_t>(trigger.trigger)));
        return (buttonHash << 1) + (triggerHash);
    }
};

struct MouseButtonTriggerEqual
{
    bool operator()(const MouseButtonTrigger& lhs, const MouseButtonTrigger& rhs) const noexcept
    {
        auto asTuple = [](const MouseButtonTrigger& mt)
        {
            return std::make_tuple(mt.button, mt.trigger);
        };
        return asTuple(lhs) == asTuple(rhs);
    }
};

using Action = std::function<void()>;
using TriggerActions = std::unordered_map<KeyboardTrigger, Action, KeyboardTriggerHash, KeyboardTriggerEqual>;
using ActiveActions = std::deque<KeyboardTrigger>;
using MouseTriggerActions = std::unordered_map<MouseButtonTrigger, Action, MouseButtonTriggerHash, MouseButtonTriggerEqual>;
using ActiveMouseActions = std::deque<MouseButtonTrigger>;

/* \brief true returned on successful operation. */
class Controller
{
public:
    Controller() = default;

    bool registerAction(KeyboardTrigger keyboardTrigger, Action action);
    bool deleteAction(KeyboardTrigger keyboardTrigger);

    bool registerMouseAction(MouseButtonTrigger mouseButtonTrigger, Action action);
    bool deleteMouseAction(MouseButtonTrigger mouseButtonTrigger);
    void registerMouseMove(std::function<void(glm::vec2)> callback);
    void registerMouseScroll(std::function<void(glm::vec2)> callback);

    glm::vec2 getMousePosition() const;

    void processInputs();

    void activate(KeyboardTrigger keyboardTrigger);
    void activateMouseButton(MouseButtonTrigger mouseButtonTrigger);
    void updateMousePosition(glm::vec2 position);
    void scrolled(glm::vec2 offset);

private:
    TriggerActions mTriggerActions;
    ActiveActions mActiveActions;

    MouseTriggerActions mMouseTriggerActions;
    ActiveMouseActions mActiveMouseActions;
    std::optional<std::function<void(glm::vec2)>> mMouseMoveCallback;
    std::optional<std::function<void(glm::vec2)>> mMouseScrollCallback;
    glm::vec2 mMousePosition{0.0f, 0.0f};
};


}