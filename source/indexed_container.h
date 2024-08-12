#pragma once

#include <unordered_map>
#include "check.h"

namespace Nothofagus
{

template <typename ElementType>
class IndexedContainer
{
public:
    IndexedContainer(){}

    std::size_t add(const ElementType& element)
    {
        std::size_t newIndex = mIndexFactory.generateIndex();
        mElements.insert({newIndex, element});
        return newIndex;
    }

    void remove(const std::size_t elementId)
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        mElements.erase(elementId);
    }

    ElementType& at(const std::size_t elementId)
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        return mElements.at(elementId);
    }

    const ElementType& at(const std::size_t elementId) const
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        return mElements.at(elementId);
    }

private:
    std::unordered_map<std::size_t, ElementType> mElements;
    IndexFactory mIndexFactory;
};

}
