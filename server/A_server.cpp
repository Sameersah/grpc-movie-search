#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
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

// ---------- A as gRPC Client to B ----------
class BClient {
public:
    BClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[A → B] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- A as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& b_address, const std::string& csv_file)
        : b_client_(grpc::CreateChannel(b_address, grpc::InsecureChannelCredentials())) {
        movies_ = loadMoviesFromCSV(csv_file);
    }

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[A] Received query: " << query << std::endl;
        
        // Search in A's local data
        for (const auto& movie : movies_) {
            if (movieMatchesQuery(movie, query)) {
                MovieInfo* result = response->add_results();
                result->set_title(movie.title);
                result->set_director(movie.production_companies); // Using production companies as "director"
                result->set_genre(movie.genres);
                result->set_year(std::stoi(movie.release_date.substr(movie.release_date.length() - 2)));
            }
        }

        // Forward request to Process B
        std::cout << "[A] Forwarding to Process B..." << std::endl;
        SearchResponse b_response = b_client_.Search(query);

        for (const auto& movie : b_response.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    BClient b_client_;
    std::vector<Movie> movies_;
};

void RunServer(const std::string& server_address, const std::string& b_address, const std::string& csv_file) {
    MovieSearchServiceImpl service(b_address, csv_file);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[A] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: ./A_server <listen_address> <B_address> <csv_file>" << std::endl;
        return 1;
    }

    std::string server_address = argv[1]; // e.g., 0.0.0.0:50001
    std::string b_address = argv[2]; // e.g., 192.168.0.3:5002
    std::string csv_file = argv[3]; // e.g., a_movies.csv
    
    RunServer(server_address, b_address, csv_file);
    return 0;
}