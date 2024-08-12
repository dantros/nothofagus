#pragma once

namespace Nothofagus
{

class IndexFactory
{
public:
    IndexFactory() = default;

    std::size_t generateIndex()
    {
        ++mLastIndex;
        return mLastIndex;
    }

private:
    std::size_t mLastIndex;
};

}