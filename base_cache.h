#pragma once

#include <thread>
#include <iostream>
#include <complex>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <vector>
#include <map>
#include <array>
#include <fstream>
#include <cstring>
#include <iterator>
#include <format>
#include <list>

const char* MUL_CACHE_INFO_FILENAME {"multi_cache_levels.txt"};

enum class CacheType : std::uint8_t {
    ARC,
    TWO_Q,
    LFU,
    LIRS
};

template <typename Key, typename Value>
class ICache
{
protected:
    size_t capacity {}; 
    size_t cache_size {}; 
    size_t hits {};
    size_t misses {};

    virtual void _evict() = 0;

    Value slow_get_value(const Key& key)
    {
        return Value{}; 
    }

public:
    ICache(size_t max_size) : capacity{max_size} {};
    virtual ~ICache() = default;

    // Interface for child classes
    virtual void   set(const Key&, const &Value) = 0;
    virtual Value* get(const Key&) = 0;
    virtual bool   remove(const Key&) = 0;
    virtual bool   has(const Key&) const = 0;
    virtual void   clear() = 0; // clearing all cache structures
    virtual bool   cache_lookup_update(const Key&) = 0;
    
    // Getting the current cache state
    void print_stats() const {
       double ratio = (hits + misses == 0) ? 0.0 : (double)hits / (hits + misses) * 100;
        std::cout << "Stats -> Hits: " << hits << ", Misses: " << misses 
                  << ", Hit Ratio: " << ratio << "%\n";
    }
};
