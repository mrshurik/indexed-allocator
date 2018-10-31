
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#include <boost/interprocess/anonymous_shared_memory.hpp>

namespace indexed {

/**
* @brief Helper class for ArrayArena. Allocates memory via mmap in pages
*/
class MmapAlloc {
public:
    MmapAlloc() = default;

protected:
    void malloc(size_t bytes) {
        m_memMapped = boost::interprocess::anonymous_shared_memory(bytes);
    }

    void* getPtr() const noexcept {
        return m_memMapped.get_address();
    }

    void free() noexcept {
        m_memMapped = boost::interprocess::mapped_region();
    }

private:
    boost::interprocess::mapped_region m_memMapped;
};

}
