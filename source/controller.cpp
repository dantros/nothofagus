
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

void Controller::processInputs()
{
    // processing all the inputs since the last update
    while (!mActiveActions.empty())
    {
        // extracting the first trigger in the queue
        KeyboardTrigger keyboardTrigger = mActiveActions.front();
        mActiveActions.pop_front();

        // do we have an action for it?
        if (mTriggerActions.count(keyboardTrigger) == 0)
            continue;

        // At this point, we are sure we have an action.
        auto& action = mTriggerActions.at(keyboardTrigger);

        // executing it
        action();
    }
}

void Controller::activate(KeyboardTrigger keyboardTrigger)
{
    mActiveActions.push_back(keyboardTrigger);
}

}