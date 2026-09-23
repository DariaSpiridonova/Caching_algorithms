#pragma once

#include "base_cache.h"

template <typename Key, typename Value>
class LIRSCache : public ICache
{
    size_t lirs_limit_;    // maximum number of     LIR      elements
    size_t hirs_limit_;    // maximum number of Resident HIR elements  

    size_t current_lirs_ {}; // Current     LIR      number
    size_t current_hirs_ {}; // Current Resident HIR number 

    // A set of possible states of an object
    enum class State 
    {
        LIR,  
        Resident_HIR, 
        Non_Resident_HIR
    };
    
    // Storage's elements
    // BOTTOM - back
    // TOP    - front
    struct Cache_Node
    {
        Key key;
        State state;
    };
    
    using CacheList = std::list<Cache_Node>;
    using ListIterator = typename CacheList::iterator;
    
    struct Hash_Element
    {
        std::optional<Value> value; 
        
        std::optional<ListIterator> stack_it; 
        std::optional<ListIterator> queue_it; 
    };

    // Storages
    std::unordered_map<Key, Hash_Element> hash_table_;
    CacheList stack_;
    CacheList queue_;

    void prune_stack()
    {
        // As long as the stack is not empty and there is no LIR at the bottom
        while (!stack_.empty() && stack_.back().state != State::LIR)
        {
            Key key_to_remove = stack_.back().key;
            
            // Making the pointer to the stack null in the hash table
            auto it = hash_table_.find(key_to_remove);
            if (it != hash_table_.end())
            {
                it->second.stack_it = std::nullopt;
                
                // If the item has become a Non-Resident HIR And is no longer on the stack,
                // it is erased from the hash table
                if (it->second.value == std::nullopt && it->second.queue_it == std::nullopt)
                    hash_table_.erase(it);
            }
            
            stack_.pop_back(); // remove from the bottom
        }
    }

    void CheckCache()
    {
        if (current_lirs_ > lirs_limit_)
        {
            ChangeLIRToHIR();
            
            if (current_hirs_ > hirs_limit_)
            {
                ChangeHIRToNonResOrDelete();
            }

            assert(current_lirs_ <= lirs_limit_ && current_hirs_ <= hirs_limit_);
        }
    }

    void CreateNewLIRS(const Key& key, const Value& value)
    {
        Cache_Node new_cache_node {key, State::LIR};

        stack_.push_front(new_cache_node);
        Hash_Element new_elem {value, stack_.begin(), std::nullopt};

        hash_table_[key] = new_elem;
    }

    void CreateNewHIRS(const Key& key, const Value& value)
    {
        Cache_Node new_cache_node {key, State::Resident_HIR};

        Cache_Node new_cache_node {key, State::Resident_HIR};

        stack_.push_front(new_cache_node);
        queue_.push_front(new_cache_node);
        Hash_Element new_elem {value, stack_.begin(), queue_.begin()};

        hash_table_[key] = new_elem;
        current_hirs_++;
        this->cache_size++;

        if (current_hirs_ > hirs_limit_)
        {
            ChangeHIRToNonResOrDelete();
        }
    }

    void CorrectElemState(Hash_Element& hash_elem)
    {
        std::optional<ListIterator> stack_opt_it = hash_elem.stack_it;
        std::optional<ListIterator> queue_opt_it = hash_elem.queue_it;

        if (stack_opt_it != std::nullopt)
        {
            ListIterator stack_it = *stack_opt_it;
            
            Cache_Node& stack_elem = *stack_it;
            
            if (stack_elem.state == State::LIR)
            {
                stack_.splice(stack_.begin(), stack_, stack_it);
                prune_stack();
            }
            else // for HIRS elem which is in stack_
            {
                assert(queue_opt_it != std::nullopt);

                // set LIR status
                stack_elem.state = State::LIR;
                current_lirs_++;
                current_hirs_--;

                // move into the top
                stack_.splice(stack_.begin(), stack_, stack_it);
                hash_elem.stack_it = stack_.begin();

                // remove fron queue_ 
                queue_.erase(*(hash_elem.queue_it));
                hash_elem.queue_it = std::nullopt;

                CheckCache();
            }
            
            
        }
        else // queue_opt_it != std::nullopt && stack_elem.state = State::Resident_HIR
        {
            assert(queue_opt_it != std::nullopt);

            ListIterator queue_it = *queue_opt_it;
            
            Cache_Node& queue_elem = *queue_it;
            assert(queue_elem.state == State::Resident_HIR);

            // move on the queue_ top
            queue_.splice(queue_.begin(), queue_, queue_it);
            hash_elem.queue_it = queue_.begin();

            // add to the stack_
            stack_.push_front(queue_elem);
            hash_elem.stack_it = stack_.begin();
        }
    }

    void ChangeLIRToHIR()
    {
        Cache_Node new_res_hirs = stack_.back();
                    
        new_res_hirs.state = State::Resident_HIR;
        // add into the queue_
        queue_.emplace_front(new_res_hirs);

        // find 
        auto hirs_hash_it = hash_table_.find(new_res_hirs.key);
        assert(hirs_hash_it != hash_table_.end());
        // set iterators  
        hirs_hash_it->second.stack_it = std::nullopt; 
        hirs_hash_it->second.queue_it = queue_.begin();  

        // delete from stack_
        prune_stack();

        current_lirs_--;
        current_hirs_++;
    }

    void ChangeHIRToNonResOrDelete()
    {
        Cache_Node old_hirs = queue_.back();
            
        // find in hash_table_ and get an iterator
        auto hash_it = hash_table_.find(deleted_hirs.key);
        assert(hash_it != hash_table_.end());

        // get structure from iterator
        Hash_Element deleted_hirs_elem = hash_it->second;

        std::optional<ListIterator> del_hirs_opt_it = deleted_hirs_elem.stack_it;
        if (del_hirs_opt_it != std::nullopt)
        {
            ListIterator del_hirs_stack_it = *del_hirs_opt_it;

            del_hirs_stack_it->state = State::Non_Resident_HIR;
            deleted_hirs_elem.value = std::nullopt;
        
            deleted_hirs_elem.queue_it = std::nullopt;
        }
        else // remove element from hash_table_
        {
            hash_table_.erase(hash_it);
        }

        // remove deleted_hirs from queue_ bottom
        queue_.erase(std::prev(queue_.end()));

        current_hirs_--;
        this->cache_size--;
    }

public:
    explicit LIRSCache(size_t capacity) : ICache<Key, Value>(capacity)
    {
        lirs_limit_ = (capacity > 1) ? static_cast<size_t>(capacity * 0.95) : 1;
        hirs_limit_ = capacity - lirs_limit_;
    }

    void set(const Key& key, Value value) override 
    {
        auto hash_it = hash_table_.find(key);

        // if we get a new element 
        if (hash_it == hash_table_.end())
        {
            if (current_lirs_ < lirs_limit_)
            {
                CreateNewLIRS(key, value);
            }
            else
            {
                CreateNewHIRS(key, value);
            }
            return;
        }
        Hash_Element& elem = hash_it->second;
        elem.value = std::move(value);

        auto stack_opt_it = elem.stack_it; 

        if (stack_opt_it != std::nullopt)
        {
            auto stack_it = *stack_opt_it;
            Cache_Node& stack_elem = *stack_it;
            if (stack_elem.state == State::Non_Resident_HIR)
            {
                stack_elem.state = State::LIR;
                current_lirs_++;
                this->cache_size++;

                stack_it = stack_.begin();
                CheckCache();  
            }
        }
    }

    Value* get(const Key& key) override 
    {
        auto hash_elem_it = hash_table_.find(key);

        // if there is no such element inside hash_table_ or it is a ghost
        if (hash_elem_it == hash_table_.end() || hash_elem_it->second.value == std::nullopt)
        {
            this->misses++;
            // Value rom_value = this->slow_get_value(key);

            // this->set(key, std::move(rom_value));

            // hash_elem_it = hash_table_.find(key);
        }

        else
        {
            this->hits++;
    
            Hash_Element& hash_elem = hash_elem_it->second;
            
            CorrectElemState(hash_elem);
        }

        return &(hash_elem_it->second.value());
    }

    bool  remove(const Key& key) override 
    {
        auto hash_it = hash_table_.find(key);

        // if there is no such element in hash_table_
        if (hash_it == hash_table_.end())
            return false;
        
        Hash_Element& hash_elem = hash_elem_it->second;
        hash_elem.value = std::nullopt;

        std::optional<ListIterator> stack_opt_it = hash_elem.stack_it;
        std::optional<ListIterator> queue_opt_it = hash_elem.queue_it;

        if (stack_opt_it != std::nullopt)
        {
            ListIterator stack_iter = *stack_opt_it;
            if (stack_iter->state == State::LIR)
            {
                current_lirs_--;
                this->cache_size--;
            }
            stack_.erase(stack_iter);
            prune_stack();
        }
        if (queue_opt_it != std::nullopt)
        {
            ListIterator queue_iter = *queue_opt_it;
            assert(queue_iter->state == State::Resident_HIR);
            current_lirs_--;
            this->cache_size--;

            queue_.erase(queue_iter);
        }

        hash_elem.stack_it = std::nullopt;
        hash_elem.queue_it = std::nullopt;

        hash_table_.erase(hash_it);
        return true;
    }

    bool  has(const Key& key) const override 
    {
        return std::ranges::contains_if(stack_, [&key] (const Cache_Node& node) {return node.key == key;}) || 
               std::ranges::contains_if(queue_, [&key] (const Cache_Node& node) {return node.key == key;});
    }

    void clear() override
    {
        this->cache_size = 0;
        this->hits       = 0;
        this->misses     = 0;

        lirs_limit_      = 0;
        hirs_limit_      = 0;
        current_lirs_    = 0;
        current_hirs_    = 0;
        
        stack_.clear();
        queue_.clear();

        hash_table_.clear();
    }
};
