#pragma once 

#include "base_cache.h"

export enum class CacheType : std::uint8_t 
{
    ARC, 
    TWO_Q, 
    LFU, 
    LIRS
};

std::vector<CacheType> ReadCacheLevelsInfo(const char *filename);

template <typename Key, typename Value>
void RunCacheSimulation(const std::vector<CacheType>& cache_types);

template <typename Key, typename Value>
class MultiCache
{
    std::vector<std::unique_ptr<ICache<Key, Value>>> levels;
    unsigned long long total_hits = 0; 

    using RamVictim_t = typename ICache<Key, Value>::RamVictim_t;

    std::function<Value(const Key&)>              slow_get_from_rom;
    std::function<void(const Key&, const Value&)> slow_set_to_rom;

    std::unique_ptr<ICache<Key, Value>> CreateCacheInstance(CacheType type, size_t capacity) 
    {
        switch (type) 
        {
            case CacheType::ARC:   
                return std::make_unique<ARCCache<Key, Value>>(capacity);

            case CacheType::TWO_Q: 
                return std::make_unique<TwoQueueCache<Key, Value>>(capacity);

            case CacheType::LFU:   
                return std::make_unique<LFUCache<Key, Value>>(capacity);

            case CacheType::LIRS:  
                return std::make_unique<LIRSCache<Key, Value>>(capacity);

            default: 
                throw std::runtime_error("Unknown cache type");
        }
    }
   
public:
    MultiCache(const std::vector<CacheType>& types, 
               size_t base_capacity,
               std::function<Value(const Key&)> rom_reader,
               std::function<void(const Key&, const Value&)> rom_writer)
        : slow_get_from_rom(rom_reader), slow_set_to_rom(rom_writer)
    {
        size_t current_capacity = base_capacity;
        
        // Fill vector of levels
        for (const auto& type : types) 
        {
            levels.push_back(create_cache_instance(type, current_capacity));
            // N level capacity = base_capacity * 2**N;
            current_capacity *= 2; 
        }
    }

    void cascade_set(const Key& key, const Value& value) 
    {
        Key   current_key   = key;
        Value current_value = value;

        // Идем строго сверху вниз: L1 -> L2 -> L3...
        for (size_t i = 0; i < levels.size(); ++i) 
        {
            // store data on the level i and get displaced victim if it exist 
            RamVictim_t evicted = levels[i]->set(current_key, current_value);
            
            if (!evicted.has_value()) 
                return; 
            
            // If level i is overflowed and the "victim" is displaced,
            // it becomes a new candidate for falling to a level below L(i+1)
            current_key = evicted->first;
            current_value = evicted->second;
        }

        // If the cycle has ended and the last level has pushed the victim outside,
        // adding it to the ROM.
        slow_set_to_rom(current_key, current_value);
    }


    Value get(const Key& key) 
    {
        for (size_t i = 0; i < levels.size(); ++i) 
        {
            Value* cached_ptr = levels[i]->get(key);
            if (cached_ptr != nullptr)
            {
                this->total_hits++; 
                return *cached_ptr; // found
            }
        }

        // miss
        Value rom_value = slow_get_from_rom(key);
        this->cascade_set(key, rom_value);

        return rom_value;
    }

    unsigned long long get_total_hits() const 
    {
        return total_hits;
    }
};

const vector<string> cache_names = { "ARC", "2Q", "LFU", "LIRS" };

std::vector<CacheType> ReadCacheLevelsInfo(const char *filename)
{
    int levels_num {};
    ifstream infile(filename);

    if (!infile.is_open()) {
        throw runtime_error("Couldn't open the file with cache levels");
    }
    if (!(infile >> levels_num) || levels_num < 0) {
        throw std::runtime_error("Incorrect data format in the file with cache levels");
    }

    vector<CacheType> active_caches;
    active_caches.reserve(levels_num); 

    // Read levels_num caches strings 
    string cache_name;
    for (int i = 0; i < levels_num; i++) 
    {
        if (!(infile >> cache_name)) {
            throw runtime_error(format("A file with cache levels info contains less then {} caches names", levels_num));
        }

        // Looking for a string in the vector of caches names
        auto it = std::find(cache_names.begin(), cache_names.end(), cache_name);
        
        if (it != cache_names.end()) {
            // find the index of the match
            auto index = std::distance(cache_names.begin(), it);
            // set the index to the CacheType type and save
            active_caches.push_back(static_cast<CacheType>(index));
        } else {
            throw runtime_error("Unknown cache type was found in the file");
        }
    }
    return active_caches;
}

template <typename Key, typename Value>
void RunCacheSimulation(const std::vector<CacheType>& cache_types) 
{
    size_t base_capacity = 0;
    size_t data_size = 0;

    if (!(std::cin >> base_capacity >> data_size)) 
        throw runtime_error("Incorrect values of base_capacity and data_size"); 

    auto rom_reader = [](const Key& key) { return Value {}; };
    auto rom_writer = [](const Key& key, const Value& value) {};

    MultiCache<Key, Value> multi_cache(cache_types, base_capacity, rom_reader, rom_writer);

    // Get keys and find values
    Key key;
    for (size_t i = 0; i < data_size; ++i) 
    {
        if (std::cin >> key) 
            multi_cache.get(key);
        else
            throw runtime_error("Incorrect key"); 
    }

    // Output the total number of hits
    std::cout << multi_cache.get_total_hits() << std::endl;
}