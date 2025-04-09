#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include "movie.grpc.pb.h"
#include "server/cache.h"
#include "server/posix_shared_memory.h"
#include "server/response_serializer.h"

using movie::MovieInfo;
using movie::SearchResponse;

// Helper function to create a test movie response
SearchResponse createTestResponse(const std::string& query, int count) {
    SearchResponse response;
    
    for (int i = 0; i < count; i++) {
        MovieInfo* movie = response.add_results();
        movie->set_title(query + " Movie " + std::to_string(i + 1));
        movie->set_director("Director " + std::to_string(i + 1));
        movie->set_genre("Test Genre");
        movie->set_year(2000 + i);
    }
    
    return response;
}

// Helper function to print a response
void printResponse(const SearchResponse& response) {
    std::cout << "Response contains " << response.results_size() << " movies:" << std::endl;
    
    for (int i = 0; i < response.results_size(); i++) {
        const MovieInfo& movie = response.results(i);
        std::cout << "  " << (i + 1) << ". " << movie.title() 
                 << " (" << movie.year() << ") - " 
                 << movie.director() << " [" << movie.genre() << "]" << std::endl;
    }
}

// Test the in-memory cache
bool testCache() {
    std::cout << "\n===== Testing In-Memory Cache =====\n" << std::endl;
    
    try {
        // Create cache with short TTL for testing
        Cache cache(2, 5);
        
        // Create test responses
        SearchResponse response1 = createTestResponse("Inception", 3);
        SearchResponse response2 = createTestResponse("Matrix", 2);
        SearchResponse response3 = createTestResponse("Avengers", 4);
        
        // Test basic put and get
        cache.put("inception", response1);
        cache.put("matrix", response2);
        
        SearchResponse result;
        bool found = cache.get("inception", result);
        
        if (!found) {
            std::cerr << "❌ Failed to retrieve 'inception' from cache" << std::endl;
            return false;
        }
        
        std::cout << "✅ Retrieved 'inception' from cache:" << std::endl;
        printResponse(result);
        
        // Test LRU eviction
        std::cout << "\nTesting LRU eviction..." << std::endl;
        
        // Fill cache to capacity
        cache.put("avengers", response3);
        cache.put("batman", createTestResponse("Batman", 2));
        cache.put("superman", createTestResponse("Superman", 1));
        
        // This should evict the oldest entry (inception)
        cache.put("wonder woman", createTestResponse("Wonder Woman", 3));
        
        // Check if inception was evicted
        found = cache.get("inception", result);
        
        if (found) {
            std::cerr << "❌ 'inception' should have been evicted but was found in cache" << std::endl;
            return false;
        }
        
        std::cout << "✅ LRU eviction working correctly ('inception' was evicted)" << std::endl;
        
        // Test TTL expiration
        std::cout << "\nTesting TTL expiration (waiting 3 seconds)..." << std::endl;
        
        // Add a new entry
        cache.put("star wars", createTestResponse("Star Wars", 2));
        
        // Wait for TTL to expire
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Try to get expired entry
        found = cache.get("star wars", result);
        
        if (found) {
            std::cerr << "❌ 'star wars' should have expired but was found in cache" << std::endl;
            return false;
        }
        
        std::cout << "✅ TTL expiration working correctly ('star wars' was expired)" << std::endl;
        
        // Test cache statistics
        std::cout << "\nCache statistics:" << std::endl;
        std::cout << "Size: " << cache.size() << " entries" << std::endl;
        std::cout << "Hits: " << cache.hit_count() << std::endl;
        std::cout << "Misses: " << cache.miss_count() << std::endl;
        std::cout << "Hit ratio: " << std::fixed << std::setprecision(2) 
                 << (cache.hit_ratio() * 100.0) << "%" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Cache test failed with exception: " << e.what() << std::endl;
        return false;
    }
}

// Test the shared memory implementation
bool testSharedMemory() {
    std::cout << "\n===== Testing Shared Memory =====\n" << std::endl;
    
    try {
        // Remove any existing shared memory from previous tests
        PosixSharedMemory::destroy("/test_movie_cache");
        
        // Create shared memory
        PosixSharedMemory shm("/test_movie_cache", 1024 * 1024); // 1 MB
        
        // Create test response
        SearchResponse response = createTestResponse("Shared Memory Test", 5);
        
        // Serialize and store in shared memory
        std::vector<uint8_t> serialized = ResponseSerializer::serialize(response);
        bool stored = shm.write("shared_test", serialized);
        
        if (!stored) {
            std::cerr << "❌ Failed to store data in shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Stored data in shared memory (" << serialized.size() << " bytes)" << std::endl;
        
        // Read back from shared memory
        std::vector<uint8_t> retrieved;
        bool found = shm.read("shared_test", retrieved);
        
        if (!found) {
            std::cerr << "❌ Failed to retrieve data from shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Retrieved data from shared memory (" << retrieved.size() << " bytes)" << std::endl;
        
        // Deserialize and check
        SearchResponse result;
        bool deserialized = ResponseSerializer::deserialize(retrieved, result);
        
        if (!deserialized) {
            std::cerr << "❌ Failed to deserialize data from shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Deserialized response from shared memory:" << std::endl;
        printResponse(result);
        
        // Test entry removal
        bool removed = shm.remove("shared_test");
        
        if (!removed) {
            std::cerr << "❌ Failed to remove entry from shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Successfully removed entry from shared memory" << std::endl;
        
        // Verify entry was removed
        found = shm.read("shared_test", retrieved);
        
        if (found) {
            std::cerr << "❌ Entry should have been removed but was found in shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Entry verification successful (entry was removed)" << std::endl;
        
        // Test shared memory statistics
        std::cout << "\nShared memory statistics:" << std::endl;
        std::cout << "Entries: " << shm.count() << std::endl;
        std::cout << "Used bytes: " << shm.usedBytes() << " bytes" << std::endl;
        
        // Clean up
        PosixSharedMemory::destroy("/test_movie_cache");
        std::cout << "✅ Destroyed shared memory" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Shared memory test failed with exception: " << e.what() << std::endl;
        return false;
    }
}

// Test combined cache and shared memory
bool testMultiProcess() {
    std::cout << "\n===== Testing Multi-Process Communication =====\n" << std::endl;
    
    // This test simulates two processes using the same shared memory segment
    
    try {
        // Remove any existing shared memory from previous tests
        PosixSharedMemory::destroy("/test_mp_cache");
        
        // Create first "process" shared memory
        PosixSharedMemory shm1("/test_mp_cache", 1024 * 1024); // 1 MB
        
        // Write some data from first "process"
        SearchResponse response1 = createTestResponse("Process 1 Data", 3);
        std::vector<uint8_t> serialized1 = ResponseSerializer::serialize(response1);
        bool stored1 = shm1.write("test_key", serialized1);
        
        if (!stored1) {
            std::cerr << "❌ Process 1: Failed to store data in shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Process 1: Stored data in shared memory" << std::endl;
        
        // Create second "process" shared memory (opens existing)
        PosixSharedMemory shm2("/test_mp_cache", 1024 * 1024, false);
        
        // Read data from second "process"
        std::vector<uint8_t> retrieved;
        bool found = shm2.read("test_key", retrieved);
        
        if (!found) {
            std::cerr << "❌ Process 2: Failed to retrieve data from shared memory" << std::endl;
            return false;
        }
        
        // Deserialize and check
        SearchResponse result;
        bool deserialized = ResponseSerializer::deserialize(retrieved, result);
        
        if (!deserialized) {
            std::cerr << "❌ Process 2: Failed to deserialize data from shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Process 2: Successfully read data written by Process 1:" << std::endl;
        printResponse(result);
        
        // Modify data from second "process"
        SearchResponse response2 = createTestResponse("Process 2 Data", 2);
        std::vector<uint8_t> serialized2 = ResponseSerializer::serialize(response2);
        bool stored2 = shm2.write("test_key2", serialized2);
        
        if (!stored2) {
            std::cerr << "❌ Process 2: Failed to store new data in shared memory" << std::endl;
            return false;
        }
        
        std::cout << "✅ Process 2: Stored new data in shared memory" << std::endl;
        
        // Read modified data from first "process"
        std::vector<uint8_t> retrieved2;
        bool found2 = shm1.read("test_key2", retrieved2);
        
        if (!found2) {
            std::cerr << "❌ Process 1: Failed to retrieve data written by Process 2" << std::endl;
            return false;
        }
        
        // Deserialize and check
        SearchResponse result2;
        bool deserialized2 = ResponseSerializer::deserialize(retrieved2, result2);
        
        if (!deserialized2) {
            std::cerr << "❌ Process 1: Failed to deserialize data from Process 2" << std::endl;
            return false;
        }
        
        std::cout << "✅ Process 1: Successfully read data written by Process 2:" << std::endl;
        printResponse(result2);
        
        // Clean up
        PosixSharedMemory::destroy("/test_mp_cache");
        std::cout << "✅ Destroyed shared memory" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Multi-process test failed with exception: " << e.what() << std::endl;
        return false;
    }
}

// Main test function
int main() {
    std::cout << "Starting cache and shared memory tests..." << std::endl;
    
    bool cacheSuccess = testCache();
    bool shmSuccess = testSharedMemory();
    bool mpSuccess = testMultiProcess();
    
    std::cout << "\n===== Test Results =====\n" << std::endl;
    std::cout << "In-Memory Cache Test: " << (cacheSuccess ? "✅ Passed" : "❌ Failed") << std::endl;
    std::cout << "Shared Memory Test: " << (shmSuccess ? "✅ Passed" : "❌ Failed") << std::endl;
    std::cout << "Multi-Process Test: " << (mpSuccess ? "✅ Passed" : "❌ Failed") << std::endl;
    
    if (cacheSuccess && shmSuccess && mpSuccess) {
        std::cout << "\n🎉 All tests passed successfully! 🎉" << std::endl;
        return 0;
    } else {
        std::cerr << "\n❌ Some tests failed" << std::endl;
        return 1;
    }
}