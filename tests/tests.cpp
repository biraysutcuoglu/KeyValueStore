#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <sstream>
#include "../fifo_cache.hpp"

class PerformanceTests {
private:
    int passed = 0;
    int failed = 0;
    
public:
    void assert_true(bool condition, const std::string& test_name) {
        if (condition) {
            std::cout << "[PASS] " << test_name << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] " << test_name << std::endl;
            failed++;
        }
    }
    
    void assert_equal(const std::string& expected, const std::string& actual, 
                     const std::string& test_name) {
        if (expected == actual) {
            std::cout << "[PASS] " << test_name << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] " << test_name << " - Expected: '" << expected 
                      << "', Got: '" << actual << "'" << std::endl;
            failed++;
        }
    }
    
    void print_summary() {
        std::cout << "\n---------- TEST SUMMARY ----------" << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Total:  " << (passed + failed) << std::endl;
        std::cout << "----------------------------------" << std::endl;
    }
};

// Basic functionality tests
void test_basic_put_get(PerformanceTests& runner) {
    std::cout << "\n--- Testing Basic Put/Get ---" << std::endl;
    FIFOCache cache;
    
    cache.put("key1", "value1");
    auto result = cache.get("key1");
    runner.assert_equal("value1", result.second, "Put and get single item");
    
    cache.put("key2", "value2");
    result = cache.get("key2");
    runner.assert_equal("value2", result.second, "Put and get second item");
    
    result = cache.get("key1");
    runner.assert_equal("value1", result.second, "Get first item again");
}

void test_get_nonexistent_key(PerformanceTests& runner) {
    std::cout << "\n--- Testing Non-existent Key ---" << std::endl;
    FIFOCache cache;
    
    auto result = cache.get("nonexistent");
    runner.assert_equal("", result.first, "Non-existent key returns empty key");
    runner.assert_equal("", result.second, "Non-existent key returns empty value");
}

void test_update_existing_key(PerformanceTests& runner) {
    std::cout << "\n--- Testing Update Existing Key ---" << std::endl;
    FIFOCache cache;
    
    cache.put("key1", "value1");
    cache.put("key1", "value2");
    auto result = cache.get("key1");
    runner.assert_equal("value2", result.second, "Updated value retrieved correctly");
}

// FIFO eviction tests
void test_fifo_eviction_basic(PerformanceTests& runner) {
    std::cout << "\n--- Testing FIFO Eviction (Basic) ---" << std::endl;
    FIFOCache cache;
    
    // Fill cache: 21 + 21 = 42 bytes (under 50)
    cache.put("a", std::string(20, 'A')); // 21 bytes
    cache.put("b", std::string(20, 'B')); // 21 bytes
    
    // This should evict "a"
    cache.put("c", std::string(20, 'C')); // 21 bytes
    
    // "a" should be evicted from cache but still in DB
    auto result_a = cache.get("a");
    runner.assert_equal(std::string(20, 'A'), result_a.second, 
                       "Evicted item retrieved from DB");
    
    // "b" and "c" should still be in cache
    auto result_b = cache.get("b");
    runner.assert_equal(std::string(20, 'B'), result_b.second, 
                       "Second item still in cache");
    
    auto result_c = cache.get("c");
    runner.assert_equal(std::string(20, 'C'), result_c.second, 
                       "Third item in cache");
}

void test_value_larger_than_max_size(PerformanceTests& runner) {
    std::cout << "\n--- Testing Value Larger Than MAX_SIZE ---" << std::endl;
    FIFOCache cache;
    
    cache.put("small", "tiny");
    
    // Try to insert value larger than MAX_SIZE (50 bytes)
    std::string huge_value(100, 'X');
    cache.put("huge", huge_value);
    
    // Small value should still be accessible
    auto result_small = cache.get("small");
    runner.assert_equal("tiny", result_small.second, 
                       "Small value still in cache");
    
    // Huge value should be in DB but not cache
    auto result_huge = cache.get("huge");
    runner.assert_equal(huge_value, result_huge.second, 
                       "Huge value retrieved from DB");
}

// Remove tests
void test_remove_from_cache(PerformanceTests& runner) {
    std::cout << "\n--- Testing Remove from Cache ---" << std::endl;
    FIFOCache cache;
    
    cache.put("key1", "value1");
    bool removed= cache.remove("key1");
    runner.assert_true(removed, "Remove returns true for existing key");
    
    auto result = cache.get("key1");
    runner.assert_equal("", result.second, "Removed key not found");
}

void test_remove_nonexistent(PerformanceTests& runner) {
    std::cout << "\n--- Testing Remove Non-existent ---" << std::endl;
    FIFOCache cache;
    
    bool removed = cache.remove("nonexistent");
    runner.assert_true(!removed, "Remove returns false for non-existent key");
}

// Concurrency tests
void test_concurrent_puts(PerformanceTests& runner) {
    std::cout << "\n--- Testing Concurrent Puts ---" << std::endl;
    FIFOCache cache;
    
    const int num_threads = 10;
    const int ops_per_thread = 20;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&cache, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; j++) {
                std::string key = "t" + std::to_string(i) + "_k" + std::to_string(j);
                std::string value = "v" + std::to_string(j);
                cache.put(key, value);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify some random keys
    auto result = cache.get("t0_k0");
    runner.assert_equal("v0", result.second, "Concurrent put thread 0");
    
    result = cache.get("t5_k10");
    runner.assert_equal("v10", result.second, "Concurrent put thread 5");
}

void test_concurrent_gets(PerformanceTests& runner) {
    std::cout << "\n--- Testing Concurrent Gets ---" << std::endl;
    FIFOCache cache;
    
    // Pre-populate cache
    for (int i = 0; i < 5; i++) {
        cache.put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&cache, &success_count]() {
            for (int j = 0; j < 5; j++) {
                auto result = cache.get("key" + std::to_string(j));
                if (result.second == "value" + std::to_string(j)) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    runner.assert_true(success_count == num_threads * 5, 
                      "All concurrent gets successful");
}

void test_concurrent_mixed_operations(PerformanceTests& runner) {
    std::cout << "\n--- Testing Concurrent Mixed Operations ---" << std::endl;
    FIFOCache cache;
    
    // Pre-populate
    for (int i = 0; i < 10; i++) {
        cache.put("init" + std::to_string(i), "val" + std::to_string(i));
    }
    
    std::vector<std::thread> threads;
    
    // Writer threads
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&cache, i]() {
            for (int j = 0; j < 10; j++) {
                cache.put("write" + std::to_string(i) + "_" + std::to_string(j), 
                         "data" + std::to_string(j));
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Reader threads
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&cache]() {
            for (int j = 0; j < 10; j++) {
                cache.get("init" + std::to_string(j));
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Remove threads
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&cache, i]() {
            cache.remove("init" + std::to_string(i));
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    runner.assert_true(true, "Mixed concurrent operations completed without crash");
}

// Edge case tests
void test_empty_key_value(PerformanceTests& runner) {
    std::cout << "\n--- Testing Empty Key/Value ---" << std::endl;
    FIFOCache cache;
    
    // Test 1: Empty key should be ignored (not stored)
    cache.put("", "value");
    auto result = cache.get("");
    runner.assert_equal("", result.first, "Empty key should not be stored - key check");
    
    // Test 2: Empty value can be allowed
    cache.put("key", "");
    result = cache.get("key");
    runner.assert_equal("key", result.first, "Key with empty value - key check");
    runner.assert_equal("", result.second, "Key with empty value - value check");
}

void test_cache_promotion_on_get(PerformanceTests& runner) {
    std::cout << "\n--- Testing Cache Promotion on Get ---" << std::endl;
    FIFOCache cache;
    
    // Fill cache
    cache.put("a", std::string(20, 'A')); // 21 bytes
    cache.put("b", std::string(20, 'B')); // 21 bytes
    
    // Evict "a"
    cache.put("c", std::string(20, 'C')); // 21 bytes
    
    // Get "a" from DB (should be re-added to cache)
    auto result = cache.get("a");
    runner.assert_equal(std::string(20, 'A'), result.second, 
                       "Item retrieved from DB");
    
    // Add another item (this should evict "b" (not "a"))
    cache.put("d", std::string(20, 'D'));
    
    // "a" should still be accessible from cache
    result = cache.get("a");
    runner.assert_equal(std::string(20, 'A'), result.second, 
                       "Recently accessed item still in cache");
}

// Stress tests
void test_rapid_insertions(PerformanceTests& runner) {
    std::cout << "\n--- Testing Rapid Insertions ---" << std::endl;
    FIFOCache cache;
    
    const int num_insertions = 1000;
    for (int i = 0; i < num_insertions; i++) {
        cache.put("rapid" + std::to_string(i), "val" + std::to_string(i));
    }
    
    // Verify some random entries
    auto result = cache.get("rapid500");
    runner.assert_equal("val500", result.second, "Rapid insertion test");
}

int main() {
    PerformanceTests runner;
    
    // Basic functionality
    test_basic_put_get(runner);
    test_get_nonexistent_key(runner);
    test_update_existing_key(runner);
    
    // FIFO eviction
    test_fifo_eviction_basic(runner);
    test_value_larger_than_max_size(runner);
    
    // Remove operations
    test_remove_from_cache(runner);
    test_remove_nonexistent(runner);
    
    // Concurrency
    test_concurrent_puts(runner);
    test_concurrent_gets(runner);
    test_concurrent_mixed_operations(runner);
    
    // Edge cases
    test_empty_key_value(runner);
    test_cache_promotion_on_get(runner);
    
    // Stress tests
    test_rapid_insertions(runner);
    
    runner.print_summary();
    
    return 0;
}