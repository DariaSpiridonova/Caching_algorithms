// Calling a file not written to the cache (Non_Resident_HIR) but presented in the stack
            /*
                1. looking for in the stack_
                2. move into the top and set LIR status
                3. if (current_lirs_ == lirs_limit_ )
                    the element EL from the bottom of the stack_ make Resident_HIR
                    remove EL from the stack_ + prune_stack() and move into the queue_ 
                    4. if (current_hirs_ == hirs_limit_) remove last queue_ elem EL1 
                        if (EL1 is in stack_) make EL1.state = Non_Resident_HIR
            */
            else if (stack_elem.state == State::Non_Resident_HIR)
            {
                // set LIR status
                stack_elem.state = State::LIR;

                // move into the top
                stack_.splice(stack_.begin(), stack_, stack_it);

                if (current_lirs_ == lirs_limit_)
                {
                    // get the element EL from the bottom of the stack_
                    Cache_Node& new_res_hirs = stack_.back();

                    // change state
                    new_res_hirs.state = State::Resident_HIR;
                    // add into the queue_
                    queue_.emplace_front(new_res_hirs);

                    // find 
                    auto hirs_hash_it = hash_table_.find(new_res_hirs.key);
                    // set iterators into nullopt 
                    if (hirs_hash_it != hash_table_.end())
                    {
                        hirs_hash_it->second.stack_it = std::nullopt; 
                        res_hash_it->second.queue_it = queue_.begin();  
                    }

                    // delete from stack_
                    prune_stack();

                    if (current_hirs_ == hirs_limit_)
                    {
                        // get excess HIRS - EL1
                        Cache_Node& deleted_hirs = queue_.back();

                        // find in hash_table_ and get an iterator
                        auto hash_it = hash_table_.find(deleted_hirs.key);
                        assert(hash_it != hash_table_.end());

                        // get structure from iterator
                        Hash_Element& deleted_hirs_elem = hash_it->second;

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
                    }
                    else
                    {
                        current_hirs_++;
                    }
                }
                else
                {
                    current_lirs_++;
                }
            }