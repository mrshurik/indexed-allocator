
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <indexed/ArrayArenaMT.h>
#include <indexed/NewAlloc.h>
#include <indexed/SingleArenaConfigUniversal.h>
#include <indexed/Allocator.h>
#include <indexed/StackTop.h>

#include <boost/intrusive/list.hpp>
#include <boost/unordered_map.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <algorithm>
#include <functional>

using namespace indexed;
using namespace std;

using Arena = ArrayArenaMT<uint16_t, NewAlloc>;

// NOTE boost::intrusive::list must use Universal config when the container is allocated on heap
//      boost::unordered_map doesn't require Universal config
namespace {
    // Emulate List, which is defined below. We just need to know its size.
    // We can't simply get sizeof(List), since List type depends on ArenaConfig.
    struct ListEquivalent {
        size_t size;
        Arena::IndexType ptr_head;
        Arena::IndexType ptr_tail;
    };

    // We have to define kObjectSize for SingleArenaConfigUniversal since we use it together with ArrayArenaMT.
    // It's needed in order to avoid data races in ArrayArenaMT (see its doc). It's not needed for ArrayArena.
    struct ArenaConfig : public SingleArenaConfigUniversalStatic<Arena, ArenaConfig, sizeof(ListEquivalent)> {
        // We want to call setContainer() on our own instead of automatic assignment
        static constexpr bool kAssignContainerFollowingAllocator = false;
    };
}

using Key = int;
using Value = int;

struct LRUKey : boost::intrusive::list_base_hook<boost::intrusive::void_pointer<Pointer<void, ArenaConfig>>> {
    Key key;

    LRUKey() = default;

    LRUKey(const Key& other)
    : key(other) {}

    struct Hasher : public hash<Key> {
        size_t operator()(const LRUKey& val) const { return hash<Key>::operator()(val.key); }
    };

    bool operator==(const LRUKey& other) const { return key == other.key; }
};

using Pair = pair<const LRUKey, Value>;
using Alloc = Allocator<Pair, ArenaConfig>;

using List = boost::intrusive::list<LRUKey>;
using Map = boost::unordered_map<LRUKey, Value, LRUKey::Hasher, equal_to<LRUKey>, Alloc>;

class LRUCache {
private:
    struct Init {
        Init(List* list) {
            // Only list object contains nodes, unordered_map object doesn't.
            ArenaConfig::setContainer(list);
        }
    };

public:
    LRUCache(size_t capacity)
    : m_dummy(&m_list)
    , m_capacity(capacity) {}

    Map::const_iterator find(const LRUKey& key) const { return m_map.find(key); }

    Map::const_iterator begin() const { return m_map.begin(); }

    Map::const_iterator end() const { return m_map.end(); }

    Pair& getOrInsert(const Pair& p) {
        auto it = m_map.find(p.first);
        if (it != m_map.end()) {
            auto& node = it->first;
            if (&node != &m_list.back()) {
                m_list.erase(m_list.iterator_to(node));
                m_list.push_back(const_cast<LRUKey&>(node));
            }
            return *it;
        }
        return (m_list.size() == m_capacity) ? dropThenAdd(p) : add(p);
    }

private:
    void dropOld() {
        auto& node = m_list.front();
        m_list.pop_front();
        m_map.erase(node);
    }

    Pair& add(const Pair& p) {
        auto pair = m_map.insert(p);
        m_list.push_back(const_cast<LRUKey&>(pair.first->first));
        return *pair.first;
    }

    Pair& dropThenAdd(const Pair& p) {
        dropOld();
        return add(p);
    }

    Init   m_dummy;
    Map    m_map;
    List   m_list;
    size_t m_capacity;
};

class End2EndIntrusiveTest : public ::testing::Test {
protected:
    static constexpr size_t capacity = 10;

    struct Init {
        Init(Arena* arena) {
            ArenaConfig::setArena(arena);
            ArenaConfig::setStackTop(getThreadStackTop());
        }
    };

    Arena m_arena;
    Init  m_dummy; // ArenaConfig must be initialized before map since map uses Alloc

    End2EndIntrusiveTest()
    : m_arena(capacity)
    , m_dummy(&m_arena) {}
};

TEST_F(End2EndIntrusiveTest, fillLRU) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    unique_ptr<LRUCache> cache{new LRUCache(capacity)};
    for (auto& p : elements) {
        cache->getOrInsert(p);
    }
    for (auto& p : elements) {
        auto it = cache->find(p.first);
        ASSERT_TRUE(it != cache->end());
        EXPECT_EQ(*it, p);
    }
}

TEST_F(End2EndIntrusiveTest, dropOld) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    LRUCache cache{4};
    for (auto& p : elements) {
        cache.getOrInsert(p);
    }
    for (auto& p : elements) {
        auto it = cache.find(p.first);
        ASSERT_TRUE(it != cache.end());
        EXPECT_EQ(*it, p);
    }
    Pair pair(6, -6);
    cache.getOrInsert(pair);
    EXPECT_TRUE(cache.find(1) == cache.end());
    auto it = cache.find(pair.first);
    ASSERT_TRUE(it != cache.end());
    EXPECT_EQ(*it, pair);
}

TEST_F(End2EndIntrusiveTest, dontDropOld) {
    initializer_list<Pair> elements = {{1, -1}, {2, -2}, {3, -3}, {5, -5}};
    LRUCache cache{4};
    for (auto& p : elements) {
        cache.getOrInsert(p);
    }
    Pair pair(2, -6);
    cache.getOrInsert(pair);
    EXPECT_TRUE(cache.find(1) != cache.end());
    auto it = cache.find(pair.first);
    ASSERT_TRUE(it != cache.end());
    EXPECT_NE(*it, pair);
    EXPECT_EQ(*it, *(elements.begin() + 1));
}

TEST_F(End2EndIntrusiveTest, nodeSize) {
    LRUCache cache{capacity};
    cache.getOrInsert({1, 2});
    size_t wsize = sizeof(size_t);
    size_t elementSize = 3 * sizeof(uint16_t) + 2 * sizeof(int) + wsize;
    elementSize = (elementSize + wsize - 1) / wsize * wsize; // round up
    // 24b for 64-bit
    EXPECT_EQ(elementSize, m_arena.elementSize()); // + 2 bytes per bucket
}
