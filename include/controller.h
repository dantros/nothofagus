#pragma once

#include <functional>
#include <unordered_map>
#include <deque>
#include "keyboard.h"

namespace Nothofagus
{

using Action = std::function<void()>;
using KeyActions = std::unordered_map<Key, Action>;
using ActiveKeys = std::deque<Key>;

/* \brief true returned on successful operation. */
class Controller
{
public:
    Controller() = default;

    bool onKeyPress(Key key, Action action);

    bool deleteAction(Key key);

    void processInputs();

    void press(Key key);

private:
    KeyActions mKeyActions;
    ActiveKeys mActiveKeys;
};


}