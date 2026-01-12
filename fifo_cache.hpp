#include <iostream>
#include <unordered_map>
#include <queue>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include "persistent_db.hpp"

class FIFOCache {
private:
    size_t current_size = 0;
    const size_t MAX_SIZE = 50; //bytes
    int capacity;

    std::unordered_map<std::string, std::string> cache; // cache holds the keys and values
    std::queue<std::string> queue; // fifo queue holds the keys in the cache
    SQLiteDB db; // persistent storage
    
    mutable std::shared_mutex cache_mutex;
    
public:
    FIFOCache() : capacity(INT_MAX) {} // cache can hold any number of keys (constrained by MAX_SIZE)
    
    /// GET method for accessing elements from key-value store
    /// Checks cache first, then database. Caches database hits
    /// @returns (key, value) pair if found, ("", "") otherwise
    std::pair<std::string, std::string> get(const std::string& key) {
        // Check cache
        {
            std::shared_lock<std::shared_mutex> cache_lock(cache_mutex); // read lock
            auto it = cache.find(key);
            // cache hit
            if (it != cache.end()) {
                return std::make_pair(it->first, it->second);
            }
        }

        // Check DB
        {
            auto value_opt = db.get_from_db(key);
            // db hit
            if (value_opt.first) {
                std::string value = value_opt.second;
                insertToCache(key, value);
                return std::make_pair(key, value);
            }
        }
        
        return {"", ""};
    }
    
    /// PUT method for inserting and updating values
    /// Does not allow inserting empty strings as keys (values can be empty)
    /// Puts every new pair to database first then inserts to cache
    void put(const std::string& key, const std::string& value) {
        if(key == ""){
            return;
        }
        db.put_to_db(key, value);
        insertToCache(key, value);
    }
    
    /// DELETE method for removing a key-value pair from cache and DB
    /// @returns true if remove successful, false otherwise
    bool remove(const std::string& key) {
        bool removed_from_db = db.remove_from_db(key); // remove from DB
        bool removed_from_cache = false;
        
        // Remove from cache
        {
            std::unique_lock<std::shared_mutex> cache_lock(cache_mutex); // write lock
            auto it = cache.find(key);
            if (it != cache.end()) {
                current_size -= (it->first.size() + it->second.size()); 
                cache.erase(it); // remove from cache
                removed_from_cache = true; 
            }
            
            // extract all elements to a vector first
            std::vector<std::string> queue_elements;
            while (!queue.empty()) {
                queue_elements.push_back(queue.front());
                queue.pop();
            }
            
            // rebuild queue excluding the removed key
            for (const auto& elem : queue_elements) {
                if (elem != key) {
                    queue.push(elem);
                }
            }
        }
        
        return removed_from_db || removed_from_cache; // a record can only be in db (not in cache) or both 
    }
    
    /// Helper method for GET and PUT
    /// Inserts new records to cache
    /// If cache is full, evicts oldest element then inserts new
    void insertToCache(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex); // write lock
        
        size_t value_size = key.size() + value.size();
        if(value_size > MAX_SIZE){
            return; // can not cache 
        }

        // if key exists
        auto it = cache.find(key);
        if(it != cache.end()){
            current_size -= (it->first.size() + it->second.size()); 
        }

        // evict until cache have enough space
        while (current_size + value_size > MAX_SIZE && !queue.empty()) {
            std::string oldest = queue.front();
            queue.pop();

            //check if oldest exists (to prevent seg. fault if another thread deletes it in the meantime)
            auto oldest_it = cache.find(oldest);
            if(oldest_it != cache.end()){
                current_size -= (oldest.size() + cache[oldest].size());
                cache.erase(oldest);
            }
        }
        
        // add new entry to queue and cache
        if (cache.find(key) == cache.end()) {
            queue.push(key);
        }
        cache[key] = value;
        current_size += value_size;
    }

    void displayCache() {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
        
        std::cout << "--- Cache State ---" << std::endl;
        std::cout << "Capacity: " << capacity << std::endl;
        std::cout << "Current Size: " << current_size << " bytes" << std::endl;
        std::cout << "Cache Contents:" << std::endl;
        
        for (const auto& [key, value] : cache) {
            std::cout << "  " << key << " -> " << value << std::endl;
        }
        
        std::cout << "FIFO Queue Order: ";
        std::queue<std::string> temp_queue = queue;
        while (!temp_queue.empty()) {
            std::cout << temp_queue.front() << " ";
            temp_queue.pop();
        }
        std::cout << std::endl << std::endl;
    }
};
