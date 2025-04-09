#include <iostream>
#include <memory>
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
using grpc::Status;

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
        movies_ = loadMoviesFromCSV(csv_file);
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[E] Received query: " << query << std::endl;

        // Search in E's local data
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
            }
        }

        return Status::OK;
    }

private:
    std::vector<Movie> movies_;
};

void RunServer(const std::string& server_address, const std::string& csv_file) {
    MovieSearchServiceImpl service(csv_file);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[E] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./E_server <listen_address> <csv_file>" << std::endl;
        return 1;
    }

    std::string e_addr = argv[1]; // e.g., 0.0.0.0:5005
    std::string csv_file = argv[2]; // e.g., e_movies.csv
    
    RunServer(e_addr, csv_file);
    return 0;
}