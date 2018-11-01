# Indexed Allocator tutorial

### Building the library
It’s a header-only library, you don’t have to build and install anything, just set path to the include directory when building your project.

### Building and running the tests
You need to have cmake and googletests installed. Go to the project directory.
```sh
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make test
```

### Building and running the benchmark
You need to have cmake installed. Go to the build directory. Add -DNO_INDEXED_TESTS=1 if you don't want or can't build tests.
```sh
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
$ ./Bench
```

## Concepts
Let’s briefly describe objects taking part in memory allocation:

**Container** - a node-based boost container allocating Node objects. Example: boost::container::list<int>.
 
**Arena** - a memory buffer, array in memory where place for Node objects is allocated. The Arena is a “stateful malloc” returning indices instead of pointers. Arena is parametrized by IndexType used for the indices, it can only allocate objects of one size and the whole Arena memory is allocated at once.

**Allocator** - a STL-compatible memory allocator needed for definition of a Container type. It redirects allocation to the Arena.

**Pointer** - a pointer class which stores indices internally. It’s defined in the Allocator, so the Container replaces raw pointers with Pointers in Nodes, making Nodes smaller.

**ArenaConfig** - a special class which defines how indices are mapped to raw pointers and back. The class contains static members only. Allocator and Pointer types are parametrized by an ArenaConfig type and so they know how to map indices to raw pointers.

The library defines Pointer<Type, ArenaConfig> class which stores an unsigned integer of IndexedType. In order to convert a raw pointer to an integer and back the following assumptions have been made:
 - A raw pointer points to an object (Node) located either on a thread’s stack, or in the Arena, or in the Container object. Any other location is not supported.
 - Stack grows from higher addresses to smaller ones, this is true for most of modern CPUs.
- The pointer must be aligned to, at least, sizeof(IndexType).
- When the pointer points to an object in the Arena, the address must be as for the array<Node>, i.e. address == Arena.begin() + k * sizeof(Node). The raw pointer can’t point to something inside a Node.

Under these assumptions 16-bit IndexType allows for 2^14 or 2^15 allocated objects, while 32-bit IndexType allows for 2^30 or 2^31 objects. There are other restrictions described below.

Pointer objects store only an index, the rest is stored in static variables of the ArenaConfig, one data for all pointers: pointer to the top of a thread’s stack, pointer to the Arena, pointer to the Container. These pointers can be thread local, so at most one Arena per thread is supported. It’s the price of small pointers.

## Description of classes
**ArrayArena** - a simple Arena, is not thread-safe, is parametrized by IndexType and Alloc. Alloc defines how real memory is allocated, the allocation happens on the first call to Arena::allocate(). There are following Alloc classes: NewAlloc - uses C++ operator new, MmapAlloc - uses OS memory pages, BufAlloc - uses an already allocated memory buffer. MmapAlloc allows to “reserve” memory instead of allocating it at once, the real memory is lazy allocated when the Arena grows, but the allocation granularity is 4 KB, which isn’t good for a small Container.

**ArrayArenaMT** - the same as ArrayArena, but it’s thread-safe, designed to reuse/share Arena’s pool between several threads. It’s slower than ArrayArena due to extra synchronization overhead.

**SingleArenaConfig** - ArenaConfig with assumption that a Node is located either on a stack, or in the Arena. As the result a Container object using this config can’t be located in heap, only on stack. For clarity, here “Container object is located on stack” means that the object itself (list) is located on the stack, while its Nodes are located in the Arena. The same SingleArenaConfig can be used by multiple Container instances. Also, it’s slightly faster than the other config type. SingleArenaConfig uses 1 bit in IndexType for an internal flag. There are SingleArenaConfigStatic and SingleArenaConfigPerThread, which use either static, or static thread local variables for stackTop and arena pointers.

**SingleArenaConfigUniversal** - ArenaConfig with assumption that a Node is located either on a stack, or in the Arena, or in the Container object. It also supports the case when the Arena’s memory is located on the stack. As a disadvantage, only one (or per thread) Container instance is supported. It’s address must be given to the config before the Container is constructed. Usually it’s done automatically by the Allocator, except for the case of boost::intrusive containers when it must be done explicitly. SingleArenaConfigUniversal uses 2 bits in IndexType for internal flags. There are SingleArenaConfigUniversalStatic and SingleArenaConfigUniversalPerThread classes, which use either static, or static thread local variables for stackTop, arena and container pointers.

**Allocator** - an STL-allocator, it’s parametrized by an ArenaConfig type. You need to define an Allocator type in order to define a Container type. Different Container types can be defined using the same ArenaConfig, but since the config uses one Arena, the Containers used at the same time must have equal size of Nodes. The Allocator contains pointer to the Arena, the pointer can be passed explicitly to the constructor or is obtained automatically from ArenaConfig::defaultArena().

## Notes

### Boost unordered set/map containers
They’re a bit special. First, for them you don’t need to use SingleArenaConfigUniversal even when the container is located in heap. Second, they need to allocate vector of buckets, which is resized from time to time. It’s not supported by the Allocator, so the Allocator rebinds to std::allocator for the bucket type. As the result, bucket memory is allocated via std::allocator.

### Stack and 16-bit IndexType
Pointer class must be able to address objects on stack. When IndexType is uint16_t, there are only 14 or 15 bits available. With the default Node alignment = sizeof(IndexType) it gives only 32 KB or 64 KB. If the stack is deeper the code may fail. There are 2 ways to fix it. You can increase Node alignment, depending on your use-case Node can have 4 or 8 bytes alignment. Be careful. Another direction, instead of pointing to the top of a stack, you can set stackTop to address below it, to a function’s frame where the container is located or used. Be very careful. 

### Debugging support
Since the code is not trivial and relies on a few assumptions these assumptions and some pre/post-conditions are checked in asserts. When the code is compiled in Release mode (NDEBUG var is defined) the asserts are removed, if you need them in Release mode please define INDEXED_DEBUG=1.

### Code example
```C++
#include <indexed/ArrayArena.h>
#include <indexed/NewAlloc.h>
#include <indexed/SingleArenaConfigUniversal.h>
#include <indexed/Allocator.h>
#include <indexed/StackTop.h>

#include <boost/container/list.hpp>

using namespace indexed;

using Arena = ArrayArena<uint16_t, NewAlloc>; // 16-bit indexed Arena with new()

namespace {
    // define your ArenaConfig via subclassing
    struct MyArenaConfig : public SingleArenaConfigUniversalStatic<Arena, MyArenaConfig> {};
}

using ValueType = int;
using Alloc = Allocator<ValueType, MyArenaConfig>;
using List = boost::container::list<ValueType, Alloc>;

void myFunction() {
    Arena myArena(10); // Arena with capacity 10
    MyArenaConfig::arena = &myArena; // set Arena pointer in the config
    MyArenaConfig::stackTop = getThreadStackTop(); // set pointer to the top of the stack
    List myList; // Alloc will use Arena from MyArenaConfig
    myList.push_back(1); // use list as usual
}
```

## FAQ
**How to resize an Arena in order to grow or shrink Containers?**
There is no easy way. The Arena’s capacity is fixed. You can only do the following trick. First, copy data from the containers to, say, a std::vector. Then, you need to destroy the containers or do container = Container(). Then, do arena.freeMemory() and arena.setCapacity(new). Now create new containers, if needed, and copy the data from the std::vector.

**How to ensure that a Container has no allocated Nodes?**
You may need it if you want to do arena.reset() or arena.freeMemory(). Simple container.clear() is not enough. Do container = Container().
