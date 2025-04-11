// ab_communication.cpp
#include "ab_communication.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "response_serializer.h"

// Create appropriate communication implementation based on address
std::unique_ptr<BServerCommunication> BServerCommunication::Create(const std::string& b_address) {
    if (IsLocalAddress(b_address)) {
        std::cout << "[A] Server B is on local machine, using shared memory communication" << std::endl;
        return std::make_unique<SharedMemoryBCommunication>();
    } else {
        std::cout << "[A] Server B is on remote machine, using gRPC communication" << std::endl;
        return std::make_unique<GrpcBCommunication>(b_address);
    }
}

// Check if address is local
bool IsLocalAddress(const std::string& address) {
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    std::string host = address.substr(0, colon_pos);

    // Check for localhost variants
    if (host == "localhost" || host == "127.0.0.1" || host == "::1") {
        return true;
    }

    // Check if it matches any local interface
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct hostent *host_entry = gethostbyname(hostname);
    if (host_entry == nullptr) {
        return false;
    }

    for (int i = 0; host_entry->h_addr_list[i] != nullptr; i++) {
        char* ip = inet_ntoa(*(struct in_addr*)host_entry->h_addr_list[i]);
        if (host == ip) {
            return true;
        }
    }

    return false;
}

// GRPC Implementation
GrpcBCommunication::GrpcBCommunication(const std::string& b_address)
    : stub_(movie::MovieSearch::NewStub(grpc::CreateChannel(b_address, grpc::InsecureChannelCredentials()))) {

    // Test connection to B on startup
    movie::SearchRequest ping;
    ping.set_title("__ping__");
    movie::SearchResponse pong;
    grpc::ClientContext context;

    std::cout << "[A] Testing gRPC connection to server B..." << std::endl;
    grpc::Status status = stub_->Search(&context, ping, &pong);

    if (status.ok()) {
        std::cout << "[A] Successfully connected to server B via gRPC" << std::endl;
        connected_ = true;
    } else {
        std::cerr << "[A]  Failed to connect to server B: "
                 << status.error_message() << std::endl;
        connected_ = false;
    }
}

movie::SearchResponse GrpcBCommunication::Search(const std::string& query) {
    movie::SearchRequest request;
    request.set_title(query);
    movie::SearchResponse response;
    grpc::ClientContext context;

    std::cout << "[A] Sending gRPC request to server B: \"" << query << "\"" << std::endl;
    grpc::Status status = stub_->Search(&context, request, &response);

    if (!status.ok()) {
        std::cerr << "[A → B] gRPC call failed: " << status.error_message() << std::endl;
        connected_ = false;
    } else {
        connected_ = true;
        std::cout << "[A] Received " << response.results_size() << " results from server B" << std::endl;
    }

    return response;
}

bool GrpcBCommunication::IsConnected() const {
    return connected_;
}

// Shared Memory Implementation
SharedMemoryBCommunication::SharedMemoryBCommunication() {
    try {
        // Create shared memory segments
        requests_shm_ = std::make_unique<PosixSharedMemory>("/movie_ab_requests", 64 * 1024);
        responses_shm_ = std::make_unique<PosixSharedMemory>("/movie_ab_responses", 256 * 1024);

        // Test connection
        std::cout << "[A] Testing shared memory connection to server B..." << std::endl;

        // Send ping request
        uint64_t ping_id = next_request_id_++;
        SharedRequest ping_req{};
        strncpy(ping_req.query, "__ping__", SharedRequest::MAX_QUERY_SIZE);
        ping_req.request_id = ping_id;
        ping_req.processed = false;

        // Serialize and store in shared memory
        std::vector<uint8_t> req_data(sizeof(SharedRequest));
        memcpy(req_data.data(), &ping_req, sizeof(SharedRequest));

        // Write ping to shared memory
        if (!requests_shm_->write(std::to_string(ping_id), req_data)) {
            std::cerr << "[A]  Failed to write ping request to shared memory" << std::endl;
            connected_ = false;
            return;
        }

        // Wait for response
        SharedResponse ping_resp{};
        if (WaitForResponse(ping_id, ping_resp)) {
            std::cout << "[A] Successfully connected to server B via shared memory" << std::endl;
            connected_ = true;
        } else {
            std::cerr << "[A]  No response from server B via shared memory" << std::endl;
            connected_ = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[A]  Failed to initialize shared memory: " << e.what() << std::endl;
        connected_ = false;
    }
}

movie::SearchResponse SharedMemoryBCommunication::Search(const std::string& query) {
    movie::SearchResponse response;

    if (!connected_) {
        std::cerr << "[A] Cannot search - not connected to Server B via shared memory" << std::endl;
        return response;
    }

    try {
        // Create request
        uint64_t req_id = next_request_id_++;
        SharedRequest request{};
        strncpy(request.query, query.c_str(), SharedRequest::MAX_QUERY_SIZE - 1);
        request.request_id = req_id;
        request.processed = false;

        // Serialize and store in shared memory
        std::vector<uint8_t> req_data(sizeof(SharedRequest));
        memcpy(req_data.data(), &request, sizeof(SharedRequest));

        std::cout << "[A] Sending shared memory request to server B: \"" << query
                  << "\" (ID: " << req_id << ")" << std::endl;

        // Write request to shared memory
        if (!requests_shm_->write(std::to_string(req_id), req_data)) {
            std::cerr << "[A]  Failed to write request to shared memory" << std::endl;
            connected_ = false;
            return response;
        }

        // Wait for response
        SharedResponse resp{};
        if (WaitForResponse(req_id, resp)) {
            // Deserialize response
            std::vector<uint8_t> resp_data(resp.serialized_response,
                                          resp.serialized_response + resp.response_size);
            ResponseSerializer::deserialize(resp_data, response);

            std::cout << "[A] Received " << response.results_size()
                      << " results from server B via shared memory" << std::endl;
        } else {
            std::cerr << "[A]  Timeout waiting for response from server B" << std::endl;
            connected_ = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[A]  Error in shared memory communication: " << e.what() << std::endl;
        connected_ = false;
    }

    return response;
}

bool SharedMemoryBCommunication::IsConnected() const {
    return connected_;
}

bool SharedMemoryBCommunication::WaitForResponse(uint64_t request_id, SharedResponse& response, int timeout_ms) {
    std::string resp_key = std::to_string(request_id);
    auto start_time = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start_time).count() < timeout_ms) {

        // Check for response
        std::vector<uint8_t> resp_data;
        if (responses_shm_->read(resp_key, resp_data)) {
            // Found response
            if (resp_data.size() >= sizeof(SharedResponse)) {
                memcpy(&response, resp_data.data(), sizeof(SharedResponse));

                // Validate it's the response we're waiting for
                if (response.request_id == request_id && response.valid) {
                    // Remove from shared memory to clean up
                    responses_shm_->remove(resp_key);
                    return true;
                }
            }
        }

        // Sleep briefly before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}