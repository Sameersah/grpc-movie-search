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

// ---------- B as gRPC Client to C ----------
class CClient {
public:
    CClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[B → C] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- B as gRPC Client to D ----------
class DClient {
public:
    DClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    SearchResponse Search(const std::string& title) {
        SearchRequest request;
        request.set_title(title);
        SearchResponse response;
        ClientContext context;

        Status status = stub_->Search(&context, request, &response);
        if (!status.ok()) {
            std::cerr << "[B → D] gRPC call failed: " << status.error_message() << std::endl;
        }

        return response;
    }

private:
    std::unique_ptr<MovieSearch::Stub> stub_;
};

// ---------- B as gRPC Server ----------
class MovieSearchServiceImpl final : public MovieSearch::Service {
public:
    MovieSearchServiceImpl(const std::string& c_address, const std::string& d_address)
        : c_client_(grpc::CreateChannel(c_address, grpc::InsecureChannelCredentials())),
          d_client_(grpc::CreateChannel(d_address, grpc::InsecureChannelCredentials())) {}

    Status Search(ServerContext* context, const SearchRequest* request,
                  SearchResponse* response) override {
        std::string query = request->title();
        std::cout << "[B] Received query: " << query << std::endl;

        // B's local data
        if (query.find("Interstellar") != std::string::npos) {
            MovieInfo* movie = response->add_results();
            movie->set_title("Interstellar");
            movie->set_director("Christopher Nolan");
            movie->set_genre("Sci-Fi");
            movie->set_year(2014);
        }

        // Forward request to C
        std::cout << "[B] Forwarding to Process C..." << std::endl;
        SearchResponse c_response = c_client_.Search(query);

        // Forward request to D
        std::cout << "[B] Forwarding to Process D..." << std::endl;
        SearchResponse d_response = d_client_.Search(query);

        // Gather results from C
        for (const auto& movie : c_response.results()) {
            *response->add_results() = movie;
        }

        // Gather results from D
        for (const auto& movie : d_response.results()) {
            *response->add_results() = movie;
        }

        return Status::OK;
    }

private:
    CClient c_client_;
    DClient d_client_;
};

void RunServer(const std::string& server_address, const std::string& c_address, const std::string& d_address) {
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

    std::string b_addr = argv[1]; // e.g., 0.0.0.0:5002
    std::string c_addr = argv[2]; // e.g., 192.168.0.4:5003
    std::string d_addr = argv[3]; // e.g., 192.168.0.4:5004
    
    RunServer(b_addr, c_addr, d_addr);
    return 0;
}