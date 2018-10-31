
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <new>

namespace indexed {

/**
* @brief Helper class for ArrayArena. Uses already allocated memory buffer
*/
class BufAlloc {
public:
    BufAlloc(void* bufBegin, size_t bufSize) noexcept
    : m_ptr(nullptr)
    , m_bufBegin(bufBegin)
    , m_bufSize(bufSize) {}

protected:
    void malloc(size_t bytes) {
        if (bytes > m_bufSize) {
            throw std::bad_alloc();
        }
        m_ptr = m_bufBegin;
    }

    void* getPtr() const noexcept {
        return m_ptr;
    }

    void free() noexcept {
        m_ptr = nullptr;
    }

private:
    void*  m_ptr;
    void*  m_bufBegin;
    size_t m_bufSize;
};

}

