#pragma once

namespace Nothofagus
{

template <typename IteratorType>
class IteratorT
{
public:
    // STL iterator concept compatibiltiy for C++20
    using value_type = IteratorType::value_type;
    using difference_type = IteratorType::difference_type;
    using pointer = IteratorType::pointer;
    using reference = IteratorType::reference;
    using iterator_category = std::random_access_iterator_tag;

    IteratorT(IteratorType mapIterator) :
        mMapIterator(mapIterator)
    {
    }

    reference operator*()
    {
        return *mMapIterator;
    }

    // pre-increment
    IteratorT& operator++()
    {
        ++mMapIterator;
        return *this;
    }

    bool operator==(const IteratorT& rhs) const
    {
        return mMapIterator == rhs.mMapIterator;
    }

    bool operator!=(const IteratorT& rhs) const
    {
        return not operator==(rhs);
    }

    IteratorT operator+(difference_type n) const
    {
        IteratorType newMapIterator = mMapIterator + n;
        return Iterator(newMapIterator);
    }

    IteratorT& operator+=(difference_type n) const
    {
        mMapIterator += n;
        return *this;
    }

    bool operator<(const IteratorT& rhs) const
    {
        return mMapIterator < rhs.mMapIterator;
    }

private:
    IteratorType mMapIterator;
};

}