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

// ---------- E as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[E] Received query: " << query << std::endl;

        // Server E's local movie data
        if (query.find("action") != std::string::npos || query.find("Action") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("The Dark Knight");
            movie->set_director("Christopher Nolan");
            movie->set_genre("Action/Thriller");
            movie->set_year(2008);
        } else if (query.find("horror") != std::string::npos || query.find("Horror") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("The Shining");
            movie->set_director("Stanley Kubrick");
            movie->set_genre("Horror");
            movie->set_year(1980);
        } else if (query.find("animation") != std::string::npos || query.find("Animation") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("Spirited Away");
            movie->set_director("Hayao Miyazaki");
            movie->set_genre("Animation");
            movie->set_year(2001);
        }

        return Status::OK;
    }
};

void RunServer(const std::string& server_address) {
    MovieSearchServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[E] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./E_server <listen_address>" << std::endl;
        return 1;
    }

    std::string e_addr = argv[1]; // e.g., 0.0.0.0:5005
    RunServer(e_addr);
    return 0;
}