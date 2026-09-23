#pragma once

#include "base_cache.h"

template <typename Key, typename Value>
class LFUCache : public ICache
{
    struct CacheNode 
    {
        Value value;
        size_t frequency;
        // An iterator that looks directly at the same Key inside the list freq_to_keys.
        typename std::list<Key>::iterator node_iterator; 
    };

    // Main storage
    std::unordered_map<Key, CacheNode> cache_storage;

    // A map where for each frequency there is a doubly linked list of keys 
    std::map<size_t, std::list<Key>> freq_to_keys;

    void _evict() override 
    {
        assert(this->cache_size == this->capacity); 

        // get an iterator for the first (minimum in frequency) element
        auto min_freq_it = freq_to_keys.begin();
        // get the list of keys for this frequency
        auto& keys_list = min_freq_it->second;
        // take the oldest key from the list
        auto deleted_elem_key = min_freq_it->second.front();
        // remove this key from the frequency list
        cache_storage.erase(deleted_elem_key);
        keys_list.pop_front();

        if (keys_list.empty()) {
            freq_to_keys.erase(min_freq_it); 
        }

        this->cache_size--;
    }

    // Updating frequency for element
    void _update_frequency(typename std::unordered_map<Key, CacheNode>::iterator storage_it) 
    {
        assert(storage_it);

        Key key = storage_it->first;
        CacheNode& node = storage_it->second;
        size_t old_freq = node.frequency;

        assert(old_freq);
        
        // 1. remove from the old list by iterator
        freq_to_keys[old_freq].erase(node.node_iterator);
        if (freq_to_keys[old_freq].empty()) {
            freq_to_keys.erase(old_freq); 
        }

        // 2. Incrementing the frequency
        node.frequency++;
        size_t new_freq = node.frequency;

        // 3. Adding it to the end of the list for the new frequency
        freq_to_keys[new_freq].push_back(key);

        // 4. Updating the iterator inside the node
        node.node_iterator = --freq_to_keys[new_freq].end();
    }


public:
    explicit LFUCache(size_t capacity) : ICache<Key, Value>(capacity) {};

    void set(const Key& key, const Value& value) override 
    {
        auto storage_it = cache_storage.find(key);
        
        // The item is not in the cache - adding a new one
        if (storage_it == cache_storage.end()) 
        {
            if (this->capacity == 0) return; // Защита от нулевого кэша

            // evict the old element if the cache is full
            if (this->cache_size >= this->capacity) {
                _evict();
            }

            auto& list_ref = freq_to_keys[1];
            list_ref.push_back(key);
            
            // get an iterator for the key that was just added to the end
            auto list_it = --list_ref.end();

            // insert it into the main storage
            cache_storage.emplace(key, CacheNode{value, 1, list_it});
            return;
        }

        // If the item is already in the cache - updating the value
        storage_it->second.value = value; 
        _update_frequency(storage_it);
    }


    Value* get(const Key& key) override 
    {
        auto storage_it = cache_storage.find(key);
        
        // if there is no such element inside the cache
        if (storage_it == cache_storage.end()) 
        {
            this->misses++; 
            Value rom_value = this->slow_get_value(key);

            this->set(key, std::move(rom_value));

            storage_it = hash_table_.find(key);
        }

        else
        {
            this->hits++;
        }

        return &(storage_it->second.value);
    }

    bool  remove(const Key& key) override 
    {
        auto storage_it = cache_storage.find(key);
        
        // if there is no such element inside the cache
        if (storage_it == cache_storage.end()) {
            return false; 
        }

        // removing an item from the list map freq_to_keys
        const CacheNode& node = storage_it->second;
        size_t freq = node.frequency;
        
        freq_to_keys[freq].erase(node.node_iterator);

        if (freq_to_keys[freq].empty()) {
            freq_to_keys.erase(freq);
        }

        // removing an item from the main cache storage
        cache_storage.erase(storage_it);

        return true;
    }

    bool  has(const Key& key) const override 
    {
        return cache_storage.contains(key);
    }

    void clear() override
    {
        this->cache_size    = 0;
        this->hits          = 0;
        this->misses        = 0;
        
        freq_to_keys.clear();
        cache_storage.clear();
    }
};