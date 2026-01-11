#include <iostream>
#include <unordered_map>
#include <queue>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <optional>

class FIFOCache {
private:
    size_t current_size = 0;
    const size_t MAX_SIZE = 50; //bytes
    int capacity;

    std::unordered_map<std::string, std::string> cache;
    std::queue<std::string> queue;
    std::unordered_map<std::string, std::string> db;
    
    mutable std::shared_mutex cache_mutex;
    mutable std::shared_mutex db_mutex;
    
public:
    FIFOCache() : capacity(INT_MAX) {}
    
    std::pair<std::string, std::string> get(const std::string& key) {
        {
            std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
            auto it = cache.find(key);
            if (it != cache.end()) {
                return std::make_pair(it->first, it->second);
            }
        }
        
        {
            std::shared_lock<std::shared_mutex> db_lock(db_mutex);
            auto it = db.find(key);
            if (it != db.end()) {
                std::string value = it->second;
                db_lock.unlock();
                
                insertToCache(key, value);
                return std::make_pair(key, value);
            }
        }
        
        return {"", ""};
    }
    
    void put(const std::string& key, const std::string& value) {
        {
            std::unique_lock<std::shared_mutex> db_lock(db_mutex);
            db[key] = value;
        }
        
        insertToCache(key, value);
    }
    
    // Remove a key-value pair from both cache and DB
    bool remove(const std::string& key) {
        bool removed_from_db = false;
        bool removed_from_cache = false;
        
        // Remove from DB first
        {
            std::unique_lock<std::shared_mutex> db_lock(db_mutex);
            auto it = db.find(key);
            if (it != db.end()) {
                db.erase(it);
                removed_from_db = true;
            }
        }
        
        // Remove from cache
        {
            std::unique_lock<std::shared_mutex> cache_lock(cache_mutex);
            auto it = cache.find(key);
            if (it != cache.end()) {
                current_size -= (it->first.size() + it->second.size()); 
                cache.erase(it);
                removed_from_cache = true;
                
                // Remove from queue - need to rebuild queue without the key
                std::queue<std::string> new_queue;
                while (!queue.empty()) {
                    std::string current = queue.front();
                    queue.pop();
                    if (current != key) {
                        new_queue.push(current);
                    }
                }
                queue = std::move(new_queue);
            }
        }
        
        return removed_from_db || removed_from_cache;
    }
    
    void insertToCache(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex);
        
        size_t value_size = key.size() + value.size();
        if(value_size > MAX_SIZE){
            return; // can not cache 
        }

        // if key exists -> remove from current size 
        auto it = cache.find(key);
        if(it != cache.end()){
            current_size -= (it->first.size() + it->second.size()); 
        }

        // evict until cache have enough space
        while (current_size + value_size > MAX_SIZE && !queue.empty()) {
            std::string oldest = queue.front();
            queue.pop();
            current_size -= (oldest.size() + cache[oldest].size());
            cache.erase(oldest);
        }
        
        // add new entry
        if (cache.find(key) == cache.end()) {
            queue.push(key);
        }
        cache[key] = value;
        current_size += value_size;
    }
    
    void putToDB(const std::string key, const std::string& value) {
        std::unique_lock<std::shared_mutex> db_lock(db_mutex);
        db[key] = value;
    }

    void displayCache() {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex);
        
        std::cout << "--- Cache State ---" << std::endl;
        std::cout << "Capacity: " << capacity << std::endl;
        std::cout << "Current Size: " << current_size << " bytes (" 
          << (current_size / 1024.0 / 1024.0) << " MB)" << std::endl;
        std::cout << "Cache Contents:" << std::endl;
        
        for (const auto& [key, value] : cache) {
            std::cout << "  " << key << " -> " << value << std::endl;
        }
        
        std::cout << "FIFO Queue Order: ";
        std::queue<std::string> temp_queue = queue;  // Copy to iterate
        while (!temp_queue.empty()) {
            std::cout << temp_queue.front() << " ";
            temp_queue.pop();
        }
        std::cout << std::endl << std::endl;
    }
};

int main() {
    FIFOCache fifo_cache;

    fifo_cache.put("a", std::string(20, 'A')); // 21 bytes
    fifo_cache.put("b", std::string(20, 'B')); // 21 bytes
    fifo_cache.displayCache();

    // should force eviction of "a"
    fifo_cache.put("c", std::string(20, 'C'));
    fifo_cache.displayCache();

    // validate behavior
    auto res = fifo_cache.get("a");
    std::cout << "Get a (should come from DB): " << res.second << std::endl;

    std::cout << "\nAttempting to add value larger than MAX_SIZE (50 bytes)..." << std::endl;
    std::string huge_value(100, 'X');  // 100 bytes + key
    fifo_cache.put("huge", huge_value);
    
    fifo_cache.displayCache();

    std::cout<< "---- Multithreaded tests ----------" << std::endl;
    // Multithreaded example
    std::thread t1([&]() {
        fifo_cache.put("e", "Image5");
        std::cout << "Thread 1 added key e" << std::endl;
    });
    
    std::thread t2([&]() {
        fifo_cache.remove("c");
        std::cout << "Thread 2 removed key c" << std::endl;
    });
    
    std::thread t3([&]() {
        std::cout << "Thread 3 access d: " << fifo_cache.get("d").second << std::endl;
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    return 0;
}