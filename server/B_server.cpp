#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"
#include "movie_struct.h"
#include "posix_shared_memory.h"
#include "response_serializer.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Channel;
using grpc::Status;
using grpc::StatusCode;

using movie::MovieSearch;
using movie::SearchRequest;
using movie::SearchResponse;
using movie::MovieInfo;

// Shared memory structures
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

// Helper function to check if a movie matches a query
bool movieMatchesQuery(const Movie& movie, const std::string& query) {
    // Convert query and relevant fields to lowercase for case-insensitive comparison
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    std::string lowerTitle = movie.title;
    std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);

    std::string lowerGenres = movie.genres;
    std::transform(lowerGenres.begin(), lowerGenres.end(), lowerGenres.begin(), ::tolower);

    std::string lowerOverview = movie.overview;
    std::transform(lowerOverview.begin(), lowerOverview.end(), lowerOverview.begin(), ::tolower);

    std::string lowerKeywords = movie.keywords;
    std::transform(lowerKeywords.begin(), lowerKeywords.end(), lowerKeywords.begin(), ::tolower);

    return
        lowerTitle.find(lowerQuery) != std::string::npos ||
        lowerGenres.find(lowerQuery) != std::string::npos ||
        lowerOverview.find(lowerQuery) != std::string::npos ||
        lowerKeywords.find(lowerQuery) != std::string::npos;
}

// ---------- B as gRPC Client to C ----------
class CClient {
public:
    CClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {
        // Test connection to C on startup
        SearchRequest ping;
        ping.set_title("__ping__");
        SearchResponse pong;
        ClientContext context;

        std::cout << "[B] Testing connection to server C..." << std::endl;
        Status status = stub_->Search(&context, ping, &pong);

        if (status.ok()) {
            std::cout << "[B] ✅ Successfully connected to server C" << std::endl;
            connected_ = true;
        } else {
            std::cerr << "[B] ❌ Failed to connect to server C: "
                     << status.error_message()
                     << " (code: " << status.error_code() << ")" << std::endl;
            if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[B] 🔍 This usually means server C is not running or the address is incorrect" << std::endl;
            }
            connected_ = false;
        }
    }

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        // Set a timeout for the request (5 seconds)
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::seconds(5);
        context.set_deadline(deadline);

        std::cout << "[B] Sending request to server C: \"" << title << "\"" << std::endl;
        Status status = stub_->Search(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "[B → C] gRPC call failed: " << status.error_message()
                     << " (code: " << status.error_code() << ")" << std::endl;

            if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
                std::cerr << "[B] 🕒 Request timed out. Server C might be overloaded or unresponsive." << std::endl;
            } else if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[B] 🔌 Server C is unavailable. Network issue or server not running." << std::endl;
            }

            connected_ = false;
        } else {
            connected_ = true;
            std::cout << "[B] Received " << response.results_size() << " results from server C" << std::endl;
        }

        return response;
    }

    bool isConnected() const {
        return connected_;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
    bool connected_ = false;
};

// ---------- B as gRPC Client to D ----------
class DClient {
public:
    DClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {
        // Test connection to D on startup
        SearchRequest ping;
        ping.set_title("__ping__");
        SearchResponse pong;
        ClientContext context;

        std::cout << "[B] Testing connection to server D..." << std::endl;
        Status status = stub_->Search(&context, ping, &pong);

        if (status.ok()) {
            std::cout << "[B] ✅ Successfully connected to server D" << std::endl;
            connected_ = true;
        } else {
            std::cerr << "[B] ❌ Failed to connect to server D: "
                     << status.error_message()
                     << " (code: " << status.error_code() << ")" << std::endl;
            if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[B] 🔍 This usually means server D is not running or the address is incorrect" << std::endl;
            }
            connected_ = false;
        }
    }

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        // Set a timeout for the request (5 seconds)
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::seconds(5);
        context.set_deadline(deadline);

        std::cout << "[B] Sending request to server D: \"" << title << "\"" << std::endl;
        Status status = stub_->Search(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "[B → D] gRPC call failed: " << status.error_message()
                     << " (code: " << status.error_code() << ")" << std::endl;

            if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
                std::cerr << "[B] 🕒 Request timed out. Server D might be overloaded or unresponsive." << std::endl;
            } else if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[B] 🔌 Server D is unavailable. Network issue or server not running." << std::endl;
            }

            connected_ = false;
        } else {
            connected_ = true;
            std::cout << "[B] Received " << response.results_size() << " results from server D" << std::endl;
        }

        return response;
    }

    bool isConnected() const {
        return connected_;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
    bool connected_ = false;
};

// ---------- B as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& c_address, const std::string& d_address, const std::string& csv_file)
        : c_client_(grpc::CreateChannel(c_address, grpc::InsecureChannelCredentials())),
          d_client_(grpc::CreateChannel(d_address, grpc::InsecureChannelCredentials())) {
        try {
            movies_ = loadMoviesFromCSV(csv_file);
            std::cout << "[B] Successfully loaded movies from " << csv_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[B] ❌ Error loading movies: " << e.what() << std::endl;
        }
    }

// In B_server.cpp - Add deduplication logic in the Search method

Status Search(ServerContext* context, const SearchRequest* request,
              SearchResponse* response) override {
    std::string query = request->title();
    std::cout << "[B] Received query: \"" << query << "\"" << std::endl;
    
    // Special case for ping
    if (query == "__ping__") {
        std::cout << "[B] Received ping request, sending empty response" << std::endl;
        return Status::OK;
    }

    // Search in B's local data
    int localMatches = 0;
    for (const auto& movie : movies_) {
        if (movieMatchesQuery(movie, query)) {
            MovieInfo* result = response->add_results();
            result->set_title(movie.title);
            result->set_director(movie.production_companies);
            result->set_genre(movie.genres);
            
            // Parse year from release date
            if (!movie.release_date.empty()) {
                try {
                    result->set_year(2000 + std::stoi(movie.release_date.substr(movie.release_date.length() - 2)));
                } catch (...) {
                    result->set_year(0); // Default if parsing fails
                }
            }
            localMatches++;
        }
    }
    std::cout << "[B] Found " << localMatches << " matches in local data" << std::endl;

    // Store all results in a map to track unique movies (by title)
    std::unordered_map<std::string, MovieInfo> uniqueMovies;
    
    // Add B's results to uniqueMovies map
    for (int i = 0; i < response->results_size(); i++) {
        const MovieInfo& movie = response->results(i);
        uniqueMovies[movie.title()] = movie;
    }

    // Forward request to C if connected
    if (c_client_.isConnected()) {
        std::cout << "[B] Forwarding query to server C: \"" << query << "\"" << std::endl;
        SearchResponse c_response = c_client_.Search(query);

        int cMatches = 0;
        for (const auto& movie : c_response.results()) {
            // Check if this movie is already in our results
            if (uniqueMovies.find(movie.title()) == uniqueMovies.end()) {
                uniqueMovies[movie.title()] = movie;
                cMatches++;
            } else {
                std::cout << "[B] ⚠️ Duplicate movie skipped: " << movie.title() << std::endl;
            }
        }
        std::cout << "[B] Added " << cMatches << " unique results from server C" << std::endl;
    } else {
        std::cerr << "[B] ⚠️ Skipping forward to server C - connection is down" << std::endl;
    }

    // Forward request to D if connected
    if (d_client_.isConnected()) {
        std::cout << "[B] Forwarding query to server D: \"" << query << "\"" << std::endl;
        SearchResponse d_response = d_client_.Search(query);

        int dMatches = 0;
        for (const auto& movie : d_response.results()) {
            // Check if this movie is already in our results
            if (uniqueMovies.find(movie.title()) == uniqueMovies.end()) {
                uniqueMovies[movie.title()] = movie;
                dMatches++;
            } else {
                std::cout << "[B] ⚠️ Duplicate movie skipped: " << movie.title() << std::endl;
            }
        }
        std::cout << "[B] Added " << dMatches << " unique results from server D" << std::endl;
    } else {
        std::cerr << "[B] ⚠️ Skipping forward to server D - connection is down" << std::endl;
    }

    // Clear the original response and rebuild with unique movies
    response->clear_results();
    for (const auto& pair : uniqueMovies) {
        *response->add_results() = pair.second;
    }

    std::cout << "[B] Returning " << response->results_size() << " deduplicated results to server A" << std::endl;
    return Status::OK;
}

private:
    CClient c_client_;
    DClient d_client_;
    std::vector<Movie> movies_;
};

// Shared memory listener for Server B
class SharedMemoryListener {
public:
    SharedMemoryListener(MovieSearchServiceImpl* service)
        : service_(service), running_(false) {}

    ~SharedMemoryListener() {
        Stop();
    }

    void Start() {
        if (running_) return;

        try {
            // Open shared memory segments
            requests_shm_ = std::make_unique<PosixSharedMemory>("/movie_ab_requests", 1024 * 1024, true);
            responses_shm_ = std::make_unique<PosixSharedMemory>("/movie_ab_responses", 5 * 1024 * 1024, true);

            running_ = true;
            listener_thread_ = std::thread(&SharedMemoryListener::ListenerLoop, this);
            std::cout << "[B] Shared memory listener started" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[B] Failed to initialize shared memory listener: " << e.what() << std::endl;
        }
    }

    void Stop() {
        running_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
    }

private:
    void ListenerLoop() {
        std::cout << "[B] Shared memory listener thread started" << std::endl;

        // Track which request IDs we've seen
        std::unordered_set<uint64_t> processed_requests;

        while (running_) {
            try {
                // Scan for new entries every 100ms
                auto start_time = std::chrono::steady_clock::now();
                bool found_new_request = false;

                // Note: In a real implementation, you might want a more efficient
                // way to discover new requests instead of polling
                for (uint64_t id = 1; id < 10000; id++) {
                    std::string key = std::to_string(id);

                    // Skip if we've already processed this request
                    if (processed_requests.find(id) != processed_requests.end()) {
                        continue;
                    }

                    // Check if request exists
                    std::vector<uint8_t> req_data;
                    if (requests_shm_->read(key, req_data)) {
                        if (req_data.size() >= sizeof(SharedRequest)) {
                            SharedRequest request;
                            memcpy(&request, req_data.data(), sizeof(SharedRequest));

                            // Skip if already processed
                            if (request.processed) {
                                processed_requests.insert(id);
                                continue;
                            }

                            found_new_request = true;

                            // Mark as processed
                            request.processed = true;
                            memcpy(req_data.data(), &request, sizeof(SharedRequest));
                            requests_shm_->write(key, req_data);

                            // Process request
                            std::string query = request.query;
                            std::cout << "[B] Received shared memory request: \"" << query
                                    << "\" (ID: " << id << ")" << std::endl;

                            // Special handling for ping
                            if (query == "__ping__") {
                                SharedResponse response{};
                                response.request_id = id;
                                response.response_size = 0;
                                response.valid = true;

                                // Write empty response
                                std::vector<uint8_t> resp_data(sizeof(SharedResponse));
                                memcpy(resp_data.data(), &response, sizeof(SharedResponse));
                                responses_shm_->write(key, resp_data);
                                std::cout << "[B] Responded to ping request" << std::endl;

                                processed_requests.insert(id);
                                continue;
                            }

                            // Regular search request
                            movie::SearchRequest grpc_req;
                            grpc_req.set_title(query);
                            movie::SearchResponse grpc_resp;
                            grpc::ServerContext ctx;

                            // Process using the standard service implementation
                            service_->Search(&ctx, &grpc_req, &grpc_resp);

 // Prepare response
                            SharedResponse response{};
                            response.request_id = id;
                            response.valid = true;

                            // Serialize search response
                            std::vector<uint8_t> serialized = ResponseSerializer::serialize(grpc_resp);
                            if (serialized.size() <= SharedResponse::MAX_RESPONSE_SIZE) {
                                response.response_size = serialized.size();
                                memcpy(response.serialized_response, serialized.data(), serialized.size());
                            } else {
                                std::cerr << "[B] Response too large for shared memory buffer" << std::endl;
                                response.valid = false;
                                response.response_size = 0;
                            }

                            // Write response to shared memory
                            std::vector<uint8_t> resp_data(sizeof(SharedResponse));
                            memcpy(resp_data.data(), &response, sizeof(SharedResponse));
                            responses_shm_->write(key, resp_data);

                            std::cout << "[B] Wrote response with " << grpc_resp.results_size()
                                     << " results to shared memory" << std::endl;

                            // Remember we've processed this request
                            processed_requests.insert(id);

                            // Keep processed_requests from growing too large
                            if (processed_requests.size() > 1000) {
                                auto oldest = processed_requests.begin();
                                processed_requests.erase(oldest);
                            }
                        }
                    }
                }

                // If no new requests, sleep for a bit
                if (!found_new_request) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (const std::exception& e) {
                std::cerr << "[B] Error in shared memory listener: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    MovieSearchServiceImpl* service_;
    std::unique_ptr<PosixSharedMemory> requests_shm_;
    std::unique_ptr<PosixSharedMemory> responses_shm_;
    std::thread listener_thread_;
    std::atomic<bool> running_;
};

void RunServer(const std::string& server_address, const std::string& c_address,
               const std::string& d_address, const std::string& csv_file) {
    std::cout << "[B] Starting server on " << server_address << std::endl;
    std::cout << "[B] Will connect to server C at " << c_address << std::endl;
    std::cout << "[B] Will connect to server D at " << d_address << std::endl;

    MovieSearchServiceImpl service(c_address, d_address, csv_file);

    // Start shared memory listener
    SharedMemoryListener shm_listener(&service);
    shm_listener.Start();

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (server) {
        std::cout << "[B] Server listening on " << server_address << std::endl;
        server->Wait();
    } else {
        std::cerr << "[B] ❌ Failed to start server on " << server_address << std::endl;
    }

    // Stop shared memory listener when server exits
    shm_listener.Stop();
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: ./B_server <listen_address> <C_address> <D_address> <csv_file>" << std::endl;
        std::cerr << "Example: ./B_server 0.0.0.0:50002 localhost:50003 localhost:50004 movies.csv" << std::endl;
        return 1;
    }

    try {
        std::string b_addr = argv[1]; // e.g., 0.0.0.0:5002
        std::string c_addr = argv[2]; // e.g., 192.168.0.4:5003
        std::string d_addr = argv[3]; // e.g., 192.168.0.4:5004
        std::string csv_file = argv[4]; // e.g., b_movies.csv

        // Register signal handler for cleanup
        signal(SIGINT, [](int) {
            std::cout << "\n[B] Exiting..." << std::endl;
            exit(0);
        });

        RunServer(b_addr, c_addr, d_addr, csv_file);
    } catch (const std::exception& e) {
        std::cerr << "[B] ❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}