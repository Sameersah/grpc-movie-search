#include <iostream>
#include <memory>
#include <string>
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

    void SearchMovie(const std::string& title) {
        SearchRequest request;
        request.set_title(title);

        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);

        if (status.ok()) {
            std::cout << "🎬 Results from A:\n";
            for (const auto& movie : response.results()) {
                std::cout << movie.title() << " - " << movie.director()
                          << " (" << movie.year() << ") [" << movie.genre() << "]\n";
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
        return 1;
    }

    std::string a_address = argv[1];  // Example: localhost:5001
    MovieClient client(grpc::CreateChannel(a_address, grpc::InsecureChannelCredentials()));

    std::string title;
    std::cout << "Enter movie title: ";
    std::getline(std::cin, title);

    client.SearchMovie(title);

    return 0;
}