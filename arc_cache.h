#pragma once

#include "base_cache.h"

template <typename Key, typename Value>
class ARCCache : public ICache
{
    size_t t1_capacity_;   
    size_t b1_capacity_;  
    size_t t2_capacity_;  
    size_t b2_capacity_;  
    
    size_t t1_current_  {};   
    size_t b1_current_  {};   
    size_t t2_current_  {};   
    size_t b2_current_  {};   
    
    enum class Location : unsigned char
    {
        T1, // recent data 
        B1, // the ghosts of recent data
        T2, // frequent data 
        B2  // the ghosts of frequent data
    };

    using CacheList = std::list<Key>;
    using ListIterator = typename CacheList::iterator;

    struct Hash_Element 
    {
        std::optional<Value> value;              // nullopt for B1 и B2
        Location location;                       // where is key now
        std::optional<ListIterator> list_opt_it; // iter on node in list
    };

    std::unordered_map<Key, Hash_Element> hash_table_;

    CacheList t1_list_; // LRU for recent data
    CacheList b1_list_; // LRU for the ghosts of recent data
    CacheList t2_list_; // LRU for frequent data
    CacheList b2_list_; // LRU for the ghosts of frequent data

    struct List_Data
    {
        CacheList& list_;
        size_t& current_size_;
    };

    std::array<List_Data, 4> lists_data_array;

    double p_ {}; // adaptation parameter

    /*
      Invoke if:
        1. t1_current_ + t2_current_ == capacity and
        2. requested elem require place in ram: 
            a) it is a new elem
            b) it is a ghost
    */
    void Replace(const Key& key)
    {
        auto hash_it = hash_table_.find(key);
    
        // Displacing from t1 if:
        // 1. t1_current_ > p_ or
        // 2. current key is in B2 and T1 isn't empty
        bool evict_from_t1 = (t1_current_ > 0) && 
                            ((t1_current_ > p_) || 
                            (hash_it != hash_table_.end() && hash_it->second.location == Location::B2));

        if (evict_from_t1)
            CreateGhost(t1_list_, t1_current_, b1_list_, b1_current_, Location::B1);
        
        else
            CreateGhost(t2_list_, t2_current_, b2_list_, b2_current_, Location::B2);
    }

    void CreateGhost(CacheList& t_list_, size_t& t_current_, CacheList& b_list_, size_t& b_current_, Location target_location)
    {
        assert(target_location == Location::B1 || target_location == Location::B2);

        Key new_b = t_list_.back();
        t_list_.pop_back();
        t_current_--;

        auto new_b_hash_it = hash_table_.find(new_b);
        assert(new_b_hash_it != hash_table_.end());

        b_list_.push_front(new_b);
        b_current_++;

        Hash_Element& new_b_hash = new_b_hash_it->second;

        new_b_hash.location = target_location;
        new_b_hash.list_opt_it  = b_list_.begin();
        new_b_hash.value = std::nullopt;
    }

    void MoveGhostToT2(const Key& key, Hash_Element& hash_elem, Value rom_value)
    {
        CacheList& b_list_ = (hash_elem.location == Location::B1) ? b1_list_ : b2_list_;
        size_t& b_current_ = (hash_elem.location == Location::B1) ? b1_current_ : b2_current_;

        // remove from ghosts list
        assert(hash_elem.list_opt_it != std::nullopt);
        b_list_.erase(*(hash_elem.list_opt_it));
        b_current_--;

        // free up 1 slot in RAM if the memory is full
        if (t1_current_ + t2_current_ == this->capacity)
            Replace(key);

        // insert into T2
        t2_list_.push_front(key);
        t2_current_++;

        // update data
        hash_elem.location    = Location::T2;
        hash_elem.list_opt_it = t2_list_.begin();
        hash_elem.value       = std::move(rom_value);
    }

    void RemoveTheOldestGhost(CacheList& b_list, size_t& b_current_)
    {
        Key oldest_b = b_list.back();
        hash_table_.erase(oldest_b);
        b_list.pop_back();
        b_current_--;
    } 

    void FreeUpSlot(const Key& key)
    {
        size_t l1_size = t1_current_ + b1_current_;
        if (l1_size == this->capacity) 
        {
            if (b1_current_ > 0) 
                RemoveTheOldestGhost(b1_list, b1_current_); 
        }
        else if (t1_current_ + b1_current_ + t2_current_ + b2_current_ == 2 * this->capacity) 
        {
            if (b2_current_ > 0)
                RemoveTheOldestGhost(b2_list, b2_current_);
        }

        if (t1_current_ + t2_current_ == this->capacity)
            Replace(key); // free up RAM  
    }

    void CorrectP_(const Hash_Element& hash_elem)
    {
        if (hash_elem.location == Location::B1) 
            p_ = std::min(this->capacity, p_ + std::max(1, b2_current_/b1_current_));
        else if (hash_elem.location == Location::B2) 
            p_ = std::max(0.0, p_ - std::max(1, b1_current_/b2_current_));
    }

    void InsertNew(const Key& key, Value value)
    {
        FreeUpSlot(key);
        
        t1_list_.push_front(key);
        t1_current_++;

        hash_table_[key] = Hash_Element {std::move(value), Location::T1, t1_list_.begin()};
    }

    // 2. Вспомогательный метод для классического кэш-хита (T1 или T2)
    void HitUpdate(const Key& key, Hash_Element& hash_elem)
    {
        if (hash_elem.location == Location::T1)
        {
            t1_list_.erase(*(hash_elem.list_opt_it));
            t2_list_.push_front(key);
            hash_elem.location = Location::T2;
            t1_current_--;
            t2_current_++;
        }
        else 
        {
            t2_list_.splice(t2_list_.begin(), t2_list_, *(hash_elem.list_opt_it));
        }

        hash_elem.list_opt_it = t2_list_.begin();
    }

    // 3. Вспомогательный метод для хита по призракам (B1 или B2)
    void GhostUpdate(const Key& key, Hash_Element& hash_elem, Value value)
    {
        CorrectP_(hash_elem);
            
        if (t1_current_ + t2_current_ == this->capacity)
            Replace(key);
            
        MoveGhostToT2(key, hash_elem, std::move(value));
    }

public:
    explicit ARCCache(size_t capacity) : ICache<Key, Value>(capacity), p_{0},
        lists_data_array{{
                {t1_list_, t1_current_}, 
                {b1_list_, b1_current_}, 
                {t2_list_, t2_current_}, 
                {b2_list_, b2_current_}  
            }} {};

    void set(const Key& key, const Value& value) override 
    {
        auto hash_elem_it = hash_table_.find(key);

        // ********** Get a new element (miss) **********
        if (hash_elem_it == hash_table_.end())
        {
            InsertNew(key, value);
            return;
        }

        Hash_Element& hash_elem = hash_elem_it->second;

        // ********** Cache-hit **********
        if (hash_elem.location == Location::T1 || hash_elem.location == Location::T2)
        {
            hash_elem.value = value;

            HitUpdate(key, hash_elem);
            return;
        }

        // ********** Hit in ghost (actually miss) **********

        GhostUpdate(key, hash_elem, value);
    }

    Value* get(const Key& key) override 
    {
        auto hash_elem_it = hash_table_.find(key);

        // ********** Get a new element (miss) **********
        if (hash_elem_it == hash_table_.end())
        {
            this->misses++;
            Value rom_value = this->slow_get_value(key);

            InsertNew(key, std::move(rom_value));
            return &*(hash_elem_it->second.value);
        }
    
        Hash_Element& hash_elem = hash_elem_it->second;

        // ********** Cache-hit **********
        if (hash_elem.location == Location::T1 || hash_elem.location == Location::T2)
        {
            this->hits++;
            HitUpdate(key, hash_elem);
            return &*(hash_elem.value);
        }

        // ********** Hit in ghost (actually miss) **********
        this->misses++;
        Value rom_value = this->slow_get_value(key);

        GhostUpdate(key, hash_elem, std::move(rom_value));
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
        CacheList& target_list = lists_data_array[static_cast<size_t>(hash_elem.location)].list_;
        size_t&    list_size   = lists_data_array[static_cast<size_t>(hash_elem.location)].current_size_;
        target_list.erase(*(hash_elem.list_opt_it));
        list_size--;
        
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

        t1_current_ = 0;   
        b1_current_ = 0;   
        t2_current_ = 0;   
        b2_current_ = 0;   

        t1_list_.clear();
        b1_list_.clear();
        t2_list_.clear();
        b2_list_.clear();

        hash_table_.clear();
    }

};