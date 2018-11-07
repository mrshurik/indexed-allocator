
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <indexed/ArrayArena.h>
#include <indexed/NewAlloc.h>
#include <indexed/SingleArenaConfig.h>
#include <indexed/Allocator.h>
#include <indexed/StackTop.h>

#include <gtest/gtest.h>

#include <cstdint>

using namespace indexed;
using namespace std;

using Arena = ArrayArena<uint16_t, NewAlloc>;

namespace {
    struct ArenaConfig : public SingleArenaConfigStatic<Arena, ArenaConfig> {};
}

class PointerTest : public ::testing::Test {
protected:
    static constexpr size_t capacity = 12;

    struct Init {
        Init(Arena* arena) {
            ArenaConfig::setArena(arena);
            ArenaConfig::setStackTop(getThreadStackTop());
        }
    };

    Arena m_arena;
    Init  m_dummy;

    PointerTest()
    : m_arena(capacity)
    , m_dummy(&m_arena) {}
};

TEST_F(PointerTest, castToConstPointer) {
    Allocator<int, ArenaConfig> alloc;
    Pointer<int, ArenaConfig> ptr = alloc.allocate(1);
    Pointer<const int, ArenaConfig> ptrToConst = ptr;
    ptrToConst = ptr;
    EXPECT_NE(ptrToConst, nullptr);
    EXPECT_EQ(ptrToConst, ptr);
    alloc.deallocate(ptr, 1);
}

TEST_F(PointerTest, castToVoidPointer) {
    Allocator<int, ArenaConfig> alloc;
    Pointer<int, ArenaConfig> ptr = alloc.allocate(1);
    Pointer<void, ArenaConfig> ptrToVoid = ptr;
    ptrToVoid = ptr;
    EXPECT_NE(ptrToVoid, nullptr);
    EXPECT_EQ(ptrToVoid, ptr);
    alloc.deallocate(ptr, 1);
}

TEST_F(PointerTest, castFromVoidPointer) {
    Allocator<int, ArenaConfig> alloc;
    Pointer<int, ArenaConfig> ptr = alloc.allocate(1);
    Pointer<void, ArenaConfig> ptrToVoid = ptr;
    Pointer<int, ArenaConfig> ptr2 = static_cast<Pointer<int, ArenaConfig>>(ptrToVoid);
    EXPECT_EQ(ptr2, ptr);
    alloc.deallocate(ptr, 1);
}

struct BaseClass {
    int member;
};

struct DerivedClass : BaseClass {};

TEST_F(PointerTest, castToBasePointer) {
    Allocator<DerivedClass, ArenaConfig> alloc;
    Pointer<DerivedClass, ArenaConfig> ptr = alloc.allocate(1);
    Pointer<BaseClass, ArenaConfig> ptrToBase = ptr;
    ptrToBase = ptr;
    EXPECT_EQ(ptrToBase, ptr);
    alloc.deallocate(ptr, 1);
}

TEST_F(PointerTest, castToDerivedPointer) {
    Allocator<DerivedClass, ArenaConfig> alloc;
    Pointer<DerivedClass, ArenaConfig> ptr = alloc.allocate(1);
    Pointer<BaseClass, ArenaConfig> ptrToBase = ptr;
    Pointer<DerivedClass, ArenaConfig> ptr2 = static_cast<Pointer<DerivedClass, ArenaConfig>>(ptrToBase);
    EXPECT_EQ(ptr2, ptr);
    alloc.deallocate(ptr, 1);
}

TEST_F(PointerTest, usePointerInArena) {
    Allocator<int, ArenaConfig> alloc;
    Pointer<int, ArenaConfig> ptr = alloc.allocate(1);
    *ptr = 1;
    EXPECT_EQ(*ptr, 1);
    EXPECT_EQ(ptr.operator->(), reinterpret_cast<int*>(m_arena.begin()));
    auto ptr2 = Pointer<int, ArenaConfig>::pointer_to(*ptr);
    EXPECT_EQ(ptr, ptr2);
    alloc.deallocate(ptr, 1);
}

TEST_F(PointerTest, usePointerOnStack) {
    Pointer<int, ArenaConfig> ptr = nullptr;
    int v = 1;
    ptr = ptr.pointer_to(v);
    EXPECT_EQ(ptr.operator->(), &v);
    EXPECT_EQ(*ptr, 1);
    *ptr = 2;
    EXPECT_EQ(v, 2);
}
