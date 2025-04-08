#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"

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

// ---------- D as gRPC Client to E ----------
class EClient {
public:
    EClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[D → E] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- D as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& e_address)
        : e_client_(grpc::CreateChannel(e_address, grpc::InsecureChannelCredentials())) {}

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[D] Received query: " << query << std::endl;

        // Server D's local movie data
        if (query.find("drama") != std::string::npos || query.find("Drama") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("The Shawshank Redemption");
            movie->set_director("Frank Darabont");
            movie->set_genre("Drama");
            movie->set_year(1994);
        }

        // Forward request to E
        std::cout << "[D] Forwarding to Process E..." << std::endl;
        SearchResponse e_response = e_client_.Search(query);

        for (const auto& movie : e_response.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    EClient e_client_;
};

void RunServer(const std::string& server_address, const std::string& e_address) {
    MovieSearchServiceImpl service(e_address);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[D] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./D_server <listen_address> <E_address>" << std::endl;
        return 1;
    }

    std::string d_addr = argv[1]; // e.g., 0.0.0.0:5004
    std::string e_addr = argv[2]; // e.g., 192.168.0.5:5005
    
    RunServer(d_addr, e_addr);
    return 0;
}