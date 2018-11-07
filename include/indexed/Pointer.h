
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <type_traits>

namespace indexed {

namespace detail {

template <typename ArenaConfig>
class VoidPointer {
public:
    using IndexType = typename ArenaConfig::IndexType;

protected:
    IndexType m_index;

    VoidPointer() = default;

    constexpr VoidPointer(std::nullptr_t)
    : m_index(0) {}

    VoidPointer(IndexType index) noexcept
    : m_index(index) {}

    static
    IndexType pointer_to(const void* ref) noexcept {
        return ArenaConfig::pointer_to(ref);
    }

    void* operator->() const noexcept {
        return ArenaConfig::getElement(m_index);
    }

public:
    explicit operator bool() const noexcept {
        return m_index;
    }

    /**
    * @brief Get index integer (e.g. for atomic update)
    */
    const IndexType& get() const noexcept {
        return m_index;
    }

    /**
    * @brief Get index integer (e.g. for atomic update)
    */
    IndexType& get() noexcept {
        return m_index;
    }
};

}

/**
* @brief C++11 pointer class using integer for storage. Usually you shouldn't use it directly.
* NOTE No support for arrays and pointer arithmetic.
* @tparam Type type of object it points to
* @tparam ArenaConfig Arena config class with ArenaConfigInterface (see doc)
*/
template <typename Type, typename ArenaConfig>
class Pointer : public detail::VoidPointer<ArenaConfig> {
private:
    using Base = detail::VoidPointer<ArenaConfig>;

    // can't have reference to void, so change return type to int
    using TypeOrInt = typename std::conditional<std::is_void<Type>::value, int, Type>::type;

public:
    static
    Pointer pointer_to(TypeOrInt& ref) noexcept {
        return Pointer(Base::pointer_to(&ref));
    }

    Pointer() = default;

    constexpr Pointer(std::nullptr_t)
    : Base(nullptr) {}

    explicit Pointer(typename Base::IndexType index) noexcept
    : Base(index) {}

    // static_cast from void pointer
    explicit Pointer(const Pointer<void, ArenaConfig>& p) noexcept
    : Base(p) {}

    template <typename Type2>
    Pointer(const Pointer<Type2,
            typename std::enable_if<std::is_convertible<Type2*, Type*>::value, ArenaConfig>::type>& p) noexcept
    : Base(p) {}

    Type* operator->() const noexcept {
        return static_cast<Type*>(Base::operator->());
    }

    TypeOrInt& operator*() const noexcept {
        return *operator->();
    }

    friend
    bool operator==(const Pointer& left, const Pointer& right) noexcept {
        return left.m_index == right.m_index;
    }

    friend
    bool operator!=(const Pointer& left, const Pointer& right) noexcept {
        return !(left == right);
    }
};

}
