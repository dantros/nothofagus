#pragma once

#include <functional>
#include <unordered_map>
#include <deque>
#include <bitset>
#include <tuple>
#include "keyboard.h"

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

using Action = std::function<void()>;
using TriggerActions = std::unordered_map<KeyboardTrigger, Action, KeyboardTriggerHash, KeyboardTriggerEqual>;
using ActiveActions = std::deque<KeyboardTrigger>;

/* \brief true returned on successful operation. */
class Controller
{
public:
    Controller() = default;

    bool registerAction(KeyboardTrigger keyboardTrigger, Action action);

    bool deleteAction(KeyboardTrigger keyboardTrigger);

    void processInputs();

    void activate(KeyboardTrigger keyboardTrigger);

private:
    TriggerActions mTriggerActions;
    ActiveActions mActiveActions;
};


}