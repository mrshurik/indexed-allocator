
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <indexed/ArrayArena.h>
#include <indexed/MmapAlloc.h>
#include <indexed/SingleArenaConfig.h>
#include <indexed/Allocator.h>
#include <indexed/StackTop.h>

#include <boost/unordered_map.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>

using namespace indexed;
using namespace std;

using Arena = ArrayArena<uint16_t, MmapAlloc>;

// NOTE boost::unordered_set/map can use non-Universal config even if the container is allocated on heap
namespace {
    struct ArenaConfig : public SingleArenaConfigStatic<Arena, ArenaConfig> {};
}

using Key = int;
using Value = int;
using Pair = pair<const Key, Value>;
using Alloc = Allocator<Pair, ArenaConfig>;
using Map = boost::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Alloc>;

class End2EndUnorderedTest : public ::testing::Test {
protected:
    static constexpr size_t capacity = 12;

    struct Init {
        Init(Arena* arena) {
            ArenaConfig::setArena(arena);
            ArenaConfig::setStackTop(getThreadStackTop());
        }
    };

    Arena m_arena;
    Init  m_dummy; // ArenaConfig must be initialized before map since map uses Alloc

    End2EndUnorderedTest()
    : m_arena(capacity)
    , m_dummy(&m_arena) {}
};

TEST_F(End2EndUnorderedTest, insertInitList) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements)};
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, map->find(srcIt->first)->second);
    }
}

TEST_F(End2EndUnorderedTest, insert) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    Map map;
    for (const Pair& p : elements) {
        map.insert(p);
    }
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, map.find(srcIt->first)->second);
    }
}

TEST_F(End2EndUnorderedTest, emplace) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    Map map;
    for (const Pair& p : elements) {
        map.emplace(p.first, p.second);
    }
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, map.find(srcIt->first)->second);
    }
}

TEST_F(End2EndUnorderedTest, eraseByKey) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements)};
    for (const Pair& p : elements) {
        map->erase(p.first);
    }
    ASSERT_EQ(0, map->size());
}

TEST_F(End2EndUnorderedTest, eraseByIter) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements.begin(), elements.end())};
    for (const Pair& p : elements) {
        map->erase(map->find(p.first));
    }
    ASSERT_EQ(0, map->size());
}

TEST_F(End2EndUnorderedTest, copyContainer) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<Map> map{new Map(elements.begin(), elements.end())};
    Map another;
    another = *map;
    for (auto srcIt = elements.begin(); srcIt != elements.end(); ++srcIt) {
        EXPECT_EQ(srcIt->second, another.find(srcIt->first)->second);
    }
}

TEST_F(End2EndUnorderedTest, mapArenaNoResetOnContainerClear) {
    Map map;
    EXPECT_EQ(0, m_arena.usedCapacity());
    map.emplace(1, 2);
    EXPECT_EQ(2, m_arena.usedCapacity()); // 1 node is extra allocated for internal use
    map.clear();
    EXPECT_EQ(1, m_arena.allocatedCount()); // 1 internal node is still not freed
    EXPECT_EQ(2, m_arena.usedCapacity());
    map = Map(); // force map to deallocate all
    EXPECT_EQ(0, m_arena.usedCapacity()); // safe to do arena.freeMemory()
    m_arena.freeMemory();
}

TEST_F(End2EndUnorderedTest, passAllocExplicitly) {
    Map map{Alloc(&m_arena)};
    map.emplace(1, 2);
    auto it = map.find(1);
    ASSERT_TRUE(it != map.end());
    ASSERT_EQ(2, it->second);
}

TEST_F(End2EndUnorderedTest, passAndParamsExplicitly) {
    Map map(0, hash<Key>(), equal_to<Key>(), Alloc(&m_arena));
    map.emplace(1, 2);
    auto it = map.find(1);
    ASSERT_TRUE(it != map.end());
    ASSERT_EQ(2, it->second);
}
