
#include "controller.h"

namespace Nothofagus
{

bool Controller::onKeyPress(Key key, Action action)
{
    if (mKeyActions.count(key) == 1)
        return false;

    mKeyActions.emplace(key, action);
    return true;
}

bool Controller::deleteAction(Key key)
{
    return mKeyActions.erase(key) == 1;
}

void Controller::processInputs()
{
    // processing all the inputs since the last update
    while (!mActiveKeys.empty())
    {
        // extracting the first key in the queue
        Key key = mActiveKeys.front();
        mActiveKeys.pop_front();

        // do we have an action for it?
        if (mKeyActions.count(key) == 0)
            continue;

        // At this point, we are sure we have an action for key.
        auto& action = mKeyActions.at(key);

        // executing it
        action();
    }
}

void Controller::press(Key key)
{
    mActiveKeys.push_back(key);
}

}