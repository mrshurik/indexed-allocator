
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <indexed/Config.h>

#ifdef WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#include <stdexcept>
#endif

namespace indexed {

// If the given functions don't compile you may need to write your own getThreadStackTop()

#ifdef WIN32

#if !(_WIN32_WINNT >= 0x0602)
#error "Thread API used here requires at least Windows 8"
#endif

/**
* @brief Get pointer to a thread's stack highest address
*/
inline void* getThreadStackTop() {
	HANDLE_PTR lowLimit = 0;
	HANDLE_PTR highLimit = 0;
	GetCurrentThreadStackLimits(&lowLimit, &highLimit);
	return (void*)(highLimit);
}

#elif defined(__APPLE__)

/**
* @brief Get pointer to a thread's stack highest address
*/
inline void* getThreadStackTop() {
    return pthread_get_stackaddr_np(pthread_self());
}

#else

/**
* @brief Get pointer to a thread's stack highest address
*/
inline void* getThreadStackTop() {
    const pthread_t id = pthread_self();
    pthread_attr_t attr;
    int err = pthread_getattr_np(id, &attr);
    void* stackAddr = nullptr;
    size_t stackSize = 0;
    if (err == 0) {
        err = pthread_attr_getstack(&attr, &stackAddr, &stackSize);
        pthread_attr_destroy(&attr);
    }
    if (err == 0) {
        return static_cast<char*>(stackAddr) + stackSize;
    } else {
        throw std::runtime_error("getThreadStackTop() error due to pthread API");
    }
}

#endif

}
