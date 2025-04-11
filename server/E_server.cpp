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

// ---------- E as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    explicit MovieSearchServiceImpl(const std::string& csv_file) {
        try {
            movies_ = loadMoviesFromCSV(csv_file);
            std::cout << "[E] Successfully loaded movies from " << csv_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[E]  Error loading movies: " << e.what() << std::endl;
        }
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[E] Received query: \"" << query << "\"" << std::endl;
        
        // Special case for ping
        if (query == "__ping__") {
            std::cout << "[E] Received ping request, sending empty response" << std::endl;
            return Status::OK;
        }

        // Search in E's local data
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
        std::cout << "[E] Found " << localMatches << " matches in local data" << std::endl;
        std::cout << "[E] Returning " << response->results_size() << " total results" << std::endl;
        
        return Status::OK;
    }

private:
    std::vector<Movie> movies_;
};

void RunServer(const std::string& server_address, const std::string& csv_file) {
    std::cout << "[E] Starting server on " << server_address << std::endl;
    
    MovieSearchServiceImpl service(csv_file);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (server) {
        std::cout << "[E] Server listening on " << server_address << std::endl;
        server->Wait();
    } else {
        std::cerr << "[E]  Failed to start server on " << server_address << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./E_server <listen_address> <csv_file>" << std::endl;
        std::cerr << "Example: ./E_server 0.0.0.0:50005 movies.csv" << std::endl;
        return 1;
    }

    try {
        std::string e_addr = argv[1]; // e.g., 0.0.0.0:5005
        std::string csv_file = argv[2]; // e.g., e_movies.csv
        
        RunServer(e_addr, csv_file);
    } catch (const std::exception& e) {
        std::cerr << "[E]  Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}