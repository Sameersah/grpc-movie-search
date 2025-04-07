#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "generated/cpp/movie.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using movie::MovieSearch;
using movie::SearchRequest;
using movie::SearchResponse;
using movie::MovieInfo;

// ----- B as Client -----
class DownstreamClient {
public:
    DownstreamClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "Failed to call downstream: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ----- B as Server -----
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(std::string c_addr, std::string d_addr)
        : client_c_(grpc::CreateChannel(c_addr, grpc::InsecureChannelCredentials())),
          client_d_(grpc::CreateChannel(d_addr, grpc::InsecureChannelCredentials())) {}

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string title = request->title();
        std::cout << "[B] Received query: " << title << std::endl;

        // Forward to C and D
        SearchResponse resp_c = client_c_.Search(title);
        SearchResponse resp_d = client_d_.Search(title);

        // Merge results
        for (const auto& movie : resp_c.results()) {
            *response->add_results() = movie;
        }
        for (const auto& movie : resp_d.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    DownstreamClient client_c_;
    DownstreamClient client_d_;
};

// ----- Server Setup -----
void RunServer(const std::string& server_address,
               const std::string& c_address,
               const std::string& d_address) {
    MovieSearchServiceImpl service(c_address, d_address);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[B] Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: ./B_server <listen_address> <C_address> <D_address>" << std::endl;
        return 1;
    }

    std::string b_addr = argv[1]; // e.g., 0.0.0.0:50052
    std::string c_addr = argv[2]; // e.g., 192.168.1.103:50053
    std::string d_addr = argv[3]; // e.g., 192.168.1.103:50054

    RunServer(b_addr, c_addr, d_addr);
    return 0;
}
