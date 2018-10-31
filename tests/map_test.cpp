
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <indexed/ArrayArena.h>
#include <indexed/NewAlloc.h>
#include <indexed/BufAlloc.h>
#include <indexed/SingleArenaConfigUniversal.h>
#include <indexed/Allocator.h>
#include <indexed/StackTop.h>

#include <boost/container/map.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <algorithm>

using namespace indexed;
using namespace std;

using Arena = ArrayArena<uint32_t, NewAlloc>;
using ArenaBuf = ArrayArena<uint32_t, BufAlloc>;

// NOTE boost::container::set/map must use Universal config when the container is allocated on heap
namespace {
    struct ArenaConfig : public SingleArenaConfigUniversalStatic<Arena, ArenaConfig> {};
    struct ArenaConfigStack : public SingleArenaConfigUniversalStatic<ArenaBuf, ArenaConfigStack> {};
}

using Key = int;
using Value = int;
using Pair = pair<const Key, Value>;
using Alloc = Allocator<Pair, ArenaConfig>;
using Map = boost::container::map<Key, Value, std::less<Key>, Alloc>;
using AllocOnStack = Allocator<Pair, ArenaConfigStack>;
using MapOnStack = boost::container::map<Key, Value, std::less<Key>, AllocOnStack>;

class End2EndMapTest : public ::testing::Test {
protected:
    static constexpr size_t capacity = 10;

    struct Init {
        Init(Arena* arena) {
            ArenaConfig::arena = arena;
            ArenaConfig::stackTop = getThreadStackTop();
        }
    };

    Arena m_arena;
    Init  m_dummy; // ArenaConfig must be initialized before map since map uses Alloc

    End2EndMapTest()
    : m_arena(capacity)
    , m_dummy(&m_arena) {}
};

TEST_F(End2EndMapTest, insertInitList) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements)};
    auto srcIt = elements.begin();
    auto mapIt = map->begin();
    for (; srcIt != elements.end(); ++srcIt, ++mapIt) {
        EXPECT_EQ(*srcIt, *mapIt);
    }
}

TEST_F(End2EndMapTest, insert) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    Map map;
    for (const Pair& p : elements) {
        map.insert(p);
    }
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, (*map.find(srcIt->first)).second);
    }
}

TEST_F(End2EndMapTest, emplace) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    Map map;
    for (const Pair& p : elements) {
        map.emplace(p.first, p.second);
    }
    ASSERT_TRUE(equal(elements.begin(), elements.end(), map.begin()));
}

TEST_F(End2EndMapTest, eraseByKey) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements)};
    for (const Pair& p : elements) {
        map->erase(p.first);
    }
    ASSERT_EQ(0, map->size());
}

TEST_F(End2EndMapTest, eraseByIter) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements.begin(), elements.end())};
    for (const Pair& p : elements) {
        map->erase(map->find(p.first));
    }
    ASSERT_EQ(0, map->size());
}

TEST_F(End2EndMapTest, passAllocExplicitly) {
    Map map{Alloc(&m_arena)};
    map.emplace(1, 2);
    auto it = map.find(1);
    ASSERT_TRUE(it != map.end());
    ASSERT_EQ(2, (*it).second);
}

TEST_F(End2EndMapTest, passAllocAndCompareExplicitly) {
    Map map{less<Key>(), Alloc(&m_arena)};
    map.emplace(1, 2);
    auto it = map.find(1);
    ASSERT_TRUE(it != map.end());
    ASSERT_EQ(2, (*it).second);
}

TEST_F(End2EndMapTest, mapArenaResetOnContainerClear) {
    Map map;
    EXPECT_EQ(0, m_arena.usedCapacity());
    map.emplace(1, 2);
    EXPECT_EQ(1, m_arena.usedCapacity());
    map.erase(1); // or do map.clear()
    EXPECT_EQ(0, m_arena.usedCapacity()); // safe to do arena.freeMemory()
    m_arena.freeMemory();
}

TEST_F(End2EndMapTest, allocateOnStack) {
    int buf[10 * 24 / 4];
    ArenaBuf arena(10, true, BufAlloc(buf, sizeof(buf)));

    ArenaConfigStack::arena = &arena;
    ArenaConfigStack::stackTop = getThreadStackTop();

    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    MapOnStack map;

    for (const Pair& p : elements) {
        map.insert(p);
    }
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, (*map.find(srcIt->first)).second);
    }
}
