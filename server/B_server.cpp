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
using grpc::ClientContext;
using grpc::Channel;
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

// ---------- B as gRPC Client to C ----------
class CClient {
public:
    CClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[B → C] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- B as gRPC Client to D ----------
class DClient {
public:
    DClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[B → D] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- B as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& c_address, const std::string& d_address, const std::string& csv_file)
        : c_client_(grpc::CreateChannel(c_address, grpc::InsecureChannelCredentials())),
          d_client_(grpc::CreateChannel(d_address, grpc::InsecureChannelCredentials())) {
        movies_ = loadMoviesFromCSV(csv_file);
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[B] Received query: " << query << std::endl;

        // Search in B's local data
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

        // Forward request to C
        std::cout << "[B] Forwarding to Process C..." << std::endl;
        SearchResponse c_response = c_client_.Search(query);

        // Forward request to D
        std::cout << "[B] Forwarding to Process D..." << std::endl;
        SearchResponse d_response = d_client_.Search(query);

        // Gather results from C
        for (const auto& movie : c_response.results()) {
            *response->add_results() = movie;
        }

        // Gather results from D
        for (const auto& movie : d_response.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    CClient c_client_;
    DClient d_client_;
    std::vector<Movie> movies_;
};

void RunServer(const std::string& server_address, const std::string& c_address, 
               const std::string& d_address, const std::string& csv_file) {
    MovieSearchServiceImpl service(c_address, d_address, csv_file);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[B] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: ./B_server <listen_address> <C_address> <D_address> <csv_file>" << std::endl;
        return 1;
    }

    std::string b_addr = argv[1]; // e.g., 0.0.0.0:5002
    std::string c_addr = argv[2]; // e.g., 192.168.0.4:5003
    std::string d_addr = argv[3]; // e.g., 192.168.0.4:5004
    std::string csv_file = argv[4]; // e.g., b_movies.csv
    
    RunServer(b_addr, c_addr, d_addr, csv_file);
    return 0;
}