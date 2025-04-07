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

class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "Received query: " << query << std::endl;

        // TODO: Check cache first (stubbed for now)
        bool inCache = false;
        if (inCache) {
            std::cout << "Cache hit (stubbed)" << std::endl;
        } else {
            std::cout << "Cache miss (stubbed). Forwarding to Process B..." << std::endl;

            // TODO: Forward request to B (gRPC client call)
            // Simulating a response from B for now:
            MovieInfo* movie = response->add_results();
            movie->set_title("Inception");
            movie->set_director("Christopher Nolan");
            movie->set_genre("Sci-Fi");
            movie->set_year(2010);
        }

        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:5001");
    MovieSearchServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Process A server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
