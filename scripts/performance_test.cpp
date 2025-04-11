// performance_test.cpp
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "movie.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using movie::MovieSearch;
using movie::SearchRequest;
using movie::SearchResponse;

// Simple timer class for performance measurements
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::string operation_name;

public:
    Timer(const std::string& name) : operation_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
    }

    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "⏱️ " << operation_name << " took " << duration << " ms" << std::endl;
    }

    double elapsed_ms() {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    }
};

// Client class for performance testing
class PerformanceClient {
private:
    std::unique_ptr<MovieSearch::Stub> stub_;

public:
    struct QueryResult {
        std::string query;
        double duration_ms;
        int result_count;
        bool success;
        std::string communication_type; // "gRPC" or "SharedMemory" (inferred from server response)
    };

    PerformanceClient(std::shared_ptr<Channel> channel)
        : stub_(MovieSearch::NewStub(channel)) {}

    QueryResult search(const std::string& query) {
        SearchRequest request;
        request.set_title(query);
        SearchResponse response;
        ClientContext context;
        QueryResult result;
        result.query = query;

        Timer timer("Search for '" + query + "'");
        Status status = stub_->Search(&context, request, &response);
        result.duration_ms = timer.elapsed_ms();

        if (status.ok()) {
            result.success = true;
            result.result_count = response.results_size();

            // Try to guess if shared memory was used based on response time
            // This is a heuristic - in reality we can't know for sure from the client
            if (result.duration_ms < 5 && query != "__ping__") {
                result.communication_type = "Likely Cache or SharedMemory";
            } else {
                result.communication_type = "Likely gRPC";
            }
        } else {
            result.success = false;
            result.result_count = 0;
            result.communication_type = "Error";
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
        }

        return result;
    }

    std::vector<QueryResult> run_test_suite(const std::vector<std::string>& queries, int repetitions) {
        std::vector<QueryResult> results;

        // For each query in the test suite
        for (const auto& query : queries) {
            for (int i = 0; i < repetitions; i++) {
                results.push_back(search(query));
            }
        }

        return results;
    }

    void print_report(const std::vector<QueryResult>& results) {
        std::cout << "\n====== Performance Test Report ======\n" << std::endl;

        // Group by query
        std::unordered_map<std::string, std::vector<double>> query_times;
        std::unordered_map<std::string, int> query_results;
        std::unordered_map<std::string, std::string> comm_types;

        for (const auto& result : results) {
            if (result.success) {
                query_times[result.query].push_back(result.duration_ms);
                query_results[result.query] = result.result_count;
                comm_types[result.query] = result.communication_type;
            }
        }

        // Print results for each query
        std::cout << std::left
                  << std::setw(20) << "Query"
                  << std::setw(10) << "Avg (ms)"
                  << std::setw(10) << "Min (ms)"
                  << std::setw(10) << "Max (ms)"
                  << std::setw(10) << "Results"
                  << std::setw(25) << "Comm Type"
                  << std::setw(10) << "Runs" << std::endl;
        std::cout << std::string(85, '-') << std::endl;

        for (const auto& pair : query_times) {
            const auto& query = pair.first;
            const auto& times = pair.second;

            double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
            double min = *std::min_element(times.begin(), times.end());
            double max = *std::max_element(times.begin(), times.end());

            std::cout << std::left
                      << std::setw(20) << query
                      << std::setw(10) << std::fixed << std::setprecision(2) << avg
                      << std::setw(10) << min
                      << std::setw(10) << max
                      << std::setw(10) << query_results[query]
                      << std::setw(25) << comm_types[query]
                      << std::setw(10) << times.size() << std::endl;
        }

        std::cout << "\n===================================" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_address> [output_csv]" << std::endl;
        return 1;
    }

    std::string server_address = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "performance_results.csv";

    // Connect to server
    PerformanceClient client(
        grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

    // Define test queries - using actual movie titles for more realistic tests
    std::vector<std::string> test_queries = {
        "inception",
        "interstellar",
        "dark knight",
        "shawshank",
        "matrix",
        "sci-fi",     // Genre search
        "comedy",     // Genre search
        "action",     // Genre search
        "spielberg",  // Director search
        "kubrick"     // Director search
    };

    // Run tests
    std::cout << "Running performance tests against " << server_address << "..." << std::endl;

    // First run: Cold cache (first request for each query)
    std::cout << "\n=== Cold Cache Test ===" << std::endl;
    auto cold_results = client.run_test_suite(test_queries, 1);

    // Wait a moment to ensure server processing is complete
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Second run: Warm cache (should hit cache for most queries)
    std::cout << "\n=== Warm Cache Test ===" << std::endl;
    auto warm_results = client.run_test_suite(test_queries, 3);

    // Combined results for reporting
    std::vector<PerformanceClient::QueryResult> all_results;
    all_results.insert(all_results.end(), cold_results.begin(), cold_results.end());
    all_results.insert(all_results.end(), warm_results.begin(), warm_results.end());

    // Generate report
    client.print_report(all_results);

    // Write results to CSV for further analysis
    std::ofstream csv_file(output_file);
    if (csv_file.is_open()) {
        csv_file << "Query,Duration(ms),ResultCount,Success,RunType,CommunicationType\n";

        for (const auto& result : cold_results) {
            csv_file << result.query << ","
                     << result.duration_ms << ","
                     << result.result_count << ","
                     << (result.success ? "true" : "false") << ","
                     << "cold" << ","
                     << result.communication_type << "\n";
        }

        for (const auto& result : warm_results) {
            csv_file << result.query << ","
                     << result.duration_ms << ","
                     << result.result_count << ","
                     << (result.success ? "true" : "false") << ","
                     << "warm" << ","
                     << result.communication_type << "\n";
        }

        csv_file.close();
        std::cout << "\nResults written to " << output_file << std::endl;
    } else {
        std::cerr << "Failed to open output file" << std::endl;
    }

    return 0;
}