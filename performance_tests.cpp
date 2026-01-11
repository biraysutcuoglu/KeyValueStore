#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <random>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include "fifo_cache.hpp"

class PerformanceTest {
private:
    FIFOCache cache;
    
    // Generate random string of given length
    std::string generateRandomString(size_t length) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }
    
    // Generate test data
    std::vector<std::pair<std::string, std::string>> generateTestData(size_t count, size_t key_size, size_t value_size) {
        std::vector<std::pair<std::string, std::string>> data;
        data.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            std::string key = "key_" + std::to_string(i) + "_" + generateRandomString(key_size);
            std::string value = "value_" + generateRandomString(value_size);
            data.emplace_back(key, value);
        }
        
        return data;
    }
    
    void printStats(const std::string& test_name, double duration_ms, size_t operations, const std::vector<double>& latencies = {}) {
        std::cout << "\n=== " << test_name << " ===" << std::endl;
        std::cout << "Total Duration: " << std::fixed << std::setprecision(2) << duration_ms << " ms" << std::endl;
        std::cout << "Operations: " << operations << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << (operations / (duration_ms / 1000.0)) << " ops/sec" << std::endl;
        
        if (!latencies.empty()) {
            auto sorted_latencies = latencies;
            std::sort(sorted_latencies.begin(), sorted_latencies.end());
            
            double avg = std::accumulate(sorted_latencies.begin(), sorted_latencies.end(), 0.0) / sorted_latencies.size();
            double p50 = sorted_latencies[sorted_latencies.size() / 2];
            double p95 = sorted_latencies[sorted_latencies.size() * 95 / 100];
            double p99 = sorted_latencies[sorted_latencies.size() * 99 / 100];
            
            std::cout << "Latency Stats (ms):" << std::endl;
            std::cout << "  Average: " << std::fixed << std::setprecision(4) << avg << std::endl;
            std::cout << "  P50: " << p50 << std::endl;
            std::cout << "  P95: " << p95 << std::endl;
            std::cout << "  P99: " << p99 << std::endl;
            std::cout << "  Min: " << sorted_latencies.front() << std::endl;
            std::cout << "  Max: " << sorted_latencies.back() << std::endl;
        }
    }

public:
    // Test 1: Sequential Writes
    void testSequentialWrites(size_t num_operations) {
        auto data = generateTestData(num_operations, 5, 10);
        std::vector<double> latencies;
        latencies.reserve(num_operations);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& [key, value] : data) {
            auto op_start = std::chrono::high_resolution_clock::now();
            cache.put(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            
            latencies.push_back(std::chrono::duration<double, std::milli>(op_end - op_start).count());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Sequential Writes", duration, num_operations, latencies);
    }
    
    // Test 2: Sequential Reads (Cache Hits)
    void testSequentialReads(size_t num_operations) {
        // Pre-populate cache
        auto data = generateTestData(num_operations, 5, 10);
        for (const auto& [key, value] : data) {
            cache.put(key, value);
        }
        
        std::vector<double> latencies;
        latencies.reserve(num_operations);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& [key, value] : data) {
            auto op_start = std::chrono::high_resolution_clock::now();
            cache.get(key);
            auto op_end = std::chrono::high_resolution_clock::now();
            
            latencies.push_back(std::chrono::duration<double, std::milli>(op_end - op_start).count());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Sequential Reads (Cache Hits)", duration, num_operations, latencies);
    }
    
    // Test 3: Mixed Read/Write Operations
    void testMixedOperations(size_t num_operations, double read_ratio = 0.7) {
        auto data = generateTestData(num_operations, 5, 10);
        std::vector<double> latencies;
        latencies.reserve(num_operations);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_operations; ++i) {
            auto op_start = std::chrono::high_resolution_clock::now();
            
            if (dis(gen) < read_ratio && i > 0) {
                // Read operation
                size_t read_idx = std::uniform_int_distribution<size_t>(0, i - 1)(gen);
                cache.get(data[read_idx].first);
            } else {
                // Write operation
                cache.put(data[i].first, data[i].second);
            }
            
            auto op_end = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::milli>(op_end - op_start).count());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Mixed Operations (70% reads, 30% writes)", duration, num_operations, latencies);
    }
    
    // Test 4: Cache Eviction Performance
    void testCacheEviction(size_t num_operations) {
        // Generate data that will exceed cache size
        auto data = generateTestData(num_operations, 5, 15); // Larger values to trigger evictions
        std::vector<double> latencies;
        latencies.reserve(num_operations);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& [key, value] : data) {
            auto op_start = std::chrono::high_resolution_clock::now();
            cache.put(key, value);
            auto op_end = std::chrono::high_resolution_clock::now();
            
            latencies.push_back(std::chrono::duration<double, std::milli>(op_end - op_start).count());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Cache Eviction Test", duration, num_operations, latencies);
    }
    
    // Test 5: Multi-threaded Concurrent Writes
    void testConcurrentWrites(size_t num_threads, size_t ops_per_thread) {
        std::vector<std::thread> threads;
        std::vector<double> all_latencies;
        std::mutex latency_mutex;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, t, ops_per_thread, &all_latencies, &latency_mutex]() {
                auto data = generateTestData(ops_per_thread, 5, 10);
                std::vector<double> thread_latencies;
                thread_latencies.reserve(ops_per_thread);
                
                for (const auto& [key, value] : data) {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    cache.put(key + "_t" + std::to_string(t), value);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    
                    thread_latencies.push_back(
                        std::chrono::duration<double, std::milli>(op_end - op_start).count()
                    );
                }
                
                std::lock_guard<std::mutex> lock(latency_mutex);
                all_latencies.insert(all_latencies.end(), thread_latencies.begin(), thread_latencies.end());
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Concurrent Writes (" + std::to_string(num_threads) + " threads)", 
                   duration, num_threads * ops_per_thread, all_latencies);
    }
    
    // Test 6: Multi-threaded Concurrent Reads
    void testConcurrentReads(size_t num_threads, size_t ops_per_thread) {
        // Pre-populate cache
        auto data = generateTestData(ops_per_thread, 5, 10);
        for (const auto& [key, value] : data) {
            cache.put(key, value);
        }
        
        std::vector<std::thread> threads;
        std::vector<double> all_latencies;
        std::mutex latency_mutex;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, &data, ops_per_thread, &all_latencies, &latency_mutex]() {
                std::vector<double> thread_latencies;
                thread_latencies.reserve(ops_per_thread);
                
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<size_t> dis(0, data.size() - 1);
                
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    cache.get(data[dis(gen)].first);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    
                    thread_latencies.push_back(
                        std::chrono::duration<double, std::milli>(op_end - op_start).count()
                    );
                }
                
                std::lock_guard<std::mutex> lock(latency_mutex);
                all_latencies.insert(all_latencies.end(), thread_latencies.begin(), thread_latencies.end());
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Concurrent Reads (" + std::to_string(num_threads) + " threads)", 
                   duration, num_threads * ops_per_thread, all_latencies);
    }
    
    // Test 7: Multi-threaded Mixed Operations
    void testConcurrentMixed(size_t num_threads, size_t ops_per_thread, double read_ratio = 0.7) {
        // Pre-populate some data
        auto initial_data = generateTestData(ops_per_thread / 2, 5, 10);
        for (const auto& [key, value] : initial_data) {
            cache.put(key, value);
        }
        
        std::vector<std::thread> threads;
        std::vector<double> all_latencies;
        std::mutex latency_mutex;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, t, ops_per_thread, read_ratio, &initial_data, &all_latencies, &latency_mutex]() {
                auto data = generateTestData(ops_per_thread, 5, 10);
                std::vector<double> thread_latencies;
                thread_latencies.reserve(ops_per_thread);
                
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> op_dis(0.0, 1.0);
                std::uniform_int_distribution<size_t> key_dis(0, initial_data.size() - 1);
                
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    
                    if (op_dis(gen) < read_ratio) {
                        // Read operation
                        cache.get(initial_data[key_dis(gen)].first);
                    } else {
                        // Write operation
                        cache.put(data[i].first + "_t" + std::to_string(t), data[i].second);
                    }
                    
                    auto op_end = std::chrono::high_resolution_clock::now();
                    thread_latencies.push_back(
                        std::chrono::duration<double, std::milli>(op_end - op_start).count()
                    );
                }
                
                std::lock_guard<std::mutex> lock(latency_mutex);
                all_latencies.insert(all_latencies.end(), thread_latencies.begin(), thread_latencies.end());
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        printStats("Concurrent Mixed Operations (" + std::to_string(num_threads) + " threads, 70% reads)", 
                   duration, num_threads * ops_per_thread, all_latencies);
    }
    
    void runAllTests() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "FIFO CACHE PERFORMANCE TESTS" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\n--- SINGLE-THREADED TESTS ---" << std::endl;
        testSequentialWrites(1000);
        testSequentialReads(1000);
        testMixedOperations(1000);
        testCacheEviction(500);
        
        std::cout << "\n--- MULTI-THREADED TESTS ---" << std::endl;
        testConcurrentWrites(4, 250);
        testConcurrentReads(4, 250);
        testConcurrentMixed(4, 250);
        
        std::cout << "\n--- SCALING TESTS ---" << std::endl;
        testConcurrentWrites(8, 125);
        testConcurrentReads(8, 125);
        testConcurrentMixed(8, 125);
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "ALL TESTS COMPLETED" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
    }
};

int main() {
    PerformanceTest test;
    test.runAllTests();
    return 0;
}