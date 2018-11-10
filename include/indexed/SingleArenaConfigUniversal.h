
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

namespace indexed {

namespace detail {

template <typename Type>
class StdAllocator;

template <typename ArenaType, typename ConfigClass>
class ConfigStoreUniversalStatic {
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

    /**
    * @brief Pointer to the start of the Container object
    */
    static void* container;
};

template <typename ArenaType, typename ConfigClass>
ArenaType* ConfigStoreUniversalStatic<ArenaType, ConfigClass>::arena = nullptr;

template <typename ArenaType, typename ConfigClass>
void* ConfigStoreUniversalStatic<ArenaType, ConfigClass>::stackTop = nullptr;

template <typename ArenaType, typename ConfigClass>
void* ConfigStoreUniversalStatic<ArenaType, ConfigClass>::container = nullptr;

template <typename ArenaType, typename ConfigClass>
class ConfigStoreUniversalPerThread {
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

    /**
    * @brief Pointer to the start of the Container object, one or per thread
    */
    static thread_local void* container;
};

template <typename ArenaType, typename ConfigClass>
thread_local ArenaType* ConfigStoreUniversalPerThread<ArenaType, ConfigClass>::arena = nullptr;

template <typename ArenaType, typename ConfigClass>
thread_local void* ConfigStoreUniversalPerThread<ArenaType, ConfigClass>::stackTop = nullptr;

template <typename ArenaType, typename ConfigClass>
thread_local void* ConfigStoreUniversalPerThread<ArenaType, ConfigClass>::container = nullptr;

template <typename ConfigStore,
          size_t kObjectSize = 0,
          size_t kNodeAlignment = sizeof(typename ConfigStore::Arena::IndexType)>
class SingleArenaConfigUniversal : public ConfigStore {
public:
    using Arena = typename ConfigStore::Arena;

    // API for Pointer
    using IndexType = typename Arena::IndexType;

    static_assert(sizeof(IndexType) >= 2, "IndexType uint8_t is not supported (not safe)");

private:
    static constexpr IndexType kOnStackFlag   = 1u << (sizeof(IndexType) * 8 - 1);
    static constexpr IndexType kContainerFlag = 1u << (sizeof(IndexType) * 8 - 2);
    static constexpr ptrdiff_t kMaxStackSize  = 2 * 1024 * 1024;

    static_assert(!(kObjectSize == 0 && Arena::kIsArrayArenaMT),
                  "ArrayArenaMT can be used only when kObjectSize is defined");

    using ConfigStore::arena;
    using ConfigStore::stackTop;
    using ConfigStore::container;

public:
    // { API for Pointer
    static void* getElement(IndexType index) noexcept {
        void* res = nullptr;
        if ((index & (kContainerFlag | kOnStackFlag)) == 0) {
            res = arena->getElement(index);
        } else {
            if ((index & kOnStackFlag) != 0) {
                res = static_cast<char*>(stackTop) - kNodeAlignment * (index ^ kOnStackFlag);
            } else {
                res = static_cast<char*>(container) + (index ^ kContainerFlag);
            }
        }
        return res;
    }

    static IndexType pointer_to(const void* ptr) noexcept {
        if (kObjectSize == 0) {
            if (ptr >= arena->begin() && ptr < arena->end()) {
                return arena->pointer_to(ptr);
            }
        }
        ptrdiff_t stackOffset = static_cast<char*>(stackTop) - static_cast<const char*>(ptr);
        if ((stackOffset >= 0) && (stackOffset < kMaxStackSize)) {
            size_t offset = size_t(stackOffset) / kNodeAlignment;
            indexed_assert(offset < kOnStackFlag && "object is too deep in stack for indexed::IndexType");
            indexed_assert(offset * kNodeAlignment == size_t(stackOffset) && "object alignment is wrong, check indexed::ArenaConfig::kAlignment");
            return IndexType(offset) | kOnStackFlag;
        }
        ptrdiff_t containerOffset = static_cast<const char*>(ptr) - static_cast<char*>(container);
        if (kObjectSize == 0) {
            indexed_assert(containerOffset >= 0 && containerOffset < 256 && "object isn't in container's body");
            return IndexType(containerOffset) | kContainerFlag;
        }
        return ((containerOffset >= 0) && (containerOffset < ptrdiff_t(kObjectSize)))
               ? IndexType(containerOffset) | kContainerFlag
               : arena->pointer_to(ptr);
    }
    // }

    // { API for user

    /**
     * @brief Set pointer to the Container object
     */
    static void setContainer(void* containerPtr) noexcept { container = containerPtr; }

    /**
     * @brief Get pointer to the Container object
     */
    static void* getContainer() noexcept { return container; }

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

    /**
     * @brief When true the container pointer is assigned to the Allocator
     * address, then it's reset on the Allocator copy or move. Nothing
     * is done automatically when the value is false.
     */
    static constexpr bool kAssignContainerFollowingAllocator = true;

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
* @brief ArenaConfig working with one Arena, single thread, single Container, stack or Arena
*        or Container as Node location is supported.
*
* NOTE Access to Pointers / Allocators defined with the config must be done from the same thread.
* NOTE The config supports only one Container at time. Once it's destucted the other one can be used.
* All Pointers and Allocators defined with the config will use the Arena pointer in the config.
* The pointer can be changed only when there are no Pointers / Allocators created with the old Arena.
* The Container pointer can be changed only when the old Container is destucted.
* @tparam ArenaType type of Arena it works with
* @tparam ConfigClass user-defined class inherited from the config (unique for the used Allocator type)
* @tparam kObjectSize (optional) Container object size in bytes, s.t. container + size doesn't overlap Arena.
* @tparam kNodeAlignment (optional) alignment in bytes for any Pointer using this config
*/
template <typename ArenaType, typename ConfigClass, size_t ...Params>
struct SingleArenaConfigUniversalStatic :
    detail::SingleArenaConfigUniversal<detail::ConfigStoreUniversalStatic<ArenaType, ConfigClass>, Params...> {};

/**
* @brief ArenaConfig allowing one Arena per thread, many threads, Container per thread, stack or
*        Arena or Container as Node location is supported.
*
* It's a MT-safe version of SingleArenaConfigUniversalStatic with thread local arena, stackTop, container.
* NOTE The config supports only one Container at time. Once it's destucted the other one can be used.
* All Pointers and Allocators defined with the config will use the Arena pointer in the config.
* The pointer can be changed only when there are no Pointers/Allocators created with the old Arena.
* @tparam ArenaType type of Arena it works with
* @tparam ConfigClass user-defined class inherited from the config (unique for the used Allocator type)
* @tparam kObjectSize (optional) Container object size in bytes, s.t. container + size doesn't overlap Arena.
* @tparam kNodeAlignment (optional) alignment in bytes for any Pointer using this config
*/
template <typename ArenaType, typename ConfigClass, size_t ...Params>
struct SingleArenaConfigUniversalPerThread :
    detail::SingleArenaConfigUniversal<detail::ConfigStoreUniversalPerThread<ArenaType, ConfigClass>, Params...> {};

}
