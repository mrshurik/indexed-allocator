
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <new>
#include <cstdint>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <type_traits>

namespace indexed {

namespace detail {

template <typename Type>
struct DoubleWidth;

template <>
struct DoubleWidth<uint32_t> {
    using type = uint64_t;
};

template <>
struct DoubleWidth<uint16_t> {
    using type = uint32_t;
};

template <typename Arena>
class LockFreeSList {
private:
    using IndexType = typename Arena::IndexType;
    using DoubleIndex = typename DoubleWidth<IndexType>::type;

    static constexpr uint8_t kBits = sizeof(IndexType) * 8;

public:
    LockFreeSList() noexcept
    : m_head(0) {}

    void reset() noexcept { m_head = 0; }

    IndexType listLength(Arena& arena) const noexcept {
        IndexType len = 0;
        IndexType next = IndexType(m_head);
        while (next != 0) {
            ++len;
            void* freeSlot = arena.getElement(next);
            next = *static_cast<IndexType*>(freeSlot);
        }
        return len;
    }

    IndexType pull(Arena& arena) noexcept {
        DoubleIndex headD = m_head;
        for(; ;) {
            IndexType head = IndexType(headD);
            if (head == 0) {
                return 0;
            }
            void* freeSlot = arena.getElement(head);
            IndexType futureHead = *static_cast<IndexType*>(freeSlot);
            DoubleIndex futureHeadD = toDoubleIndex(futureHead, toStamp(headD) + 1);
            if (m_head.compare_exchange_strong(headD, futureHeadD)) {
                return head;
            }
        }
    }

    void push(IndexType free, Arena& arena) noexcept {
        void* freeSlot = arena.getElement(free);
        DoubleIndex headD = m_head;
        for(; ;) {
            *static_cast<IndexType*>(freeSlot) = IndexType(headD);
            DoubleIndex futureHeadD = toDoubleIndex(free, toStamp(headD) + 1);
            if (m_head.compare_exchange_strong(headD, futureHeadD)) {
                break;
            }
        }
    }

private:
    static DoubleIndex toStamp(DoubleIndex indexD) noexcept { return indexD >> kBits; }

    static DoubleIndex toDoubleIndex(IndexType index, DoubleIndex stamp) noexcept { return (stamp << kBits) | index; }

    std::atomic<DoubleIndex> m_head;
};

}

/**
* @brief Arena assigning index to allocated memory blocks. It's supposed to be used by indexed::Allocator.
*
* Thread-safe version of ArrayArena. Consider ArrayArena if you don't access the same Arena in MT mode.
* Index type limits Arena's maximal capacity, further limits can be imposed by ArenaConfig.
* Can allocate objects of one size only, the size is fixed on the first object allocation.
* Real memory is allocated for the whole capacity via Alloc on the first object allocation,
* it's never released until the Arena destructor or freeMemory() is called.
* Arena capacity must be set before the first allocation, it can be changed only after freeMemory().
*
* ArrayArenaMT avoids unnecessary synchronization for performance. It assumes that the first call in every
* thread is allocate(), or at least, the first call to getElement() or pointer_to() in a thread A happens
* after allocate() has been already called in a thread B. This assumption is met when the Arena is
* accessed via Allocator class.
*
* @tparam Index unsigned integer type used for pointer representation: uint16_t or uint32_t
* @tparam Alloc class responsible for memory buffer allocation, e.g. NewAlloc
*/
template <typename Index, typename Alloc>
class ArrayArenaMT : public Alloc {
    static_assert(std::is_same<Index, uint16_t>::value ||
                std::is_same<Index, uint32_t>::value, "Index must be uint16_t or uint32_t");

public:
    using IndexType = Index;

    static constexpr bool kIsArrayArenaMT = true;

    /**
    * @brief Create Arena
    * @param capacity capacity in objects
    * @param enableDelete see enableDelete()
    * @param alloc object of Alloc type
    */
    explicit ArrayArenaMT(size_t capacity = 0, bool enableDelete = true, Alloc&& alloc = Alloc())
    : Alloc(std::move(alloc))
    , m_capacity(0)
    , m_elementSizeInIndex(0)
    , m_doDelete(enableDelete)
    , m_isAllocError(false)
    , m_allocMutex()
    , m_freeList()
    , m_usedCapacity(0) {
        setCapacity(capacity);
    }

    ArrayArenaMT(const ArrayArenaMT&) = delete;

    ArrayArenaMT& operator=(const ArrayArenaMT&) = delete;

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
    * @brief peek size ever reached, not MT-safe (mostly for debug)
    */
    size_t usedCapacity() const noexcept { return m_usedCapacity; }

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
    * @brief set Arena capacity, must be done before the first allocation.
    * NOTE The method is not MT-safe, read freeMemory() for details.
    * @param capacity new capacity
    */
    void setCapacity(size_t capacity) {
        if (capacity >= (1u << (sizeof(Index) * 8 - 1))) {
            throw std::length_error("indexed::ArrayArenaMT capacity is too big for Index type");
        }
        if (begin() != nullptr) {
            throw std::runtime_error("indexed::ArrayArenaMT capacity must be set before allocation");
        }
        m_capacity = Index(capacity);
    }

    /**
    * @brief Converts pointer to index
    * @param ptr pointer to element allocated with the Arena
    * @return index of the element in the Arena
    */
    Index pointer_to(const void* ptr) const noexcept {
        size_t offset = static_cast<const char*>(ptr) - begin();
        Index pos = Index(uint32_t(offset / sizeof(Index)) / m_elementSizeInIndex);
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
        indexed_assert((elementSize() == 0 || elementSize() == typeSize)
            && "indexed::ArrayArenaMT can't handle different-sized allocations");
        // NOTE should first check for m_doDelete?
        Index index = m_freeList.pull(*this);
        if (index == 0) {
            Index futureCapacity = ++m_usedCapacity;
            try {
                if (futureCapacity > m_capacity) {
                    throw std::bad_alloc();
                }
                if (begin() == nullptr) {
                    allocateBuffer(typeSize);
                }
            } catch (const std::exception&) {
                --m_usedCapacity;
                throw;
            }
            index = futureCapacity;
        }
        return index;
    }

    /**
    * @brief Deallocate object allocated before with the Arena
    * @param index index of the object obtained in allocate()
    */
    void deallocate(Index index, size_t) noexcept {
        if (m_doDelete) {
            m_freeList.push(index, *this);
        }
    }

    /**
    * @brief Reset container to the "new" state with no allocated objects.
    * The memory isn't released, it's reused.
    * NOTE You should be sure that there are no allocated objects or they will never be used.
    * NOTE The method is not MT-safe, read freeMemory() for details.
    */
    void reset() noexcept {
        indexed_warning(m_usedCapacity == m_freeList.listLength(*this)
            && "ArrayArenaMT::reset() is called while there are allocated objects");
        m_freeList.reset();
        m_usedCapacity = 0;
    }

    /**
    * @brief Reset the Arena and release its memory. New memory will be allocated on allocate().
    * NOTE You should be sure that there are no allocated objects or they will never be used.
    * NOTE The method is not MT-safe, it should be called only inside a critical section for
    *      the threads sharing the Arena (or once they've joined in one thread).
    */
    void freeMemory() noexcept {
        Alloc::free();
        m_elementSizeInIndex = 0;
        m_isAllocError = false;
        reset();
    }

    ~ArrayArenaMT() noexcept {
        indexed_warning(m_usedCapacity == m_freeList.listLength(*this)
            && "ArrayArenaMT is destructed while there are allocated objects");
    }

private:
    void* getElementInt(Index index, size_t elementSize) const noexcept {
        indexed_assert(index > 0 && index <= m_usedCapacity && "indexed::Pointer is invalid");
        return begin() + elementSize * (index - 1);
    }

    void allocateBuffer(size_t typeSize) {
        std::lock_guard<std::mutex> guard(m_allocMutex);
        if (m_isAllocError) {
            throw std::bad_alloc();
        }
        if (begin() != nullptr) {
            return;
        }
        indexed_assert(typeSize % sizeof(Index) == 0
            && "indexed::ArrayArenaMT elementSize must be multiple of Index size");
        try {
            Alloc::malloc(typeSize * m_capacity);
        } catch (const std::exception&) {
            m_isAllocError = true;
            throw;
        }
        m_elementSizeInIndex = decltype(m_elementSizeInIndex)(typeSize / sizeof(Index));
        indexed_assert(m_elementSizeInIndex == typeSize / sizeof(Index)
            && "indexed::ArrayArenaMT elementSize is too large");
    }

    Index m_capacity;
    uint16_t m_elementSizeInIndex; // size / sizeof(Index)
    bool m_doDelete;
    bool m_isAllocError;
    std::mutex m_allocMutex;
    detail::LockFreeSList<ArrayArenaMT> m_freeList;
    std::atomic<Index> m_usedCapacity;
};

}
