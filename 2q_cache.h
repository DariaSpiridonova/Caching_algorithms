#pragma once

#include "base_cache.h"

template <typename Key, typename Value>
class TwoQCache : public ICache
{
    // Variables
    size_t a1_in_capacity_;   
    size_t a1_out_capacity_;  
    size_t am_capacity_;

    size_t a1_in_current_  {};   
    size_t a1_out_current_ {};  
    size_t am_current_     {};

    using CacheList = std::list<Key>; 
    using ListIterator = typename CacheList::iterator;

    // in which list is the element stored
    enum class Location 
    {
        A1_IN,
        A1_OUT,
        AM
    };

    struct Hash_Element
    {
        std::optional<Value> value; 
        Location location;                
        std::optional<ListIterator> list_opt_it;       
    };

    // cache storages
    std::unordered_map<Key, Hash_Element> hash_table_;
    CacheList a1_in_list;
    CacheList a1_out_list;
    CacheList am_list;

    void CheckCache()
    {
        if (a1_in_current_ > a1_in_capacity_)
        {
            Key new_a1_out_key = a1_in_list.back();
            a1_in_list.pop_back();
            a1_in_current_--;

            a1_out_list.emplace_front(new_a1_out_key);
            a1_out_current_++;

            auto hash_it = hash_table_.find(new_a1_out_key);
            assert(hash_it != hash_table_.end());
            
            hash_it->second.location    = Location::A1_OUT; 
            hash_it->second.list_opt_it = a1_out_list.begin(); 
            hash_it->second.value       = std::nullopt; 

            if (a1_out_current_ > a1_out_capacity_)
            {
                Key oldest_ghost = a1_out_list.back();
                hash_table_.erase(oldest_ghost); 
                a1_out_list.pop_back();
                a1_out_current_--;
            }
        }

        assert(a1_in_current_ <= a1_in_capacity_ && a1_out_current_ <= a1_out_capacity_);
    }

    void CreateNewA1In(const Key& key, const Value& value)
    {
        a1_in_list.push_front(key);
        Hash_Element new_elem {value, Location::A1_IN, a1_in_list.begin()};
        hash_table_[key] = new_elem;
        a1_in_current_++;

        CheckCache();
    }

    void MoveGhostToAm(const Key& key, Hash_Element& hash_elem, const Value& rom_value)
    {
        // remove from A1_OUT
        assert(hash_elem.list_opt_it != std::nullopt);
        a1_out_list.erase(*(hash_elem.list_opt_it));
        a1_out_current_--;
        am_current_++;

        if (am_current_ > am_capacity_) 
        {
            Key oldest_am_key = am_list.back();
            hash_table_.erase(oldest_am_key);
            am_list.pop_back();
            am_current_--;
        }

        // insert into AM
        am_list.push_front(key);
        
        // update hash data for elem
        hash_elem.value = std::move(rom_value);
        hash_elem.location = Location::AM;
        hash_elem.list_opt_it = am_list.begin();
    }


    void CorrectElemLocation(Hash_Element& hash_elem)
    {
        assert(hash_elem.list_opt_it != std::nullopt);
        ListIterator list_it = hash_elem.list_opt_it();
        if (hash_elem.location == Location::AM)
        {
            splice(am_list.begin(), am_list, list_it);
        }
    }

public:
    explicit TwoQCache(size_t capacity) : ICache<Key, Value>(capacity) 
    {
        if (capacity < 2) {
            this->capacity = 2; 
        }

        // list of new elems
        a1_in_capacity_ = capacity / 4;
        if (a1_in_capacity_ == 0) a1_in_capacity_ = 1;

        // elems that have been requested more than 1 time
        am_capacity_ = capacity - a1_in_capacity_;

        // ghosts (elems without values)
        a1_out_capacity_ = capacity / 2;
        if (a1_out_capacity_ == 0) a1_out_capacity_ = 1;
        
    };

    void set(const Key& key, const Value& value) 
    {
        auto it = hash_table_.find(key);

        if (it == hash_table_.end()) 
        {
            // create elem in A1_IN
            CreateNewA1In(key, value);
        } 
        else if (it->second.location == Location::A1_OUT) 
        {
            MoveGhostToAm(key, it->second, value); 
        } 
        else 
        {
            // elem is in RAM (in A1_IN or AM) — just update value
            it->second.value = value;
            CorrectElemLocation(key, it->second);
        }
    }

    Value* get(const Key& key) override 
    {
        auto hash_elem_it = hash_table_.find(key);

        // if we get a new element 
        if (hash_elem_it == hash_table_.end())
        {
            this->misses++;
            Value rom_value = this->slow_get_value(key);
            
            // create a new elem in A1_IN
            CreateNewA1In(key, std::move(rom_value));
            
            // updating the iterator to return the value
            hash_elem_it = hash_table_.find(key);

            return &*(hash_elem_it->second.value);
        }

        Hash_Element& hash_elem = hash_elem_it->second;

        if (hash_elem.location == Location::A1_OUT) 
        {
            this->misses++;
            Value rom_value = this->slow_get_value(key);

            MoveGhostToAm(key, hash_elem, rom_value);

            return &*(hash_elem.value);
        }

        // cache-hit
        this->hits++;
        CorrectElemLocation(key, hash_elem);

        return &*(hash_elem.value);
    }

    bool  remove(const Key& key) override 
    {
        auto hash_it = hash_table_.find(key);

        // if there is no such element in hash_table_
        if (hash_it == hash_table_.end())
            return false;
        
        Hash_Element& hash_elem = hash_it->second;
        
        assert(hash_elem.list_opt_it != std::nullopt);
        switch(hash_elem.location)
        {
            case Location::A1_IN:
                a1_in_list.erase(*(hash_elem.list_opt_it));
                a1_in_current_--;
                break;
            
            case Location::A1_OUT:
                a1_out_list.erase(*(hash_elem.list_opt_it));
                a1_out_current_--;
                break;
            
            case Location::AM:
                am_list.erase(*(hash_elem.list_opt_it));
                am_current_--;
                break;
            
        }

        hash_table_.erase(hash_it);
        return true;
    }

    bool  has(const Key& key) const override 
    {
        return hash_table_.find(key) != hash_table_.end();
    }

    void clear() override
    {
        this->cache_size = 0;
        this->hits       = 0;
        this->misses     = 0;

        a1_in_current_  = 0;   
        a1_out_current_ = 0;  
        am_current_     = 0;

        a1_in_list.clear();
        a1_out_list.clear();
        am_list.clear();

        hash_table_.clear();
    }
};