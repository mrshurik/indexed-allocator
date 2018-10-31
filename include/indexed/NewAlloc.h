
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <memory>

namespace indexed {

/**
* @brief Helper class for ArrayArena. Allocates memory via ::new
*/
class NewAlloc {
public:
    NewAlloc() = default;

protected:
    void malloc(size_t bytes) {
        m_memBlock.reset(new char[bytes]);
    }

    void* getPtr() const noexcept {
        return m_memBlock.get();
    }

    void free() noexcept {
        m_memBlock.reset();
    }

private:
    std::unique_ptr<char[]> m_memBlock;
};

}
