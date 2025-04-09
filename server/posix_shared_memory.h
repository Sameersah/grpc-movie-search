#ifndef POSIX_SHARED_MEMORY_H
#define POSIX_SHARED_MEMORY_H

// Add this at the top of posix_shared_memory.h
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __APPLE__
// Define missing constants for macOS
#ifndef O_DIRECT
#define O_DIRECT 0
#endif
#endif

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>      // For O_* constants
#include <sys/mman.h>   // For mmap, munmap
#include <sys/stat.h>   // For mode constants
#include <unistd.h>     // For ftruncate, close
#include <semaphore.h>  // For POSIX semaphores
#include <cstdint>      // For uint8_t
#include <ctime>        // For time_t

/**
 * A POSIX-based shared memory implementation for inter-process communication.
 * Uses shm_open and mmap to create and map shared memory.
 * Uses POSIX named semaphores for synchronization.
 */
class PosixSharedMemory {
public:
    /**
     * Constructor - creates or opens shared memory segment
     * @param name Name of the shared memory segment (must start with /)
     * @param size Size of the shared memory segment in bytes
     * @param create Whether to create the segment if it doesn't exist
     */
    PosixSharedMemory(const std::string& name, size_t size, bool create = true)
        : name_(name), size_(size), data_(nullptr), sem_(nullptr) {
        // Validate name
        if (name.empty() || name[0] != '/') {
            throw std::invalid_argument("Shared memory name must start with '/'");
        }
        
        // Create semaphore name from shared memory name
        std::string semName = name + "_sem";
        
        if (create) {
            // Create or open shared memory
            fd_ = shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
            if (fd_ == -1) {
                throw std::runtime_error("Failed to create shared memory: " + std::string(strerror(errno)));
            }
            
            // Set size
            if (ftruncate(fd_, size) == -1) {
                close(fd_);
                throw std::runtime_error("Failed to set shared memory size: " + std::string(strerror(errno)));
            }
            
            // Create or open semaphore
            sem_ = sem_open(semName.c_str(), O_CREAT, S_IRUSR | S_IWUSR, 1);
            if (sem_ == SEM_FAILED) {
                close(fd_);
                throw std::runtime_error("Failed to create semaphore: " + std::string(strerror(errno)));
            }
        } else {
            // Open existing shared memory
            fd_ = shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
            if (fd_ == -1) {
                throw std::runtime_error("Failed to open shared memory: " + std::string(strerror(errno)));
            }
            
            // Get the size if not specified
            if (size_ == 0) {
                struct stat sb;
                if (fstat(fd_, &sb) == -1) {
                    close(fd_);
                    throw std::runtime_error("Failed to get shared memory size: " + std::string(strerror(errno)));
                }
                size_ = sb.st_size;
            }
            
            // Open existing semaphore
            sem_ = sem_open(semName.c_str(), 0);
            if (sem_ == SEM_FAILED) {
                close(fd_);
                throw std::runtime_error("Failed to open semaphore: " + std::string(strerror(errno)));
            }
        }
        
        // Map shared memory into address space
        data_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (data_ == MAP_FAILED) {
            close(fd_);
            sem_close(sem_);
            throw std::runtime_error("Failed to map shared memory: " + std::string(strerror(errno)));
        }
        
        // Initialize header if we're the creator
        if (create) {
            // Check if memory is already initialized
            Header* header = getHeader();
            if (header->magic != MAGIC_NUMBER) {
                // Initialize header
                header->magic = MAGIC_NUMBER;
                header->entryCount = 0;
                header->usedBytes = 0;
            }
        }
    }
    
    /**
     * Destructor - unmaps shared memory and closes semaphore
     */
    ~PosixSharedMemory() {
        if (data_ != MAP_FAILED && data_ != nullptr) {
            munmap(data_, size_);
        }
        
        if (fd_ != -1) {
            close(fd_);
        }
        
        if (sem_ != SEM_FAILED && sem_ != nullptr) {
            sem_close(sem_);
        }
    }
    
    /**
     * Store data in shared memory
     * @param key The key to store the data under
     * @param value The data to store
     * @return Whether the operation was successful
     */
    bool write(const std::string& key, const std::vector<uint8_t>& value) {
        if (key.empty() || value.empty()) {
            return false;
        }
        
        // Lock the shared memory
        lock();
        
        try {
            // Get the header
            Header* header = getHeader();
            
            // Find existing entry or space for a new one
            int entryIndex = findEntry(key);
            
            // Calculate required space for the entry
            size_t entrySize = sizeof(EntryHeader) + key.size() + value.size();
            
            // If we found an existing entry
            if (entryIndex != -1) {
                EntryHeader* entry = getEntryHeader(header->entries[entryIndex]);
                
                // If new data is same size or smaller, we can update in place
                if (entrySize <= sizeof(EntryHeader) + entry->keySize + entry->valueSize) {
                    // Update entry
                    entry->timestamp = time(nullptr);
                    entry->valueSize = value.size();
                    
                    // Copy the value
                    uint8_t* valuePtr = getValuePointer(entry, key.size());
                    memcpy(valuePtr, value.data(), value.size());
                    
                    unlock();
                    return true;
                } else {
                    // New data is larger, remove old entry
                    removeEntry(entryIndex);
                    entryIndex = -1; // Mark as new entry needed
                }
            }
            
            // If we need to create a new entry
            if (entryIndex == -1) {
                // Check if we have enough space
                if (header->usedBytes + entrySize > size_ - sizeof(Header)) {
                    // Try to compact memory first
                    compactMemory();
                    
                    // Check again after compaction
                    if (header->usedBytes + entrySize > size_ - sizeof(Header)) {
                        // Not enough space even after compaction
                        unlock();
                        return false;
                    }
                }
                
                // Create new entry
                size_t offset = header->usedBytes;
                EntryHeader* entry = getEntryHeader(offset);
                
                // Initialize entry
                entry->keySize = key.size();
                entry->valueSize = value.size();
                entry->timestamp = time(nullptr);
                
                // Copy the key and value
                uint8_t* keyPtr = reinterpret_cast<uint8_t*>(entry + 1);
                memcpy(keyPtr, key.data(), key.size());
                
                uint8_t* valuePtr = keyPtr + key.size();
                memcpy(valuePtr, value.data(), value.size());
                
                // Add entry to header
                header->entries[header->entryCount] = offset;
                header->entryCount++;
                header->usedBytes += entrySize;
                
                unlock();
                return true;
            }
            
            unlock();
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Error writing to shared memory: " << e.what() << std::endl;
            unlock();
            return false;
        }
    }
    
    /**
     * Read data from shared memory
     * @param key The key to read
     * @param value The retrieved data (if successful)
     * @return Whether the read was successful
     */
    bool read(const std::string& key, std::vector<uint8_t>& value) {
        if (key.empty()) {
            return false;
        }
        
        // Lock the shared memory
        lock();
        
        try {
            // Get the header
            Header* header = getHeader();
            
            // Find entry
            int entryIndex = findEntry(key);
            if (entryIndex == -1) {
                unlock();
                return false;
            }
            
            // Get entry
            EntryHeader* entry = getEntryHeader(header->entries[entryIndex]);
            
            // Get value
            uint8_t* valuePtr = getValuePointer(entry, entry->keySize);
            
            // Copy value
            value.resize(entry->valueSize);
            memcpy(value.data(), valuePtr, entry->valueSize);
            
            // Update timestamp
            entry->timestamp = time(nullptr);
            
            unlock();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error reading from shared memory: " << e.what() << std::endl;
            unlock();
            return false;
        }
    }
    
    /**
     * Remove an entry from shared memory
     * @param key The key to remove
     * @return Whether the operation was successful
     */
    bool remove(const std::string& key) {
        if (key.empty()) {
            return false;
        }
        
        // Lock the shared memory
        lock();
        
        try {
            // Find entry
            int entryIndex = findEntry(key);
            if (entryIndex == -1) {
                unlock();
                return false;
            }
            
            // Remove entry
            removeEntry(entryIndex);
            
            unlock();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error removing from shared memory: " << e.what() << std::endl;
            unlock();
            return false;
        }
    }
    
    /**
     * Get the number of entries in shared memory
     * @return The number of entries
     */
    size_t count() {
        lock();
        size_t count = getHeader()->entryCount;
        unlock();
        return count;
    }
    
    /**
     * Get used bytes in shared memory
     * @return The number of used bytes
     */
    size_t usedBytes() {
        lock();
        size_t bytes = getHeader()->usedBytes;
        unlock();
        return bytes;
    }
    
    /**
     * Clear all entries from shared memory
     */
    void clear() {
        lock();
        Header* header = getHeader();
        header->entryCount = 0;
        header->usedBytes = 0;
        unlock();
    }
    
    /**
     * Static method to destroy the shared memory segment
     * @param name The name of the segment to destroy
     */
    static void destroy(const std::string& name) {
        // Remove shared memory
        shm_unlink(name.c_str());
        
        // Remove semaphore
        std::string semName = name + "_sem";
        sem_unlink(semName.c_str());
    }

private:
    static const uint32_t MAGIC_NUMBER = 0x53484D30; // "SHM0"
    static const size_t MAX_ENTRIES = 1000;
    
    // Header at start of shared memory
    struct Header {
        uint32_t magic;            // Magic number to identify initialized memory
        size_t entryCount;         // Number of entries
        size_t usedBytes;          // Used bytes (excluding header)
        size_t entries[MAX_ENTRIES]; // Offsets to entries
    };
    
    // Entry header
    struct EntryHeader {
        size_t keySize;     // Size of key
        size_t valueSize;   // Size of value
        time_t timestamp;   // Last access time
    };
    
    // Lock shared memory
    void lock() {
        if (sem_wait(sem_) == -1) {
            throw std::runtime_error("Failed to lock semaphore: " + std::string(strerror(errno)));
        }
    }
    
    // Unlock shared memory
    void unlock() {
        if (sem_post(sem_) == -1) {
            throw std::runtime_error("Failed to unlock semaphore: " + std::string(strerror(errno)));
        }
    }
    
    // Get header pointer
    Header* getHeader() {
        return reinterpret_cast<Header*>(data_);
    }
    
    // Get entry header at offset
    EntryHeader* getEntryHeader(size_t offset) {
        return reinterpret_cast<EntryHeader*>(
            reinterpret_cast<uint8_t*>(data_) + sizeof(Header) + offset);
    }
    
    // Get pointer to key for an entry
    uint8_t* getKeyPointer(EntryHeader* entry) {
        return reinterpret_cast<uint8_t*>(entry + 1);
    }
    
    // Get pointer to value for an entry
    uint8_t* getValuePointer(EntryHeader* entry, size_t keySize) {
        return getKeyPointer(entry) + keySize;
    }
    
    // Find entry by key, returns index or -1 if not found
    int findEntry(const std::string& key) {
        Header* header = getHeader();
        
        for (size_t i = 0; i < header->entryCount; i++) {
            EntryHeader* entry = getEntryHeader(header->entries[i]);
            
            // Check key size first
            if (entry->keySize != key.size()) {
                continue;
            }
            
            // Check key content
            uint8_t* keyPtr = getKeyPointer(entry);
            if (memcmp(keyPtr, key.data(), key.size()) == 0) {
                return static_cast<int>(i);
            }
        }
        
        return -1; // Not found
    }
    
    // Remove entry at index
    void removeEntry(int entryIndex) {
        Header* header = getHeader();
        
        // Shift entries down
        for (size_t i = entryIndex; i < header->entryCount - 1; i++) {
            header->entries[i] = header->entries[i + 1];
        }
        
        header->entryCount--;
    }
    
    // Compact memory to reclaim space from deleted entries
    void compactMemory() {
        Header* header = getHeader();
        
        // If no entries, just reset
        if (header->entryCount == 0) {
            header->usedBytes = 0;
            return;
        }
        
        // Create temporary buffer for valid entries
        std::vector<uint8_t> tempBuffer(size_ - sizeof(Header));
        std::vector<size_t> newOffsets(header->entryCount);
        
        size_t newUsedBytes = 0;
        size_t newEntryCount = 0;
        
        // Copy valid entries to temporary buffer
        for (size_t i = 0; i < header->entryCount; i++) {
            EntryHeader* entry = getEntryHeader(header->entries[i]);
            size_t entrySize = sizeof(EntryHeader) + entry->keySize + entry->valueSize;
            
            // Copy entry to temporary buffer
            memcpy(tempBuffer.data() + newUsedBytes, entry, entrySize);
            
            // Store new offset
            newOffsets[newEntryCount] = newUsedBytes;
            newEntryCount++;
            
            // Update used bytes
            newUsedBytes += entrySize;
        }
        
        // Copy temporary buffer back to shared memory
        memcpy(
            reinterpret_cast<uint8_t*>(data_) + sizeof(Header),
            tempBuffer.data(), 
            newUsedBytes
        );
        
        // Update header
        header->usedBytes = newUsedBytes;
        header->entryCount = newEntryCount;
        
        // Update entry offsets
        for (size_t i = 0; i < newEntryCount; i++) {
            header->entries[i] = newOffsets[i];
        }
    }

    std::string name_;  // Name of shared memory segment
    size_t size_;       // Size of shared memory
    int fd_;            // File descriptor
    void* data_;        // Mapped memory
    sem_t* sem_;        // Semaphore for synchronization
};

#endif // POSIX_SHARED_MEMORY_H