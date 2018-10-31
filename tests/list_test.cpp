
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

#include <boost/container/list.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <algorithm>

using namespace indexed;
using namespace std;

using Arena = ArrayArena<uint16_t, NewAlloc>;
using ArenaBuf = ArrayArena<uint16_t, BufAlloc>;

// NOTE boost::container::slist/list must use Universal config when the container is allocated on heap
namespace {
    struct ArenaConfig : public SingleArenaConfigUniversalStatic<Arena, ArenaConfig> {};
    struct ArenaConfigStack : public SingleArenaConfigUniversalStatic<ArenaBuf, ArenaConfigStack> {};
}

using Value = int;
using Alloc = Allocator<Value, ArenaConfig>;
using List = boost::container::list<Value, Alloc>;
using AllocOnStack = Allocator<Value, ArenaConfigStack>;
using ListOnStack = boost::container::list<Value, AllocOnStack>;

class End2EndListTest : public ::testing::Test {
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

    End2EndListTest()
    : m_arena(capacity)
    , m_dummy(&m_arena) {}
};

TEST_F(End2EndListTest, insertInitList) {
    initializer_list<Value> elements = {7, 4, 2, 2, 49, 3, -1};
    unique_ptr<List> list{new List(elements)};
    auto srcIt = elements.begin();
    auto listIt = list->begin();
    for (; srcIt != elements.end(); ++srcIt, ++listIt) {
        EXPECT_EQ(*srcIt, *listIt);
    }
}

TEST_F(End2EndListTest, push_back) {
    initializer_list<Value> elements = {7, 4, 2, 2, 49, 3, -1};
    unique_ptr<List> list{new List};
    for (Value v : elements) {
        list->push_back(v);
    }
    auto srcIt = elements.begin();
    auto listIt = list->begin();
    for (; srcIt != elements.end(); ++srcIt, ++listIt) {
        EXPECT_EQ(*srcIt, *listIt);
    }
}

TEST_F(End2EndListTest, insert) {
    Value elements[] = {7, 4, 2, 2, 49, 3, -1};
    size_t size = sizeof(elements) / sizeof(Value);
    List list;
    list.push_back(elements[0]);
    list.push_back(elements[size - 1]);
    for (size_t i = 1; i < size - 1; ++i) {
        list.insert(--list.end(), elements[i]);
    }
    ASSERT_TRUE(equal(elements, elements + size, list.begin()));
}

TEST_F(End2EndListTest, erase) {
    initializer_list<Value> elements = {7, 4, 2, 2, 49, 3, -1};
    List list(elements);
    list.erase(find(list.begin(), list.end(), 2));
    EXPECT_EQ(elements.size() - 1, list.size());
}

TEST_F(End2EndListTest, passAllocExplicitly) {
    List list{Alloc(&m_arena)};
    list.push_back(1);
    ASSERT_EQ(1, list.back());
}

TEST_F(End2EndListTest, mapArenaResetOnContainerClear) {
    List list;
    EXPECT_EQ(0, m_arena.usedCapacity());
    list.push_back(1);
    EXPECT_EQ(8, m_arena.elementSize());
    EXPECT_EQ(1, m_arena.usedCapacity());
    list.pop_front(); // or do map.clear()
    EXPECT_EQ(0, m_arena.usedCapacity()); // safe to do arena.freeMemory()
    m_arena.freeMemory();
}

TEST_F(End2EndListTest, allocateOnStack) {
    int buf[8 * 10 / 4];
    ArenaBuf arena(10, true, BufAlloc(buf, sizeof(buf)));

    ArenaConfigStack::arena = &arena;
    ArenaConfigStack::stackTop = getThreadStackTop();

    initializer_list<Value> elements = {7, 4, 2, 2, 49, 3, -1};
    ListOnStack list;

    for (Value v : elements) {
        list.push_back(v);
    }
    ASSERT_TRUE(equal(elements.begin(), elements.end(), list.begin()));
}
