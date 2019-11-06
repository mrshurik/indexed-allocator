
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <cstddef>

namespace indexed {

namespace detail {

template <typename Type>
class StdAllocator;

template <typename ArenaType, typename ConfigClass>
class ConfigStoreStatic {
public:
    using Arena = ArenaType;

protected:
    /**
    * @brief Arena used by Pointer and Allocator classes
    */
    static ArenaType* arena;

    /**
    * @brief Pointer to the highest address of the thread's stack
    */
    static void* stackTop;
};

template <typename ArenaType, typename ConfigClass>
ArenaType* ConfigStoreStatic<ArenaType, ConfigClass>::arena = nullptr;

template <typename ArenaType, typename ConfigClass>
void* ConfigStoreStatic<ArenaType, ConfigClass>::stackTop = nullptr;

template <typename ArenaType, typename ConfigClass>
class ConfigStorePerThread {
public:
    using Arena = ArenaType;

protected:
    /**
    * @brief Arena used by Pointer and Allocator classes, one or per thread
    */
    static thread_local ArenaType* arena;

    /**
    * @brief Pointer to the highest address of the thread's stack (per thread)
    */
    static thread_local void* stackTop;
};

template <typename ArenaType, typename ConfigClass>
thread_local ArenaType* ConfigStorePerThread<ArenaType, ConfigClass>::arena = nullptr;

template <typename ArenaType, typename ConfigClass>
thread_local void* ConfigStorePerThread<ArenaType, ConfigClass>::stackTop = nullptr;

template <typename ConfigStore,
          size_t kNodeAlignment = sizeof(typename ConfigStore::Arena::IndexType)>
class SingleArenaConfig : public ConfigStore {
public:
    using Arena = typename ConfigStore::Arena;

    // API for Pointer
    using IndexType = typename Arena::IndexType;

    static_assert(sizeof(IndexType) >= 2, "IndexType uint8_t is not supported (not safe)");

private:
    static constexpr IndexType kOnStackFlag   = 1u << (sizeof(IndexType) * 8 - 1);
    static constexpr ptrdiff_t kMaxStackSize  = 2 * 1024 * 1024;

    using ConfigStore::arena;
    using ConfigStore::stackTop;

public:
    // { API for Pointer
    static void* getElement(IndexType index) noexcept {
        return ((index & kOnStackFlag) != 0)
                ? static_cast<char*>(stackTop) - kNodeAlignment * (index ^ kOnStackFlag)
                : arena->getElement(index);
    }

    static IndexType pointer_to(const void* ptr) noexcept {
        ptrdiff_t stackOffset = static_cast<char*>(stackTop) - static_cast<const char*>(ptr);
        IndexType index = 0;
        if ((stackOffset >= 0) && (stackOffset < kMaxStackSize)) {
            size_t offset = size_t(stackOffset) / kNodeAlignment;
            indexed_assert(offset < kOnStackFlag && "object is too deep in stack for indexed::IndexType");
            indexed_assert(offset * kNodeAlignment == size_t(stackOffset) && "object alignment is wrong, check indexed::ArenaConfig::kAlignment");
            index = IndexType(offset) | kOnStackFlag;
        } else {
            index = arena->pointer_to(ptr);
        }
        return index;
    }
    // }

    // { API for user

    /**
     * @brief No op for SingleArenaConfig
     */
    static void setContainer(void* containerPtr) noexcept { }

    /**
     * @brief Always nullptr for SingleArenaConfig
     */
    static void* getContainer() noexcept { return nullptr; }

    /**
     * @brief Set Arena used by Pointer and Allocator classes
     */
    static void setArena(Arena* arenaPtr) noexcept { arena = arenaPtr; }

    /**
     * @brief Get Arena used by Pointer and Allocator classes
     */
    static Arena* getArena() noexcept { return arena; }

    /**
     * @brief Set pointer to the highest address of the thread's stack
     */
    static void setStackTop(void* stackTopPtr) noexcept { stackTop = stackTopPtr; }

    /**
     * @brief Get pointer to the highest address of the thread's stack
     */
    static void* getStackTop() noexcept { return stackTop; }

    // }

    // { API for Allocator
    using ArenaPtr = typename ConfigStore::Arena*;

    // SingleArenaConfig doesn't need to track the container pointer
    static constexpr bool kAssignContainerFollowingAllocator = false;

    template <typename Type>
    using ArrayAllocator = StdAllocator<Type>;

    static ArenaPtr defaultArena() noexcept { return arena; }

    template <typename Node>
    static IndexType arenaToPtrIndex(IndexType fromArena) noexcept { return fromArena; }

    template <typename Node>
    static IndexType ptrToArenaIndex(IndexType fromPtr) noexcept { return fromPtr; }

    // }
};

}

/**
* @brief ArenaConfig working with one Arena, single thread, stack or Arena as Node location support.
*
* NOTE Access to Pointers / Allocators defined with the config must be done from the same thread.
* NOTE The case when a Node is located on heap, but not in Arena (e.g. in Container) is not supported.
* NOTE The case when Arena storage is allocated on the stack is not supported.
* All Pointers and Allocators defined with the config will use the Arena pointer in the config.
* The pointer can be changed only when there are no Pointers / Allocators created with the old Arena.
* @tparam ArenaType type of Arena it works with
* @tparam ConfigClass user-defined class inherited from the config (unique for the used Allocator type)
* @tparam kNodeAlignment (optional) alignment in bytes for any Pointer using this config
*/
template <typename ArenaType, typename ConfigClass, size_t ...kNodeAlignment>
struct SingleArenaConfigStatic :
    detail::SingleArenaConfig<detail::ConfigStoreStatic<ArenaType, ConfigClass>, kNodeAlignment...> {};

/**
* @brief ArenaConfig allowing one Arena per thread, many threads, stack or Arena(s) as Node location support.
*
* It's a MT-safe version of SingleArenaConfigStatic with thread local variables arena, stackTop.
* NOTE The case when a Node is located on heap, but not in Arena (e.g. in Container) is not supported.
* NOTE The case when Arena storage is allocated on the stack is not supported.
* All Pointers and Allocators defined with the config will use the Arena pointer in the config.
* The pointer can be changed only when there are no Pointers/Allocators created with the old Arena.
* @tparam ArenaType type of Arena it works with
* @tparam ConfigClass user-defined class inherited from the config (unique for the used Allocator type)
* @tparam kNodeAlignment (optional) alignment in bytes for any Pointer using this config
*/
template <typename ArenaType, typename ConfigClass, size_t ...kNodeAlignment>
struct SingleArenaConfigPerThread :
    detail::SingleArenaConfig<detail::ConfigStorePerThread<ArenaType, ConfigClass>, kNodeAlignment...> {};

}
