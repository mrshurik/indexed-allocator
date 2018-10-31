
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>
#include <indexed/Pointer.h>

#include <type_traits>
#include <memory>

// "hardcode" bucket type used in boost::unordered_* containers
namespace boost { namespace unordered { namespace detail {

template <typename Type>
struct bucket;

} } }

namespace indexed {

template <typename Type, typename ArenaConfig>
class Allocator;

namespace detail {

template <typename Type>
struct IsUnorderedBucket {
    static constexpr bool value = false;
};

template <typename Type>
struct IsUnorderedBucket<boost::unordered::detail::bucket<Type>> {
    static constexpr bool value = true;
};

// needed in order to provide cast from Allocator to std::allocator
template <typename Type>
class StdAllocator : public std::allocator<Type> {
public:
    StdAllocator() = default;

    template <typename Type2, typename ArenaConfig>
    StdAllocator(const Allocator<Type2, ArenaConfig>&) noexcept
    : StdAllocator() {}
};

template <typename ArenaConfig>
class VoidAllocator {
public:
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    template <typename Type>
    struct rebind {
        using other = typename std::conditional<IsUnorderedBucket<Type>::value,
                                    typename ArenaConfig::template ArrayAllocator<Type>,
                                    Allocator<Type, ArenaConfig>>::type;
    };

protected:
    using ArenaPtr = typename ArenaConfig::ArenaPtr;

    VoidAllocator() noexcept
    : m_arena(ArenaConfig::defaultArena()) {
        ArenaConfig::setContainer(this);
    }

    VoidAllocator(const ArenaPtr& arena) noexcept
    : m_arena(arena) {}

    ArenaPtr m_arena;
};

}

/**
* @brief C++11 STL allocator class using indexed Pointer and Arena for allocation.
* NOTE Can allocate only 1 element, can't allocate array of elements.
* @tparam Type type of allocated object
* @tparam ArenaConfig Arena config class with ArenaConfigInterface (see doc)
*/
template <typename Type, typename ArenaConfig>
class Allocator : public detail::VoidAllocator<ArenaConfig> {
private:
    using Base = detail::VoidAllocator<ArenaConfig>;
    using ArenaPtr = typename Base::ArenaPtr;

public:
    using value_type = Type;
    using pointer = Pointer<Type, ArenaConfig>;

    /**
    * @brief Create allocator using Arena obtained via ArenaConfig::defaultArena()
    */
    Allocator() = default;

    /**
    * @brief Create allocator with given Arena
    * @param arena Pointer to Arena used for allocation (see doc)
    */
    explicit Allocator(const ArenaPtr& arena) noexcept
    : Base(arena) {}

    Allocator(const Allocator& other) noexcept
    : Base(other) {
        ArenaConfig::setContainer(this);
    }

    template <typename Type2>
    Allocator(const Allocator<Type2, ArenaConfig>& alloc) noexcept
    : Base(alloc) {
        ArenaConfig::setContainer(this);
    }

    Allocator& operator=(const Allocator& other) noexcept {
        this->m_arena = other.m_arena;
        ArenaConfig::setContainer(this);
        return *this;
    }

    pointer allocate(size_t n) const {
        indexed_assert(n == 1 && "indexed::Allocator can't allocate/deallocate array");
        return pointer(this->m_arena->allocate(sizeof(Type)));
    }

    void deallocate(const pointer& ptr, size_t) const noexcept {
        this->m_arena->deallocate(ptr.get(), sizeof(Type));
    }

    friend
    bool operator==(const Allocator& left, const Allocator& right) noexcept {
        return left.m_arena == right.m_arena;
    }

    friend
    bool operator!=(const Allocator& left, const Allocator& right) noexcept {
        return !(left == right);
    }
};

}
