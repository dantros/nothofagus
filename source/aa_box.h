#pragma once

namespace Nothofagus
{

struct AABox
{
    int x, y, width, height;

    bool contains(int x_, int y_) const
    {
        auto inRange = [](int value, int lo, int hi) { return lo <= value && value <= hi; };
        return inRange(x_, x, x + width) && inRange(y_, y, y + height);
    }
};

}
