
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <new>
#include <cstdint>
#include <stdexcept>

namespace indexed {

/**
* @brief Arena assigning index to allocated memory blocks. It's supposed to be used by indexed::Allocator.
*
* Not thread-safe, use ArrayArenaMT if you need to access the same arena from multiple threads.
* Index type limits Arena's maximal capacity, further limits can be imposed by ArenaConfig.
* Can allocate objects of one size only, the size is fixed on the first object allocation.
* Real memory is allocated for the whole capacity via Alloc on the first object allocation,
* it's never released until the Arena destructor or freeMemory() is called.
* Arena capacity must be set before the first allocation, it can be changed only after freeMemory().
* @tparam Index unsigned integer type used for pointer representation: uint16_t or uint32_t
* @tparam Alloc class responsible for memory buffer allocation, e.g. NewAlloc
*/
template <typename Index, typename Alloc>
class ArrayArena : public Alloc {
public:
    using IndexType = Index;

    static constexpr bool kIsArrayArenaMT = false;

    /**
    * @brief Create Arena
    * @param capacity capacity in objects
    * @param enableDelete see enableDelete()
    * @param alloc object of Alloc type
    */
    explicit ArrayArena(size_t capacity = 0, bool enableDelete = true, Alloc&& alloc = Alloc())
    : Alloc(std::move(alloc))
    , m_capacity(0)
    , m_elementSizeInIndex(0)
    , m_doDelete(enableDelete)
    , m_nextFree(0)
    , m_allocatedCount(0)
    , m_usedCapacity(0) {
        setCapacity(capacity);
    }

    ArrayArena(ArrayArena&&) = default;
    ArrayArena(const ArrayArena&) = delete;

    ArrayArena& operator=(ArrayArena&&) = default;
    ArrayArena& operator=(const ArrayArena&) = delete;

    /**
    * @brief start of allocated memory buffer
    */
    char* begin() const noexcept { return static_cast<char*>(Alloc::getPtr()); }

    /**
    * @brief end of allocated memory buffer
    */
    char* end() const noexcept { return begin() + elementSize() * m_capacity; }

    /**
    * @brief capacity of the Arena
    */
    size_t capacity() const noexcept { return m_capacity; }

    /**
    * @brief peek size ever reached (mostly for debug)
    */
    size_t usedCapacity() const noexcept { return m_usedCapacity; }

    /**
    * @brief number of alive objects = allocated - deallocated (mostly for debug)
    */
    size_t allocatedCount() const noexcept { return m_allocatedCount; }

    /**
    * @brief size of allocated memory objects in bytes
    */
    size_t elementSize() const noexcept { return m_elementSizeInIndex * sizeof(Index); }

    /**
    * @brief true if deletion is on, see enableDelete()
    */
    bool deleteIsEnabled() const noexcept { return m_doDelete; }

    /**
    * @brief get pointer of object by index
    * @param index index returned by the Arena allocate()
    */
    void* getElement(Index index) const noexcept { return getElementInt(index, elementSize()); }

    /**
    * @brief Enable/disable object deletion
    * When deletion is on index released after deallocate() is used in future allocate().
    * When deletion is off, a new index is always assigned, allocate()/deallocate() is faster,
    * but Arena may require more capacity.
    * @param enable true - deletion is on
    */
    void enableDelete(bool enable) noexcept { m_doDelete = enable; }

    /**
    * @brief set Arena capacity, must be done before the first allocation
    * @param capacity new capacity
    */
    void setCapacity(size_t capacity) {
        if (capacity >= (1u << (sizeof(Index) * 8 - 1))) {
            throw std::length_error("indexed::ArrayArena capacity is too big for Index type");
        }
        if (begin() != nullptr) {
            throw std::runtime_error("indexed::ArrayArena capacity must be set before allocation");
        }
        m_capacity = capacity;
    }

    /**
    * @brief Converts pointer to index
    * @param ptr pointer to element allocated with the Arena
    * @return index of the element in the Arena
    */
    Index pointer_to(const void* ptr) const noexcept {
        size_t offset = static_cast<const char*>(ptr) - begin();
        Index pos = uint32_t(offset / sizeof(Index)) / m_elementSizeInIndex;
        indexed_assert(elementSize() * pos == offset
            && "Attempt to create indexed::Pointer pointing inside an allocated Node, do you use iterator-> ?");
        return pos + 1;
    }

    /**
    * @brief Allocate object in the Arena
    * @param typeSize size of the object in bytes
    * @return index assigned to the allocated object
    */
    Index allocate(size_t typeSize) {
        indexed_assert((elementSize() == typeSize || elementSize() == 0)
            && "indexed::ArrayArena can't handle different-sized allocations");
        Index index = 0;
        if (m_nextFree != 0) {
            index = m_nextFree;
            void* outPtr = getElementInt(index, typeSize);
            m_nextFree = *static_cast<Index*>(outPtr);
        } else {
            if (m_usedCapacity == m_capacity) {
                throw std::bad_alloc();
            }
            if (begin() == nullptr) {
                indexed_assert(typeSize % sizeof(Index) == 0
                    && "indexed::ArrayArena elementSize must be multiple of Index size");
                Alloc::malloc(typeSize * m_capacity);
                m_elementSizeInIndex = typeSize / sizeof(Index);
            }
            ++m_usedCapacity;
            index = m_usedCapacity;
        }
        ++m_allocatedCount;
        return index;
    }

    /**
    * @brief Deallocate object allocated before with the Arena
    * @param index index of the object obtained in allocate()
    * @param typeSize size of the object in bytes
    */
    void deallocate(Index index, size_t typeSize) noexcept {
        --m_allocatedCount;
        if (m_allocatedCount == 0) {
            reset();
            return;
        }
        if (m_doDelete) {
            void* ptr = getElementInt(index, typeSize);
            *static_cast<Index*>(ptr) = m_nextFree;
            m_nextFree = index;
        }
    }

    /**
    * @brief Reset container to the "new" state, the memory isn't released, it's reused.
    * NOTE You should be sure that there are no allocated objects or they will never be used.
    */
    void reset() noexcept {
        indexed_warning(m_allocatedCount == 0 && "ArrayArena::reset() is called while there are allocated objects");
        m_nextFree = 0;
        m_allocatedCount = 0;
        m_usedCapacity = 0;
    }

    /**
    * @brief Reset the Arena and release its memory. New memory will be allocated on allocate().
    * NOTE You should be sure that there are no allocated objects or they will never be used.
    */
    void freeMemory() noexcept {
        m_elementSizeInIndex = 0;
        reset();
        Alloc::free();
    }

    ~ArrayArena() noexcept {
        indexed_warning(m_allocatedCount == 0 && "ArrayArena is destructed while there are allocated objects");
    }

private:
    void* getElementInt(Index index, size_t elementSize) const noexcept {
        indexed_assert(index > 0 && index <= m_usedCapacity && "indexed::Pointer is invalid");
        return begin() + elementSize * (index - 1);
    }

    Index m_capacity;
    uint16_t m_elementSizeInIndex; // size / sizeof(Index)
    bool m_doDelete;
    Index m_nextFree; // slist of free elements
    Index m_allocatedCount;
    Index m_usedCapacity;
};

}
