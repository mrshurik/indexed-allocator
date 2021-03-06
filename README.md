[![Build Status](https://travis-ci.com/mrshurik/indexed-allocator.svg?branch=master)](https://travis-ci.com/mrshurik/indexed-allocator)

# Indexed allocator
It’s a header-only C++ lib of special allocators for boost node-based containers.

# Main features
- decreases container’s memory overhead by reducing size of pointers used in the container nodes
- allows to allocate containers on stack
- allows to share containers between processes
- quick container serialization (container’s memory is one buffer)

## How?
Indexed allocator defines pointer class which stores pointers as 32- or 16-bit integers.

## Motivation
std::map contains 3 pointers per node, a simple LRU-cache (map + list) contains 5 pointers per node. With 64-bit pointers the overhead is significant, especially when the payload is small, say you store pair<int, int> or one pointer.

## Requirements
- at least C++11 standard compilation 
- need to use only boost::container::slist/list/set/map, boost::unordered::set/map or boost::intrusive::slist/list/set/map containers

## Performance
It depends on your use-case. On the one hand, the lib decreases memory footprint and improves cache locality, so it may lead to higher performance due to cache. On the other hand, pointer operations require more CPU instructions and do more memory jumps, so it may be slower. The lib contains a small benchmark, just use it.

## License
This project is licensed under the [Boost Software License](LICENSE_1_0.txt)

## Author
Dr. Alexander Bulovyatov. Please share your opinion and ideas to bulovyatov AT g m a i l . c o m

## Tutorial
[TUTORIAL.md](TUTORIAL.md)
