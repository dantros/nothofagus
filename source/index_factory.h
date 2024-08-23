#pragma once

namespace Nothofagus
{

class IndexFactory
{
public:
    IndexFactory() : mLastIndex{0}
    {}

    std::size_t generateIndex()
    {
        ++mLastIndex;
        return mLastIndex-1;
    }

private:
    std::size_t mLastIndex;
};

}