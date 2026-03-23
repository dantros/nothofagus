
#include "controller.h"
#include <algorithm>

namespace Nothofagus
{

bool Controller::registerAction(KeyboardTrigger keyboardTrigger, Action action)
{
    if (mTriggerActions.count(keyboardTrigger) == 1)
        return false;

    mTriggerActions.emplace(keyboardTrigger, action);
    return true;
}

bool Controller::deleteAction(KeyboardTrigger keyboardTrigger)
{
    return mTriggerActions.erase(keyboardTrigger) == 1;
}

bool Controller::registerMouseAction(MouseButtonTrigger mouseButtonTrigger, Action action)
{
    if (mMouseTriggerActions.count(mouseButtonTrigger) == 1)
        return false;

    mMouseTriggerActions.emplace(mouseButtonTrigger, action);
    return true;
}

bool Controller::deleteMouseAction(MouseButtonTrigger mouseButtonTrigger)
{
    return mMouseTriggerActions.erase(mouseButtonTrigger) == 1;
}

void Controller::registerMouseMove(std::function<void(glm::vec2)> callback)
{
    mMouseMoveCallback = std::move(callback);
}

glm::vec2 Controller::getMousePosition() const
{
    return mMousePosition;
}

void Controller::processInputs()
{
    // processing all the keyboard inputs since the last update
    while (!mActiveActions.empty())
    {
        KeyboardTrigger keyboardTrigger = mActiveActions.front();
        mActiveActions.pop_front();

        if (mTriggerActions.count(keyboardTrigger) == 0)
            continue;

        auto& action = mTriggerActions.at(keyboardTrigger);
        action();
    }

    // processing all the mouse button inputs since the last update
    while (!mActiveMouseActions.empty())
    {
        MouseButtonTrigger mouseButtonTrigger = mActiveMouseActions.front();
        mActiveMouseActions.pop_front();

        if (mMouseTriggerActions.count(mouseButtonTrigger) == 0)
            continue;

        auto& action = mMouseTriggerActions.at(mouseButtonTrigger);
        action();
    }

    // processing all the gamepad button inputs since the last update
    while (!mActiveGamepadActions.empty())
    {
        GamepadButtonTrigger gamepadTrigger = mActiveGamepadActions.front();
        mActiveGamepadActions.pop_front();

        if (mGamepadTriggerActions.count(gamepadTrigger) == 0)
            continue;

        auto& action = mGamepadTriggerActions.at(gamepadTrigger);
        action();
    }
}

void Controller::activate(KeyboardTrigger keyboardTrigger)
{
    mActiveActions.push_back(keyboardTrigger);
}

void Controller::activateMouseButton(MouseButtonTrigger mouseButtonTrigger)
{
    mActiveMouseActions.push_back(mouseButtonTrigger);
}

void Controller::updateMousePosition(glm::vec2 position)
{
    mMousePosition = position;

    if (mMouseMoveCallback.has_value())
        (*mMouseMoveCallback)(mMousePosition);
}

void Controller::registerMouseScroll(std::function<void(glm::vec2)> callback)
{
    mMouseScrollCallback = std::move(callback);
}

void Controller::scrolled(glm::vec2 offset)
{
    if (mMouseScrollCallback.has_value())
        (*mMouseScrollCallback)(offset);
}

// ---------------------------------------------------------------------------
// Gamepad implementation
// ---------------------------------------------------------------------------

bool Controller::registerGamepadAction(GamepadButtonTrigger trigger, Action action)
{
    if (mGamepadTriggerActions.count(trigger) == 1)
        return false;

    mGamepadTriggerActions.emplace(trigger, action);
    return true;
}

bool Controller::deleteGamepadAction(GamepadButtonTrigger trigger)
{
    return mGamepadTriggerActions.erase(trigger) == 1;
}

void Controller::registerGamepadAxis(int gamepadId, GamepadAxis axis, std::function<void(float)> callback)
{
    mGamepadAxisCallbacks[{gamepadId, axis}] = std::move(callback);
}

void Controller::registerGamepadConnected(std::function<void(int)> callback)
{
    mGamepadConnectedCallback = std::move(callback);
}

void Controller::registerGamepadDisconnected(std::function<void(int)> callback)
{
    mGamepadDisconnectedCallback = std::move(callback);
}

float Controller::getGamepadAxis(int gamepadId, GamepadAxis axis) const
{
    auto it = mGamepadAxisValues.find({gamepadId, axis});
    return it != mGamepadAxisValues.end() ? it->second : 0.0f;
}

bool Controller::getGamepadButton(int gamepadId, GamepadButton button) const
{
    auto it = mGamepadButtonValues.find({gamepadId, button});
    return it != mGamepadButtonValues.end() ? it->second : false;
}

bool Controller::isGamepadConnected(int gamepadId) const
{
    return mConnectedGamepads.count(gamepadId) > 0;
}

std::vector<int> Controller::getConnectedGamepadIds() const
{
    std::vector<int> ids(mConnectedGamepads.begin(), mConnectedGamepads.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

void Controller::activateGamepadButton(GamepadButtonTrigger trigger)
{
    mGamepadButtonValues[{trigger.gamepadId, trigger.button}] =
        (trigger.trigger == DiscreteTrigger::Press);
    mActiveGamepadActions.push_back(trigger);
}

void Controller::updateGamepadAxis(int gamepadId, GamepadAxis axis, float value)
{
    GamepadAxisKey key{gamepadId, axis};
    auto it = mGamepadAxisValues.find(key);
    float previous = (it != mGamepadAxisValues.end()) ? it->second : 0.0f;
    mGamepadAxisValues[key] = value;

    if (value != previous)
    {
        auto callbackIt = mGamepadAxisCallbacks.find(key);
        if (callbackIt != mGamepadAxisCallbacks.end())
            callbackIt->second(value);
    }
}

void Controller::gamepadConnected(int gamepadId)
{
    mConnectedGamepads.insert(gamepadId);
    if (mGamepadConnectedCallback.has_value())
        (*mGamepadConnectedCallback)(gamepadId);
}

void Controller::gamepadDisconnected(int gamepadId)
{
    mConnectedGamepads.erase(gamepadId);
    if (mGamepadDisconnectedCallback.has_value())
        (*mGamepadDisconnectedCallback)(gamepadId);
}

}