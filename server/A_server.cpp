#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>       // For std::thread
#include <chrono>       // For std::chrono
#include <csignal>      // For signal handling (instead of std::signal)
#include <iomanip>      // For std::setprecision
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"
#include "movie_struct.h" // Include our movie structure header
#include "cache.h" // Include our cache implementation
#include "posix_shared_memory.h" // Include our shared memory implementation
#include "response_serializer.h" // Include our response serializer

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

// ---------- A as gRPC Client to B ----------
class BClient {
public:
    BClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {
        // Test connection to B on startup
        SearchRequest ping;
        ping.set_title("__ping__");
        SearchResponse pong;
        ClientContext context;
        
        std::cout << "[A] Testing connection to server B..." << std::endl;
        Status status = stub_->Search(&context, ping, &pong);
        
        if (status.ok()) {
            std::cout << "[A] Successfully connected to server B" << std::endl;
            connected_ = true;
        } else {
            std::cerr << "[A]  Failed to connect to server B: " 
                     << status.error_message() 
                     << " (code: " << status.error_code() << ")" << std::endl;
            if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[A] 🔍 This usually means server B is not running or the address is incorrect" << std::endl;
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

        std::cout << "[A] Sending request to server B: \"" << title << "\"" << std::endl;
        Status status = stub_->Search(&context, request, &response);
        
        if (!status.ok()) {
            std::cerr << "[A → B] gRPC call failed: " << status.error_message() 
                     << " (code: " << status.error_code() << ")" << std::endl;
            
            if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
                std::cerr << "[A] 🕒 Request timed out. Server B might be overloaded or unresponsive." << std::endl;
            } else if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[A] 🔌 Server B is unavailable. Network issue or server not running." << std::endl;
            }
            
            connected_ = false;
        } else {
            connected_ = true;
            std::cout << "[A] Received " << response.results_size() << " results from server B" << std::endl;
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

// ---------- A as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& b_address, const std::string& csv_file, 
                          int cache_ttl = 300, size_t cache_size = 100)
        : b_client_(grpc::CreateChannel(b_address, grpc::InsecureChannelCredentials())), 
          cache_(cache_ttl, cache_size) {
        try {
            // Load local movie data
            movies_ = loadMoviesFromCSV(csv_file);
            std::cout << "[A] Successfully loaded movies from " << csv_file << std::endl;
            
            // Initialize shared memory
            try {
                const size_t SHM_SIZE = 10 * 1024 * 1024; // 10 MB shared memory
                shm_ = std::make_unique<PosixSharedMemory>("/movie_search_cache", SHM_SIZE);
                std::cout << "[A] Successfully initialized shared memory" << std::endl;
                shm_available_ = true;
            } catch (const std::exception& e) {
                std::cerr << "[A] ⚠️ Failed to initialize shared memory: " << e.what() << std::endl;
                std::cerr << "[A] ⚠️ Will continue without shared memory" << std::endl;
                shm_available_ = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[A]  Error loading movies: " << e.what() << std::endl;
        }
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::string query = request->title();
        std::cout << "[A] Received query: \"" << query << "\"" << std::endl;
        
        // Special case for ping
        if (query == "__ping__") {
            std::cout << "[A] Received ping request, sending empty response" << std::endl;
            return Status::OK;
        }
        
        // Try to get from in-memory cache first
        bool cache_hit = cache_.get(query, *response);
        
        if (cache_hit) {
            std::cout << "[A] 🎯 Cache hit for query: \"" << query << "\"" << std::endl;
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "[A] Query completed in " << duration.count() << "ms (from cache)" << std::endl;
            return Status::OK;
        }
        
        // If not in memory cache, try shared memory
        if (shm_available_) {
            std::vector<uint8_t> serialized_data;
            bool shm_hit = shm_->read(query, serialized_data);
            
            if (shm_hit) {
                // Deserialize response from shared memory
                bool deserialized = ResponseSerializer::deserialize(serialized_data, *response);
                
                if (deserialized) {
                    std::cout << "[A] 💾 Shared memory hit for query: \"" << query << "\"" << std::endl;
                    
                    // Also update in-memory cache
                    cache_.put(query, *response);
                    
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    std::cout << "[A] Query completed in " << duration.count() << "ms (from shared memory)" << std::endl;
                    return Status::OK;
                }
            }
        }
        
        // Cache miss, need to search locally and forward request
        std::cout << "[A] 🔍 Cache miss for query: \"" << query << "\"" << std::endl;
        
        // Search in A's local data
        int localMatches = 0;
        for (const auto& movie : movies_) {
            if (movieMatchesQuery(movie, query)) {
                MovieInfo* result = response->add_results();
                result->set_title(movie.title);
                result->set_director(movie.production_companies); // Using production companies as "director"
                result->set_genre(movie.genres);
                
                // Parse year from release date (format: MM/DD/YY)
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
        std::cout << "[A] Found " << localMatches << " matches in local data" << std::endl;

        // Forward request to Process B if connected
        if (b_client_.isConnected()) {
            std::cout << "[A] Forwarding query to server B: \"" << query << "\"" << std::endl;
            SearchResponse b_response = b_client_.Search(query);

            int bMatches = 0;
            for (const auto& movie : b_response.results()) {
                *response->add_results() = movie;
                bMatches++;
            }
            std::cout << "[A] Added " << bMatches << " results from server B" << std::endl;
        } else {
            std::cerr << "[A] ⚠️ Skipping forward to server B - connection is down" << std::endl;
        }

        // Store response in caches
        if (response->results_size() > 0) {
            // Store in memory cache
            cache_.put(query, *response);
            
            // Store in shared memory if available
            if (shm_available_) {
                try {
                    std::vector<uint8_t> serialized_data = ResponseSerializer::serialize(*response);
                    bool stored = shm_->write(query, serialized_data);
                    
                    if (stored) {
                        std::cout << "[A] 💾 Stored result in shared memory" << std::endl;
                    } else {
                        std::cerr << "[A] ⚠️ Failed to store in shared memory (possibly out of space)" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[A] ⚠️ Error serializing/storing in shared memory: " << e.what() << std::endl;
                }
            }
        }

        std::cout << "[A] Returning " << response->results_size() << " total results to client" << std::endl;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "[A] Query completed in " << duration.count() << "ms (from search)" << std::endl;
        
        // Print cache statistics
        std::cout << "[A] Cache stats: " << cache_.size() << " entries, "
                 << cache_.hit_count() << " hits, "
                 << cache_.miss_count() << " misses, "
                 << std::fixed << std::setprecision(2) << (cache_.hit_ratio() * 100.0) << "% hit ratio" 
                 << std::endl;
                 
        return Status::OK;
    }

    // Print cache statistics
    void printCacheStats() {
        std::cout << "\n===== Cache Statistics =====" << std::endl;
        std::cout << "Entries: " << cache_.size() << std::endl;
        std::cout << "Hits: " << cache_.hit_count() << std::endl;
        std::cout << "Misses: " << cache_.miss_count() << std::endl;
        std::cout << "Hit ratio: " << std::fixed << std::setprecision(2) 
                 << (cache_.hit_ratio() * 100.0) << "%" << std::endl;
        
        if (shm_available_) {
            std::cout << "Shared memory entries: ~" << shm_->count() << std::endl;
            std::cout << "Shared memory used: " << (shm_->usedBytes() / 1024) << " KB" << std::endl;
        }
    }

private:
    BClient b_client_;
    std::vector<Movie> movies_;
    Cache cache_;
    std::unique_ptr<PosixSharedMemory> shm_;
    bool shm_available_ = false;
};

void RunServer(const std::string& server_address, const std::string& b_address, 
               const std::string& csv_file, int cache_ttl, size_t cache_size) {
    std::cout << "[A] Starting server on " << server_address << std::endl;
    std::cout << "[A] Will connect to server B at " << b_address << std::endl;
    std::cout << "[A] Cache TTL: " << cache_ttl << " seconds, max size: " << cache_size << " entries" << std::endl;
    
    MovieSearchServiceImpl service(b_address, csv_file, cache_ttl, cache_size);

    ServerBuilder builder;
    // Set timeout options
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (server) {
        std::cout << "[A] Server listening on " << server_address << std::endl;
        
        // Print cache stats periodically in a separate thread
        std::thread stats_thread([&service]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                service.printCacheStats();
            }
        });
        
        // Detach the thread so it runs independently
        stats_thread.detach();
        
        server->Wait();
    } else {
        std::cerr << "[A]  Failed to start server on " << server_address << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: ./A_server <listen_address> <B_address> <csv_file> [cache_ttl] [cache_size]" << std::endl;
        std::cerr << "Example: ./A_server 0.0.0.0:50001 localhost:50002 movies.csv 300 1000" << std::endl;
        std::cerr << "  cache_ttl: Time-to-live for cache entries in seconds (default: 300)" << std::endl;
        std::cerr << "  cache_size: Maximum number of entries in cache (default: 100)" << std::endl;
        return 1;
    }

    try {
        std::string server_address = argv[1]; // e.g., 0.0.0.0:50001
        std::string b_address = argv[2]; // e.g., 192.168.0.3:5002
        std::string csv_file = argv[3]; // e.g., a_movies.csv
        
        // Parse optional cache parameters
        int cache_ttl = 300; // Default: 5 minutes
        size_t cache_size = 100; // Default: 100 entries
        
        if (argc >= 5) {
            cache_ttl = std::stoi(argv[4]);
        }
        
        if (argc >= 6) {
            cache_size = std::stoul(argv[5]);
        }
        
        // Register signal handler for cleanup
        signal(SIGINT, [](int) {
            std::cout << "\n[A] Cleaning up shared memory..." << std::endl;
            PosixSharedMemory::destroy("/movie_search_cache");
            std::cout << "[A] Exiting..." << std::endl;
            exit(0);
        });
        
        RunServer(server_address, b_address, csv_file, cache_ttl, cache_size);
    } catch (const std::exception& e) {
        std::cerr << "[A]  Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}