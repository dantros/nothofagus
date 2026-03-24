#pragma once

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <bitset>
#include <tuple>
#include <cstdint>
#include <optional>
#include <vector>
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

// ---------------------------------------------------------------------------
// Gamepad types
// ---------------------------------------------------------------------------

enum class GamepadButton : std::uint8_t
{
    A, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DpadUp, DpadRight, DpadDown, DpadLeft
};

enum class GamepadAxis : std::uint8_t
{
    LeftX, LeftY, RightX, RightY,
    LeftTrigger, RightTrigger
};

struct GamepadButtonTrigger
{
    int             gamepadId;
    GamepadButton   button;
    DiscreteTrigger trigger;
};

struct GamepadButtonTriggerHash
{
    std::size_t operator()(const GamepadButtonTrigger& t) const noexcept
    {
        using IdBitset      = std::bitset<4>;
        using ButtonBitset  = std::bitset<4>;
        using TriggerBitset = std::bitset<1>;
        std::size_t idHash      = std::hash<IdBitset>{}(IdBitset(static_cast<unsigned>(t.gamepadId)));
        std::size_t buttonHash  = std::hash<ButtonBitset>{}(ButtonBitset(static_cast<std::uint8_t>(t.button)));
        std::size_t triggerHash = std::hash<TriggerBitset>{}(TriggerBitset(static_cast<std::uint8_t>(t.trigger)));
        return (idHash << 2) ^ (buttonHash << 1) ^ triggerHash;
    }
};

struct GamepadButtonTriggerEqual
{
    bool operator()(const GamepadButtonTrigger& lhs, const GamepadButtonTrigger& rhs) const noexcept
    {
        return std::make_tuple(lhs.gamepadId, lhs.button, lhs.trigger)
            == std::make_tuple(rhs.gamepadId, rhs.button, rhs.trigger);
    }
};

struct GamepadAxisKey
{
    int         gamepadId;
    GamepadAxis axis;
};

struct GamepadAxisKeyHash
{
    std::size_t operator()(const GamepadAxisKey& k) const noexcept
    {
        using IdBitset   = std::bitset<4>;
        using AxisBitset = std::bitset<3>;
        std::size_t idHash   = std::hash<IdBitset>{}(IdBitset(static_cast<unsigned>(k.gamepadId)));
        std::size_t axisHash = std::hash<AxisBitset>{}(AxisBitset(static_cast<std::uint8_t>(k.axis)));
        return (idHash << 1) ^ axisHash;
    }
};

struct GamepadAxisKeyEqual
{
    bool operator()(const GamepadAxisKey& lhs, const GamepadAxisKey& rhs) const noexcept
    {
        return std::make_tuple(lhs.gamepadId, lhs.axis) == std::make_tuple(rhs.gamepadId, rhs.axis);
    }
};

struct GamepadButtonKey
{
    int           gamepadId;
    GamepadButton button;
};

struct GamepadButtonKeyHash
{
    std::size_t operator()(const GamepadButtonKey& k) const noexcept
    {
        using IdBitset     = std::bitset<4>;
        using ButtonBitset = std::bitset<4>;
        std::size_t idHash     = std::hash<IdBitset>{}(IdBitset(static_cast<unsigned>(k.gamepadId)));
        std::size_t buttonHash = std::hash<ButtonBitset>{}(ButtonBitset(static_cast<std::uint8_t>(k.button)));
        return (idHash << 1) ^ buttonHash;
    }
};

struct GamepadButtonKeyEqual
{
    bool operator()(const GamepadButtonKey& lhs, const GamepadButtonKey& rhs) const noexcept
    {
        return std::make_tuple(lhs.gamepadId, lhs.button) == std::make_tuple(rhs.gamepadId, rhs.button);
    }
};

using GamepadTriggerActions = std::unordered_map<GamepadButtonTrigger, Action,    GamepadButtonTriggerHash, GamepadButtonTriggerEqual>;
using ActiveGamepadActions  = std::deque<GamepadButtonTrigger>;
using GamepadAxisCallbacks  = std::unordered_map<GamepadAxisKey,       std::function<void(float)>, GamepadAxisKeyHash,       GamepadAxisKeyEqual>;
using GamepadAxisValues     = std::unordered_map<GamepadAxisKey,       float,                      GamepadAxisKeyHash,       GamepadAxisKeyEqual>;
using GamepadButtonValues   = std::unordered_map<GamepadButtonKey,     bool,                       GamepadButtonKeyHash,     GamepadButtonKeyEqual>;

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

    // Gamepad registration
    bool registerGamepadAction(GamepadButtonTrigger trigger, Action action);
    bool deleteGamepadAction(GamepadButtonTrigger trigger);
    void registerGamepadAxis(int gamepadId, GamepadAxis axis, std::function<void(float)> callback);
    void registerGamepadConnected(std::function<void(int)> callback);
    void registerGamepadDisconnected(std::function<void(int)> callback);

    // Gamepad polling
    float            getGamepadAxis(int gamepadId, GamepadAxis axis) const;
    bool             getGamepadButton(int gamepadId, GamepadButton button) const;
    bool             isGamepadConnected(int gamepadId) const;
    std::vector<int> getConnectedGamepadIds() const;

    void processInputs();

    /// Clear all registered actions and callbacks.
    /// Call between manifest sessions before clearing Python globals.
    void clear();

    // Internal — keyboard/mouse
    void activate(KeyboardTrigger keyboardTrigger);
    void activateMouseButton(MouseButtonTrigger mouseButtonTrigger);
    void updateMousePosition(glm::vec2 position);
    void scrolled(glm::vec2 offset);

    // Internal — gamepad (called by CanvasImpl each frame)
    void activateGamepadButton(GamepadButtonTrigger trigger);
    void updateGamepadAxis(int gamepadId, GamepadAxis axis, float value);
    void gamepadConnected(int gamepadId);
    void gamepadDisconnected(int gamepadId);

private:
    TriggerActions mTriggerActions;
    ActiveActions mActiveActions;

    MouseTriggerActions mMouseTriggerActions;
    ActiveMouseActions mActiveMouseActions;
    std::optional<std::function<void(glm::vec2)>> mMouseMoveCallback;
    std::optional<std::function<void(glm::vec2)>> mMouseScrollCallback;
    glm::vec2 mMousePosition{0.0f, 0.0f};

    GamepadTriggerActions mGamepadTriggerActions;
    ActiveGamepadActions  mActiveGamepadActions;
    GamepadAxisCallbacks  mGamepadAxisCallbacks;
    GamepadAxisValues     mGamepadAxisValues;
    GamepadButtonValues   mGamepadButtonValues;
    std::optional<std::function<void(int)>> mGamepadConnectedCallback;
    std::optional<std::function<void(int)>> mGamepadDisconnectedCallback;
    std::unordered_set<int> mConnectedGamepads;
};


}