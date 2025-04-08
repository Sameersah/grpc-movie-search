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
    MovieSearchServiceImpl(const std::string& b_address)
        : b_client_(grpc::CreateChannel(b_address, grpc::InsecureChannelCredentials())) {}

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[A] Received query: " << query << std::endl;

        std::cout << "[A] Forwarding to Process B..." << std::endl;
        SearchResponse b_response = b_client_.Search(query);

        for (const auto& movie : b_response.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    BClient b_client_;
};

void RunServer(const std::string& b_address) {
    std::string server_address("0.0.0.0:5001");
    MovieSearchServiceImpl service(b_address);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[A] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./A_server <B_address>" << std::endl;
        return 1;
    }

    std::string b_address = argv[1]; // e.g., 192.168.0.3:5002
    RunServer(b_address);
    return 0;
}