#!/bin/bash
# test_configurations.sh

# Base directories and files
BUILD_DIR="./build"
DATA_DIR="./data"
CSV_FILE="$DATA_DIR/A_data.csv"
RESULTS_DIR="./performance_results"
mkdir -p $RESULTS_DIR

# Server addresses
A_ADDR="127.0.0.1:50001"
B_ADDR="127.0.0.1:50002"
C_ADDR="127.0.0.1:50003"
D_ADDR="127.0.0.1:50004"
E_ADDR="127.0.0.1:50005"

# Kill any running servers
function cleanup {
    echo "Cleaning up servers..."
    pkill -f A_server
    pkill -f B_server
    pkill -f C_server
    pkill -f D_server
    pkill -f E_server
    sleep 2
}

# Start the overlay network
function start_overlay {
    echo "Starting servers B, C, D, E..."
    $BUILD_DIR/E_server $E_ADDR $CSV_FILE > /dev/null 2>&1 &
    sleep 1
    $BUILD_DIR/D_server $D_ADDR $E_ADDR $CSV_FILE > /dev/null 2>&1 &
    $BUILD_DIR/C_server $C_ADDR $E_ADDR $CSV_FILE > /dev/null 2>&1 &
    sleep 1
    $BUILD_DIR/B_server $B_ADDR $C_ADDR $D_ADDR $CSV_FILE > /dev/null 2>&1 &
    sleep 1
}

# Test scenario 1: No caching
function test_no_cache {
    echo "🧪 Testing configuration: No Caching"
    cleanup
    start_overlay

    # Start A with very small cache size (effectively disabled)
    $BUILD_DIR/A_server $A_ADDR $B_ADDR $CSV_FILE 1 1 > /dev/null 2>&1 &
    sleep 2

    # Run performance test
    $BUILD_DIR/performance_test $A_ADDR $RESULTS_DIR/no_cache_results.csv

    cleanup
}

# Test scenario 2: In-memory cache only
function test_memory_cache {
    echo "🧪 Testing configuration: In-Memory Cache Only"
    cleanup
    start_overlay

    # Start A with in-memory cache but effectively disabled shared memory
    $BUILD_DIR/A_server $A_ADDR $B_ADDR $CSV_FILE 300 100 > /dev/null 2>&1 &
    sleep 2

    # Run performance test
    $BUILD_DIR/performance_test $A_ADDR $RESULTS_DIR/memory_cache_results.csv

    cleanup
}

# Test scenario 3: Both caches
function test_both_caches {
    echo "🧪 Testing configuration: Both Caches"
    cleanup
    start_overlay

    # Start A with both caches
    $BUILD_DIR/A_server $A_ADDR $B_ADDR $CSV_FILE 300 100 > /dev/null 2>&1 &
    sleep 2

    # Run performance test
    $BUILD_DIR/performance_test $A_ADDR $RESULTS_DIR/both_caches_results.csv

    cleanup
}

# Test scenario 4: Multi-server with both caches
function test_multi_server {
    echo "🧪 Testing configuration: Multi-Server With Both Caches"
    cleanup
    start_overlay

    # Start two A servers with both caches
    A2_ADDR="127.0.0.1:50010"
    $BUILD_DIR/A_server $A_ADDR $B_ADDR $CSV_FILE 300 100 > /dev/null 2>&1 &
    $BUILD_DIR/A_server $A2_ADDR $B_ADDR $CSV_FILE 300 100 > /dev/null 2>&1 &
    sleep 2

    # Run performance test against first server
    echo "Testing first server ($A_ADDR)..."
    $BUILD_DIR/performance_test $A_ADDR $RESULTS_DIR/multi_server_1_results.csv

    # Run performance test against second server
    echo "Testing second server ($A2_ADDR)..."
    $BUILD_DIR/performance_test $A2_ADDR $RESULTS_DIR/multi_server_2_results.csv

    cleanup
}

# Run all tests
echo "Starting performance tests..."
test_no_cache
test_memory_cache
test_both_caches
test_multi_server

echo "All tests completed! Results in $RESULTS_DIR/"