## Efficient Key Value Store
An implementation of thread-safe efficient key value store. 
- Utilizes a cache structure with FIFO queue.
- When the cache is full, evicts the oldest key in the queue. 
- As persistent storage, it uses SQLite DB. 

### How to run:
Unit tests and performance tests are available under */tests* folder. To build and run these tests, the steps are given as below:
1. cmake -S . -B build   
2. cmake --build build    
3. To run unit tests:
    - ./build/unit_tests
4. To run performance tests:
    - ./build/performance_tests


