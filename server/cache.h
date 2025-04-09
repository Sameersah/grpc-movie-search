#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <list>
#include <memory>
#include <atomic>
#include "movie.grpc.pb.h"

/**
 * Thread-safe LRU cache for movie search results.
 * Uses TTL (time-to-live) for entries and LRU eviction when full.
 */
class Cache {
public:
    /**
     * Constructor
     * @param ttl_seconds Time-to-live for cache entries in seconds
     * @param max_size Maximum number of entries in the cache
     */
    explicit Cache(int ttl_seconds = 300, size_t max_size = 100)
        : ttl_(std::chrono::seconds(ttl_seconds)), 
          max_size_(max_size), 
          hit_count_(0), 
          miss_count_(0) {}

    /**
     * Try to get result from cache
     * @param query The search query
     * @param response The response to populate (if cache hit)
     * @return Whether the item was found in cache (cache hit)
     */
    bool get(const std::string& query, movie::SearchResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Clean expired entries first
        clean_expired();
        
        auto it = cache_.find(query);
        if (it == cache_.end()) {
            // Cache miss
            miss_count_++;
            return false;
        }
        
        // Check if entry is expired
        if (std::chrono::system_clock::now() - it->second.timestamp > ttl_) {
            // Remove expired entry
            cache_.erase(it);
            lru_list_.remove(query);
            miss_count_++;
            return false;
        }
        
        // Update LRU position (move to front)
        lru_list_.remove(query);
        lru_list_.push_front(query);
        
        // Return cached results
        response = *(it->second.response);
        hit_count_++;
        return true;
    }

    /**
     * Store result in cache
     * @param query The search query
     * @param response The response to cache
     */
    void put(const std::string& query, const movie::SearchResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Clean expired entries first
        clean_expired();
        
        // Check if we need to evict entries due to size limits
        if (cache_.size() >= max_size_ && !lru_list_.empty()) {
            // Remove least recently used entry
            const std::string& oldest = lru_list_.back();
            cache_.erase(oldest);
            lru_list_.pop_back();
        }
        
        // Add new entry
        CacheEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.response = std::make_shared<movie::SearchResponse>(response);
        
        cache_[query] = std::move(entry);
        
        // Update LRU tracking
        lru_list_.remove(query); // In case it was already there
        lru_list_.push_front(query);
    }
    
    /**
     * Clear all entries from the cache
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        lru_list_.clear();
    }
    
    /**
     * Get current cache size
     * @return The number of entries in the cache
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
    /**
     * Get cache hit ratio
     * @return Hit ratio (0.0 to 1.0)
     */
    double hit_ratio() const {
        uint64_t hits = hit_count_;
        uint64_t misses = miss_count_;
        uint64_t total = hits + misses;
        
        if (total == 0) {
            return 0.0;
        }
        
        return static_cast<double>(hits) / static_cast<double>(total);
    }
    
    /**
     * Get cache hit count
     * @return Number of cache hits
     */
    uint64_t hit_count() const {
        return hit_count_;
    }
    
    /**
     * Get cache miss count
     * @return Number of cache misses
     */
    uint64_t miss_count() const {
        return miss_count_;
    }

private:
    /**
     * Structure to hold a cache entry
     */
    struct CacheEntry {
        std::chrono::system_clock::time_point timestamp;
        std::shared_ptr<movie::SearchResponse> response;
    };
    
    /**
     * Remove all expired entries from the cache
     */
    void clean_expired() {
        auto now = std::chrono::system_clock::now();
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (now - it->second.timestamp > ttl_) {
                lru_list_.remove(it->first);
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Cache storage
    std::unordered_map<std::string, CacheEntry> cache_;
    
    // LRU tracking
    std::list<std::string> lru_list_;
    
    // TTL for cache entries
    std::chrono::seconds ttl_;
    
    // Maximum number of entries in cache
    size_t max_size_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Statistics
    std::atomic<uint64_t> hit_count_;
    std::atomic<uint64_t> miss_count_;
};

#endif // CACHE_H