#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using movie::MovieSearch;
using movie::SearchRequest;
using movie::SearchResponse;
using movie::MovieInfo;

// ---------- B as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    Status Search(ServerContext* context, const SearchRequest* request,
              SearchResponse* response) override {
    std::string title = request->title();
    std::cout << "[B] ✅ Got request for: " << title << std::endl;

    MovieInfo* movie = response->add_results();
    movie->set_title("Interstellar");
    movie->set_director("Christopher Nolan");
    movie->set_genre("Sci-Fi");
    movie->set_year(2014);
    return Status::OK;
}

};

void RunServer(const std::string& server_address) {
    MovieSearchServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[B] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./B_server <listen_address>" << std::endl;
        return 1;
    }

    std::string b_addr = argv[1]; // e.g., 0.0.0.0:5002
    RunServer(b_addr);
    return 0;
}