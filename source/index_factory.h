#pragma once

namespace Nothofagus
{

/**
 * @brief A factory class that generates unique indices.
 * 
 * The `IndexFactory` class is responsible for generating unique indices. Each time `generateIndex()` is called, it returns a new index that is incremented from the last generated index.
 * This is useful to ensure that each element in a container can be uniquely identified by an index.
 */
class IndexFactory
{
public:
    /**
     * @brief Default constructor for IndexFactory.
     * 
     * Initializes the index generator with the last index set to 0.
     */
    IndexFactory() : mLastIndex{0}
    {}

    /**
     * @brief Generates and returns a unique index.
     * 
     * Increments the `mLastIndex` and returns the previous value as the unique index.
     * Each call to this method returns a new unique index.
     * 
     * @return A unique index as a `std::size_t`.
     */
    std::size_t generateIndex()
    {
        ++mLastIndex;
        return mLastIndex-1;
    }

private:
    std::size_t mLastIndex;
};

}