// ab_communication.h
#ifndef AB_COMMUNICATION_H
#define AB_COMMUNICATION_H

#include <string>
#include <memory>
#include <atomic>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"
#include "posix_shared_memory.h"

// Structure for request-response via shared memory
struct SharedRequest {
    static const int MAX_QUERY_SIZE = 256;
    char query[MAX_QUERY_SIZE];
    uint64_t request_id;
    bool processed;
};

struct SharedResponse {
    static const int MAX_RESPONSE_SIZE = 8192;
    char serialized_response[MAX_RESPONSE_SIZE];
    size_t response_size;
    uint64_t request_id;
    bool valid;
};

// Communication interface to Server B (abstracts gRPC or shared memory)
class BServerCommunication {
public:
    // Factory method that creates appropriate implementation
    static std::unique_ptr<BServerCommunication> Create(const std::string& b_address);

    virtual ~BServerCommunication() = default;

    // Send search request to Server B
    virtual movie::SearchResponse Search(const std::string& query) = 0;

    // Check if connection to Server B is working
    virtual bool IsConnected() const = 0;
};

// gRPC-based implementation
class GrpcBCommunication : public BServerCommunication {
public:
    GrpcBCommunication(const std::string& b_address);
    movie::SearchResponse Search(const std::string& query) override;
    bool IsConnected() const override;

private:
    std::unique_ptr<movie::MovieSearch::Stub> stub_;
    bool connected_ = false;
};

// Shared memory based implementation
class SharedMemoryBCommunication : public BServerCommunication {
public:
    SharedMemoryBCommunication();
    movie::SearchResponse Search(const std::string& query) override;
    bool IsConnected() const override;

private:
    std::unique_ptr<PosixSharedMemory> requests_shm_;
    std::unique_ptr<PosixSharedMemory> responses_shm_;
    std::atomic<uint64_t> next_request_id_{1};
    bool connected_ = false;

    // Wait for response with matching request ID
    bool WaitForResponse(uint64_t request_id, SharedResponse& response, int timeout_ms = 5000);
};

// Function to check if address is local
bool IsLocalAddress(const std::string& address);

#endif // AB_COMMUNICATION_H