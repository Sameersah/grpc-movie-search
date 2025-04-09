#include <iostream>
#include <memory>
#include <string>
#include <iomanip>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using movie::MovieSearch;
using movie::SearchRequest;
using movie::SearchResponse;
using movie::MovieInfo;

class MovieClient {
public:
    MovieClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    void SearchMovie(const std::string& query) {
        SearchRequest request;
        request.set_title(query);

        SearchResponse response;
        ClientContext context;

        auto start_time = std::chrono::high_resolution_clock::now();
        Status status = stub_->Search(&context, request, &response);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (status.ok()) {
            std::cout << "🎬 Results for query \"" << query << "\" (found " 
                      << response.results_size() << " matches in " 
                      << duration.count() << "ms):\n";
            
            if (response.results_size() == 0) {
                std::cout << "No movies found matching your query.\n";
            } else {
                // Print header
                std::cout << std::left << std::setw(40) << "TITLE" 
                          << std::setw(30) << "PRODUCTION" 
                          << std::setw(25) << "GENRE" 
                          << "YEAR\n";
                std::cout << std::string(100, '-') << "\n";
                
                // Print each movie
                for (const auto& movie : response.results()) {
                    std::string title = movie.title();
                    if (title.length() > 37) {
                        title = title.substr(0, 34) + "...";
                    }
                    
                    std::string director = movie.director();
                    if (director.length() > 27) {
                        director = director.substr(0, 24) + "...";
                    }
                    
                    std::string genre = movie.genre();
                    if (genre.length() > 22) {
                        genre = genre.substr(0, 19) + "...";
                    }
                    
                    std::cout << std::left << std::setw(40) << title
                              << std::setw(30) << director
                              << std::setw(25) << genre
                              << movie.year() << "\n";
                }
            }
        } else {
            std::cerr << "❌ gRPC failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./client <A_address>\n";
        std::cerr << "Example: ./client localhost:50001\n";
        return 1;
    }

    std::string a_address = argv[1];
    MovieClient client(grpc::CreateChannel(a_address, grpc::InsecureChannelCredentials()));

    std::string query;
    std::cout << "🔍 Enter search term (title, genre, keywords): ";
    std::getline(std::cin, query);

    client.SearchMovie(query);

    return 0;
}