
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <indexed/StackTop.h>
#include <indexed/NewAlloc.h>
#include <indexed/ArrayArena.h>
#include <indexed/ArrayArenaMT.h>
#include <indexed/Allocator.h>
#include <indexed/SingleArenaConfig.h>
#include <indexed/SingleArenaConfigUniversal.h>

#include <boost/container/map.hpp>
#include <boost/unordered_map.hpp>

#include <iostream>
#include <exception>
#include <cstdint>
#include <chrono>
#include <thread>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>

#ifndef NDEBUG
#warning You compile the benchmark not in Release mode!
#endif

using namespace std;
using namespace indexed;

using Arena = ArrayArena<uint32_t, NewAlloc>;
using ArenaMT = ArrayArenaMT<uint32_t, NewAlloc>;

namespace {

struct ArenaConfig : public SingleArenaConfigStatic<Arena, ArenaConfig> {};
struct ArenaConfigUniversal : public SingleArenaConfigUniversalStatic<Arena, ArenaConfigUniversal> {};
struct ArenaConfigMT : public SingleArenaConfigPerThread<ArenaMT, ArenaConfigMT> {};
struct ArenaConfigTL : public SingleArenaConfigPerThread<Arena, ArenaConfigTL> {};

}

using Key = int;
using Value = int;

template <typename Config>
struct Types {
    using Alloc = Allocator<pair<const Key, Value>, Config>;
    using Map = boost::container::map<Key, Value, less<Key>, Alloc>;
    using UnMap = boost::unordered_map<Key, Value, boost::hash<Key>, equal_to<Key>, Alloc>;
};

using Map = boost::container::map<Key, Value>;
using UnMap = boost::unordered_map<Key, Value>;

template <typename Config>
struct Bench {

    using Arena = typename Config::Arena;

    typename Config::ArenaPtr arena;
    size_t n;
    size_t m;
    size_t repeat;
    size_t numThreads;
    bool   runThreadLocal;
    bool   dummy;

template <typename Map>
void map_quick_insert(const char name[], bool showOutput = true);

template <typename Map>
void map_query(const char name[], bool showOutput = true);

template <typename Map>
void map_insert_and_remove(const char name[], bool showOutput = true);

template <typename Map>
void runParallel(const char func[], const char text[]) {
    size_t numThreads = this->numThreads;
    using Func = function<void(Bench&, const char name[], bool showOutput)>;
    Func f;
    string fname(func);
    if (fname == "map_quick_insert") {
        f = Func(&Bench::map_quick_insert<Map>);
    } else if (fname == "map_query") {
        f = Func(&Bench::map_query<Map>);
    } else if (fname == "map_insert_and_remove") {
        f = Func(&Bench::map_insert_and_remove<Map>);
    } else {
        throw runtime_error("can't find function");
    }
    vector<thread> threads;
    threads.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, f, i, text]{
            Config::setArena(this->arena);
            Config::setStackTop(getThreadStackTop());
            f(*this, text, i == 0);
        });
    }
    for (size_t i = 0; i < numThreads; ++i) {
        threads[i].join();
    }
}

unique_ptr<Arena> useLocalArenaIfNeeded(bool doDelete) {
    unique_ptr<Arena> res;
    if (runThreadLocal) {
        res.reset(new Arena(n * m + 1, doDelete));
        Config::setArena(res.get());
    }
    return res;
}

};

template <typename Config>
void benchSingleThread() {
    cout << endl << "Test in single thread mode with " <<
        (is_same<Config, ArenaConfig>::value ? "standard" : "universal") << " ArenaConfig" << endl << endl;
    size_t n = 1024;
    size_t m = 1024;
    Arena arena(n * m + 1);
    Config::setArena(&arena);
    Config::setStackTop(getThreadStackTop());
    Bench<Config> bench = {&arena, n, m, 3, 1, false};

    using indexed = Types<Config>;
    using IndMap = typename indexed::Map;
    using IndUnMap = typename indexed::UnMap;

    // map
    arena.enableDelete(false);
    bench.template map_quick_insert<IndMap>("Insert with indexed map");
    bench.template map_quick_insert<Map>("Insert with map");
    arena.reset();
    bench.template map_query<IndMap>("Query with indexed map");
    bench.template map_query<Map>("Query with map");
    arena.enableDelete(true);
    arena.reset();
    bench.template map_insert_and_remove<IndMap>("Insert and remove with indexed map");
    bench.template map_insert_and_remove<Map>("Insert and remove with map");
    arena.freeMemory();

    // unordered_map
    arena.enableDelete(false);
    bench.template map_quick_insert<IndUnMap>("Insert with unordered indexed map");
    bench.template map_quick_insert<UnMap>("Insert with unordered map");
    arena.reset();
    bench.template map_query<IndUnMap>("Query with unordered indexed map");
    bench.template map_query<UnMap>("Query with unordered map");
    arena.enableDelete(true);
    arena.reset();
    bench.template map_insert_and_remove<IndUnMap>("Insert and remove with unordered indexed map");
    bench.template map_insert_and_remove<UnMap>("Insert and remove with unordered map");
    arena.freeMemory();

    cout << (bench.dummy ? "" : " ") << endl;
}

void benchMultiThreadShared() {
    size_t numThreads = 2;
    cout << endl << "Test in multithread mode with shared ArenaMT and " << numThreads << " threads" << endl << endl;
    size_t n = 1024;
    size_t m = 1024;
    ArenaMT arenaMT((n * m + 1) * numThreads);
    Bench<ArenaConfigMT> bench = {&arenaMT, n, m, 3, numThreads, false};

    using indexed = Types<ArenaConfigMT>;

    // map
    arenaMT.enableDelete(false);
    bench.runParallel<indexed::Map>("map_query", "Query with indexed map");
    bench.runParallel<Map>("map_query", "Query with map");
    arenaMT.reset();
    arenaMT.enableDelete(true);
    bench.runParallel<indexed::Map>("map_insert_and_remove", "Insert and remove with indexed map");
    bench.runParallel<Map>("map_insert_and_remove", "Insert and remove with map");
    arenaMT.freeMemory();

    // unordered map
    arenaMT.enableDelete(false);
    bench.runParallel<indexed::UnMap>("map_query", "Query with indexed unordered map");
    bench.runParallel<UnMap>("map_query", "Query with unordered map");
    arenaMT.reset();
    arenaMT.enableDelete(true);
    bench.runParallel<indexed::UnMap>("map_insert_and_remove", "Insert and remove with indexed unordered map");
    bench.runParallel<UnMap>("map_insert_and_remove", "Insert and remove with unordered map");
    arenaMT.freeMemory();

    cout << (bench.dummy ? "" : " ") << endl;
}

void benchMultiThreadPerThread() {
    size_t numThreads = 2;
    cout << endl << "Test in multithread mode with thread local Arena and " << numThreads << " threads" << endl << endl;
    size_t n = 1024;
    size_t m = 1024;
    Bench<ArenaConfigTL> bench = {nullptr, n, m, 3, numThreads, true};

    using indexed = Types<ArenaConfigTL>;

    // map
    bench.runParallel<indexed::Map>("map_quick_insert", "Insert with indexed map");
    bench.runParallel<Map>("map_quick_insert", "Insert with map");
    bench.runParallel<indexed::Map>("map_query", "Query with indexed map");
    bench.runParallel<Map>("map_query", "Query with map");
    bench.runParallel<indexed::Map>("map_insert_and_remove", "Insert and remove with indexed map");
    bench.runParallel<Map>("map_insert_and_remove", "Insert and remove with map");

    // unordered map
    bench.runParallel<indexed::UnMap>("map_quick_insert", "Insert with indexed unordered map");
    bench.runParallel<UnMap>("map_quick_insert", "Insert with unordered map");
    bench.runParallel<indexed::UnMap>("map_query", "Query with indexed unordered map");
    bench.runParallel<UnMap>("map_query", "Query with unordered map");
    bench.runParallel<indexed::UnMap>("map_insert_and_remove", "Insert and remove with indexed unordered map");
    bench.runParallel<UnMap>("map_insert_and_remove", "Insert and remove with unordered map");

    cout << (bench.dummy ? "" : " ") << endl;
}

int main() {
#ifndef NDEBUG
    cout << "You run the benchmark compiled not in Release mode!" << endl;
#endif
    try {
        benchSingleThread<ArenaConfig>();
        benchSingleThread<ArenaConfigUniversal>();
        benchMultiThreadPerThread();
        benchMultiThreadShared();
    } catch(const exception& ex) {
        cerr << "Bench exit with exception " << ex.what() << endl;
        return 1;
    }
    return 0;
}

template <typename Config>
template <typename Map>
void Bench<Config>::map_quick_insert(const char name[], bool showOutput) {
    auto locArena = useLocalArenaIfNeeded(false);
    size_t dummy = 0;
    auto start = chrono::high_resolution_clock::now();
    for (size_t k = 0; k < repeat; ++k) {
        Config::getArena()->reset();
        Map map;

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < m; ++j) {
                int key = int(j * n + i);
                map.emplace(key, 0);
            }
        }

        dummy += map.size();
    }
    auto end = chrono::high_resolution_clock::now();
    auto time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    if (showOutput) {
        cout << name << ": wall time " << time << endl;
    }
    this->dummy |= dummy;
}

template <typename Config>
template <typename Map>
void Bench<Config>::map_query(const char name[], bool showOutput) {
    auto locArena = useLocalArenaIfNeeded(false);
    Map map;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < m; ++j) {
            int key = int(j * n + i);
            map.emplace(2 * key, 1);
        }
    }

    size_t dummy = 0;
    auto start = chrono::high_resolution_clock::now();
    const Map& cmap = map;
    for (size_t k = 0; k < repeat; ++k) {
        for (size_t i = n; i > 0; --i) {
            for (size_t j = 0; j < m; ++j) {
                int key = int(j * n + i - 1);
                dummy += (*cmap.find(2 * key)).second;
                dummy += cmap.count(2 * key + 1);
            }
        }
    }
    auto end = chrono::high_resolution_clock::now();
    auto time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    if (showOutput) {
        cout << name << ": wall time " << time << endl;
    }
    this->dummy |= dummy;
}

template <typename Config>
template <typename Map>
void Bench<Config>::map_insert_and_remove(const char name[], bool showOutput) {
    auto locArena = useLocalArenaIfNeeded(true);
    const size_t capacity = n * m;
    size_t dummy = 0;
    auto start = chrono::high_resolution_clock::now();
    for (size_t k = 0; k < repeat; ++k) {
        Map map;

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < m; ++j) {
                int key = int(j * n + i);
                map.emplace(key, 0);
            }
            if (i % 2 == 1) {
                for (size_t j = 0; j < m; ++j) {
                    int key = int(j * n + i - 1);
                    map.erase(key);
                }
                for (size_t j = 0; j < m; ++j) {
                    int key = int(j * n + i - 1 + capacity);
                    map.emplace(key, 0);
                }
            }
        }

        dummy += map.size();
    }
    auto end = chrono::high_resolution_clock::now();
    auto time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    if (showOutput) {
        cout << name << ": wall time " << time << endl;
    }
    this->dummy |= dummy;
}
