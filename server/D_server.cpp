#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"
#include "movie_struct.h" // Include our movie structure header

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

// ---------- D as gRPC Client to E ----------
class EClient {
public:
    EClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {
        // Test connection to E on startup
        SearchRequest ping;
        ping.set_title("__ping__");
        SearchResponse pong;
        ClientContext context;
        
        std::cout << "[D] Testing connection to server E..." << std::endl;
        Status status = stub_->Search(&context, ping, &pong);
        
        if (status.ok()) {
            std::cout << "[D] Successfully connected to server E" << std::endl;
            connected_ = true;
        } else {
            std::cerr << "[D]  Failed to connect to server E: " 
                     << status.error_message() 
                     << " (code: " << status.error_code() << ")" << std::endl;
            if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[D] 🔍 This usually means server E is not running or the address is incorrect" << std::endl;
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

        std::cout << "[D] Sending request to server E: \"" << title << "\"" << std::endl;
        Status status = stub_->Search(&context, request, &response);
        
        if (!status.ok()) {
            std::cerr << "[D → E] gRPC call failed: " << status.error_message() 
                     << " (code: " << status.error_code() << ")" << std::endl;
            
            if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
                std::cerr << "[D] 🕒 Request timed out. Server E might be overloaded or unresponsive." << std::endl;
            } else if (status.error_code() == StatusCode::UNAVAILABLE) {
                std::cerr << "[D] 🔌 Server E is unavailable. Network issue or server not running." << std::endl;
            }
            
            connected_ = false;
        } else {
            connected_ = true;
            std::cout << "[D] Received " << response.results_size() << " results from server E" << std::endl;
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

// ---------- D as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& e_address, const std::string& csv_file)
        : e_client_(grpc::CreateChannel(e_address, grpc::InsecureChannelCredentials())) {
        try {
            movies_ = loadMoviesFromCSV(csv_file);
            std::cout << "[D] Successfully loaded movies from " << csv_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[D]  Error loading movies: " << e.what() << std::endl;
        }
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[D] Received query: \"" << query << "\"" << std::endl;
        
        // Special case for ping
        if (query == "__ping__") {
            std::cout << "[D] Received ping request, sending empty response" << std::endl;
            return Status::OK;
        }

        // Search in D's local data
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
        std::cout << "[D] Found " << localMatches << " matches in local data" << std::endl;

        // Forward request to E if connected
        if (e_client_.isConnected()) {
            std::cout << "[D] Forwarding query to server E: \"" << query << "\"" << std::endl;
            SearchResponse e_response = e_client_.Search(query);

            int eMatches = 0;
            for (const auto& movie : e_response.results()) {
                *response->add_results() = movie;
                eMatches++;
            }
            std::cout << "[D] Added " << eMatches << " results from server E" << std::endl;
        } else {
            std::cerr << "[D] ⚠️ Skipping forward to server E - connection is down" << std::endl;
        }

        std::cout << "[D] Returning " << response->results_size() << " total results to server B" << std::endl;
        return Status::OK;
    }

private:
    EClient e_client_;
    std::vector<Movie> movies_;
};

void RunServer(const std::string& server_address, const std::string& e_address, const std::string& csv_file) {
    std::cout << "[D] Starting server on " << server_address << std::endl;
    std::cout << "[D] Will connect to server E at " << e_address << std::endl;
    
    MovieSearchServiceImpl service(e_address, csv_file);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (server) {
        std::cout << "[D] Server listening on " << server_address << std::endl;
        server->Wait();
    } else {
        std::cerr << "[D]  Failed to start server on " << server_address << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: ./D_server <listen_address> <E_address> <csv_file>" << std::endl;
        std::cerr << "Example: ./D_server 0.0.0.0:50004 localhost:50005 movies.csv" << std::endl;
        return 1;
    }

    try {
        std::string d_addr = argv[1]; // e.g., 0.0.0.0:5004
        std::string e_addr = argv[2]; // e.g., 192.168.0.5:5005
        std::string csv_file = argv[3]; // e.g., d_movies.csv
        
        RunServer(d_addr, e_addr, csv_file);
    } catch (const std::exception& e) {
        std::cerr << "[D]  Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}