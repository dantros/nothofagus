
#include "controller.h"

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

}