#pragma once

#include "base_cache.h"

const char* MUL_CACHE_INFO_FILENAME {"multi_cache_levels.txt"};

enum class CacheType : std::uint8_t 
{
    ARC,
    TWO_Q,
    LFU,
    LIRS
};

struct LayerConfig 
{
    CacheType type;
    size_t capacity;
};

std::vector<std::unique_ptr<ICache>> levels;

class MultiCache
{
protected:
    int levels_num {};
    std::vector<CacheType> active_caches;
    int cache_size {};

public:
    MultiCache(pair<int, vector<CacheType>> info) : levels_num {info.first}, active_caches {info.second} 
    {
        for (auto type : info.second) 
        {
            switch (type)
            {
            case CacheType::ARC:
                levels.push_back(std::make_unique<ARCCache>(cache_size));
                break;
            case CacheType::TWO_Q:
                levels.push_back(std::make_unique<TwoQCache>(cache_size)); // Исправлено тип кэша
                break;
            case CacheType::LFU:
                levels.push_back(std::make_unique<LFUCache>(cache_size));  // Исправлено тип кэша
                break;
            case CacheType::LIRS:
                levels.push_back(std::make_unique<LIRSCache>(cache_size)); // Исправлено тип кэша
                break;
            }
        }

    };
};

