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

// ---------- C as gRPC Client to E ----------
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
            std::cerr << "[C → E] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- C as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& e_address)
        : e_client_(grpc::CreateChannel(e_address, grpc::InsecureChannelCredentials())) {}

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[C] Received query: " << query << std::endl;

        // Server C's local movie data
        if (query.find("comedy") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("Superbad");
            movie->set_director("Greg Mottola");
            movie->set_genre("Comedy");
            movie->set_year(2007);
        }

        // Forward request to E
        std::cout << "[C] Forwarding to Process E..." << std::endl;
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
    std::cout << "[C] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./C_server <listen_address> <E_address>" << std::endl;
        return 1;
    }

    std::string c_addr = argv[1]; // e.g., 0.0.0.0:5003
    std::string e_addr = argv[2]; // e.g., 192.168.0.5:5005
    
    RunServer(c_addr, e_addr);
    return 0;
}