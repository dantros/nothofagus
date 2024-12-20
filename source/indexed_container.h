#pragma once

#include <unordered_map>
#include "check.h"
#include "index_factory.h"

namespace Nothofagus
{

// TODO convert key from size_t to something castable to size_t via concepts
/**
 * @brief A container class for storing elements indexed by a generated index.
 * 
 * This template class stores elements of type `ElementType` and provides methods for adding, removing, and accessing them by an automatically generated index.
 * The class uses an internal map to store elements and ensure that each element has a unique index.
 * 
 * @tparam ElementType The type of elements to be stored in the container.
 */
template <typename ElementType>
class IndexedContainer
{
public:
    /**
     * @brief Default constructor for IndexedContainer.
     * 
     * Initializes an empty container with no elements.
     */
    IndexedContainer(){}

    /**
     * @brief Adds an element to the container.
     * 
     * Generates a unique index for the element and stores it in the container.
     * 
     * @param element The element to be added to the container.
     * @return The generated index for the added element.
     */
    std::size_t add(const ElementType& element)
    {
        std::size_t newIndex = mIndexFactory.generateIndex();
        mElements.insert({newIndex, element});
        return newIndex;
    }

    /**
     * @brief Removes an element from the container by its index.
     * 
     * Erases the element associated with the given index from the container.
     * 
     * @param elementId The index of the element to be removed.
     * @throws Throws an exception if the elementId does not exist in the container.
     */
    void remove(const std::size_t elementId)
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        mElements.erase(elementId);
    }

    /**
     * @brief Accesses an element by its index.
     * 
     * Returns a reference to the element associated with the given index.
     * 
     * @param elementId The index of the element to be accessed.
     * @return A reference to the element.
     * @throws Throws an exception if the elementId does not exist in the container.
     */
    ElementType& at(const std::size_t elementId)
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        return mElements.at(elementId);
    }

    /**
     * @brief Accesses a constant element by its index.
     * 
     * Returns a const reference to the element associated with the given index.
     * 
     * @param elementId The index of the element to be accessed.
     * @return A const reference to the element.
     * @throws Throws an exception if the elementId does not exist in the container.
     */
    const ElementType& at(const std::size_t elementId) const
    {
        debugCheck(mElements.contains(elementId), "elementId is not included in IndexedContainer");
        return mElements.at(elementId);
    }

    /**
     * @brief Checks if an element exists in the container by its index.
     * 
     * @param elementId The index of the element to be checked.
     * @return `true` if the element exists in the container, `false` otherwise.
     */
    bool contains(const std::size_t elementId) const
    {
        return mElements.contains(elementId);
    }

    /**
     * @brief Returns the number of elements in the container.
     * 
     * @return The number of elements in the container.
     */
    std::size_t size() const
    {
        return mElements.size();
    }

    /**
     * @brief Returns a constant reference to the underlying map of elements.
     * 
     * This map is keyed by the generated index for each element.
     * 
     * @return A const reference to the map of elements.
     */
    const std::unordered_map<std::size_t, ElementType>& map() const
    {
        return mElements;
    }

    /**
     * @brief Returns a reference to the underlying map of elements.
     * 
     * This map is keyed by the generated index for each element.
     * 
     * @return A reference to the map of elements.
     */
    std::unordered_map<std::size_t, ElementType>& map()
    {
        return mElements;
    }

private:
    std::unordered_map<std::size_t, ElementType> mElements; ///< Map of elements indexed by a generated index
    IndexFactory mIndexFactory; ///< Factory for generating unique indexes for elements
};

}
