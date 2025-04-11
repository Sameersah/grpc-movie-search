#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"
#include "movie_struct.h" // Include our new movie structure header

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

void RunServer(const std::string& server_address, const std::string& c_address, 
               const std::string& d_address, const std::string& csv_file) {
    std::cout << "[B] Starting server on " << server_address << std::endl;
    std::cout << "[B] Will connect to server C at " << c_address << std::endl;
    std::cout << "[B] Will connect to server D at " << d_address << std::endl;
    
    MovieSearchServiceImpl service(c_address, d_address, csv_file);

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
        
        RunServer(b_addr, c_addr, d_addr, csv_file);
    } catch (const std::exception& e) {
        std::cerr << "[B] ❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}