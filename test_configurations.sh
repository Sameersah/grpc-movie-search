#!/bin/bash
# test_configurations.sh - Test different communication configurations

# Bold and colors for better readability
BOLD=$(tput bold)
GREEN=$(tput setaf 2)
RED=$(tput setaf 1)
YELLOW=$(tput setaf 3)
BLUE=$(tput setaf 4)
RESET=$(tput sgr0)

# Get the start time of the script
START_TIME=$(date +%s)

# Log function with timestamps
log() {
    local level=$1
    local message=$2
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")

    case $level in
        "INFO")
            echo "${BLUE}${BOLD}[${timestamp}] [INFO]${RESET} $message"
            ;;
        "SUCCESS")
            echo "${GREEN}${BOLD}[${timestamp}] [SUCCESS]${RESET} $message"
            ;;
        "WARNING")
            echo "${YELLOW}${BOLD}[${timestamp}] [WARNING]${RESET} $message"
            ;;
        "ERROR")
            echo "${RED}${BOLD}[${timestamp}] [ERROR]${RESET} $message"
            ;;
        *)
            echo "[${timestamp}] $message"
            ;;
    esac
}

# Base directories and files
BUILD_DIR="./build"
DATA_DIR="./data"
RESULTS_DIR="./performance_results"
LOG_DIR="./logs"

# Create necessary directories
mkdir -p $RESULTS_DIR
mkdir -p $LOG_DIR

log "INFO" "Created results directory: $RESULTS_DIR"
log "INFO" "Created logs directory: $LOG_DIR"

# Server addresses - Single node configuration
SINGLE_A_ADDR="127.0.0.1:50001"
SINGLE_B_ADDR="127.0.0.1:50002"
SINGLE_C_ADDR="127.0.0.1:50003"
SINGLE_D_ADDR="127.0.0.1:50004"
SINGLE_E_ADDR="127.0.0.1:50005"

# Server addresses - Multi node configuration
# Update these with your actual machine IP addresses
MULTI_A_ADDR="127.0.0.1:50001"       # Local machine
MULTI_B_ADDR="169.254.54.50:50002"   # Remote machine 1
MULTI_C_ADDR="169.254.54.50:50003"   # Remote machine 2
MULTI_D_ADDR="169.254.54.50:50004"   # Remote machine 2
MULTI_E_ADDR="169.254.54.50:50005"   # Remote machine 3

# Local B address (to force shared memory)
LOCAL_B_ADDR="localhost:50002"

# Data files for each server
A_DATA="$DATA_DIR/A_data.csv"
B_DATA="$DATA_DIR/B_data.csv"
C_DATA="$DATA_DIR/C_data.csv"
D_DATA="$DATA_DIR/D_data.csv"
E_DATA="$DATA_DIR/E_data.csv"

# Maximum wait time for server startup in seconds
MAX_WAIT_TIME=30

# Function to check if a server is listening on a specific port
wait_for_server() {
    local host=$1
    local port=$2
    local server_name=$3
    local max_attempts=$4
    local attempt=1
    local log_file=$5

    log "INFO" "Waiting for $server_name to be ready on $host:$port (max $max_attempts attempts)..."

    while [ $attempt -le $max_attempts ]; do
        # Check if the server process is still running
        if ! pgrep -f "$server_name" > /dev/null; then
            log "ERROR" "$server_name process is not running! Check $log_file for details."
            return 1
        fi

        # Use netcat to check if port is open
        if nc -z $host $port 2>/dev/null; then
            log "SUCCESS" "$server_name is ready on $host:$port (after $attempt attempts)"
            return 0
        fi

        log "INFO" "Attempt $attempt/$max_attempts: $server_name not ready yet. Waiting..."
        sleep 1
        attempt=$((attempt + 1))
    done

    log "ERROR" "$server_name failed to start after $max_attempts attempts. Check $log_file for details."
    return 1
}

# Kill any running servers on local machine
function cleanup_local {
    log "INFO" "Cleaning up local servers..."

    for server in A_server B_server C_server D_server E_server; do
        if pgrep -f $server > /dev/null; then
            log "INFO" "Stopping $server process..."
            pkill -f $server
        fi
    done

    # Wait to ensure all processes are terminated
    sleep 2

    # Check if any servers are still running
    for server in A_server B_server C_server D_server E_server; do
        if pgrep -f $server > /dev/null; then
            log "WARNING" "Process $server is still running, forcing termination..."
            pkill -9 -f $server
        fi
    done

    # Clean up shared memory
    log "INFO" "Cleaning up shared memory segments..."
    if [[ "$(uname)" == "Darwin" ]]; then
        rm -f /tmp/movie_*
        log "INFO" "Removed shared memory on macOS (/tmp/movie_*)"
    else
        rm -f /dev/shm/movie_*
        log "INFO" "Removed shared memory on Linux (/dev/shm/movie_*)"
    fi
}

# Start all servers on single node
function start_overlay_single_node {
    log "INFO" "Starting all servers on local machine..."

    # Start Server E (Leaf Node)
    local e_log="$LOG_DIR/E_server_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server E on $SINGLE_E_ADDR (log: $e_log)..."
    $BUILD_DIR/E_server $SINGLE_E_ADDR $E_DATA > $e_log 2>&1 &
    E_PID=$!

    # Wait for E to be ready
    if ! wait_for_server $(echo $SINGLE_E_ADDR | tr ':' ' ') "E_server" $MAX_WAIT_TIME $e_log; then
        log "ERROR" "Failed to start Server E. Aborting test."
        return 1
    fi

    # Start Server D
    local d_log="$LOG_DIR/D_server_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server D on $SINGLE_D_ADDR (log: $d_log)..."
    $BUILD_DIR/D_server $SINGLE_D_ADDR $SINGLE_E_ADDR $D_DATA > $d_log 2>&1 &
    D_PID=$!

    # Start Server C
    local c_log="$LOG_DIR/C_server_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server C on $SINGLE_C_ADDR (log: $c_log)..."
    $BUILD_DIR/C_server $SINGLE_C_ADDR $SINGLE_E_ADDR $C_DATA > $c_log 2>&1 &
    C_PID=$!

    # Wait for C and D to be ready
    if ! wait_for_server $(echo $SINGLE_C_ADDR | tr ':' ' ') "C_server" $MAX_WAIT_TIME $c_log; then
        log "ERROR" "Failed to start Server C. Aborting test."
        return 1
    fi

    if ! wait_for_server $(echo $SINGLE_D_ADDR | tr ':' ' ') "D_server" $MAX_WAIT_TIME $d_log; then
        log "ERROR" "Failed to start Server D. Aborting test."
        return 1
    fi

    # Start Server B
    local b_log="$LOG_DIR/B_server_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server B on $SINGLE_B_ADDR (log: $b_log)..."
    $BUILD_DIR/B_server $SINGLE_B_ADDR $SINGLE_C_ADDR $SINGLE_D_ADDR $B_DATA > $b_log 2>&1 &
    B_PID=$!

    # Wait for B to be ready
    if ! wait_for_server $(echo $SINGLE_B_ADDR | tr ':' ' ') "B_server" $MAX_WAIT_TIME $b_log; then
        log "ERROR" "Failed to start Server B. Aborting test."
        return 1
    fi

    log "SUCCESS" "All overlay servers started successfully!"
    return 0
}

# Instructions for multi-node setup
function print_multi_node_instructions {
    log "INFO" "${YELLOW}${BOLD}[MULTI-NODE SETUP INSTRUCTIONS]${RESET}"
    echo "For multi-node testing, you need to run the following commands on the respective machines:"
    echo
    echo "On Machine 3 (E server):"
    echo "  ./build/E_server $MULTI_E_ADDR $E_DATA"
    echo
    echo "On Machine 2 (C and D servers):"
    echo "  ./build/C_server $MULTI_C_ADDR $MULTI_E_ADDR $C_DATA"
    echo "  ./build/D_server $MULTI_D_ADDR $MULTI_E_ADDR $D_DATA"
    echo
    echo "On Machine 1 (B server):"
    echo "  ./build/B_server $MULTI_B_ADDR $MULTI_C_ADDR $MULTI_D_ADDR $B_DATA"
    echo
    echo "Make sure these servers are running before continuing."
    echo "Press Enter when the remote servers are ready or Ctrl+C to cancel..."
    read
}

# Verify if servers are accessible
function verify_remote_servers {
    log "INFO" "Verifying connectivity to remote servers..."

    # Check if B server is reachable
    local b_host=$(echo $MULTI_B_ADDR | cut -d: -f1)
    local b_port=$(echo $MULTI_B_ADDR | cut -d: -f2)

    log "INFO" "Checking B server at $b_host:$b_port..."
    if nc -z $b_host $b_port 2>/dev/null; then
        log "SUCCESS" "B server is reachable"
    else
        log "ERROR" "Cannot reach B server at $b_host:$b_port"
        return 1
    fi

    # Check if C server is reachable
    local c_host=$(echo $MULTI_C_ADDR | cut -d: -f1)
    local c_port=$(echo $MULTI_C_ADDR | cut -d: -f2)

    log "INFO" "Checking C server at $c_host:$c_port..."
    if nc -z $c_host $c_port 2>/dev/null; then
        log "SUCCESS" "C server is reachable"
    else
        log "ERROR" "Cannot reach C server at $c_host:$c_port"
        return 1
    fi

    # Check if D server is reachable
    local d_host=$(echo $MULTI_D_ADDR | cut -d: -f1)
    local d_port=$(echo $MULTI_D_ADDR | cut -d: -f2)

    log "INFO" "Checking D server at $d_host:$d_port..."
    if nc -z $d_host $d_port 2>/dev/null; then
        log "SUCCESS" "D server is reachable"
    else
        log "ERROR" "Cannot reach D server at $d_host:$d_port"
        return 1
    fi

    # Check if E server is reachable
    local e_host=$(echo $MULTI_E_ADDR | cut -d: -f1)
    local e_port=$(echo $MULTI_E_ADDR | cut -d: -f2)

    log "INFO" "Checking E server at $e_host:$e_port..."
    if nc -z $e_host $e_port 2>/dev/null; then
        log "SUCCESS" "E server is reachable"
    else
        log "ERROR" "Cannot reach E server at $e_host:$e_port"
        return 1
    fi

    log "SUCCESS" "All remote servers are reachable!"
    return 0
}

# Run performance test with proper logging
function run_performance_test {
    local a_addr=$1
    local output_file=$2
    local test_name=$3

    log "INFO" "Running performance test for $test_name..."
    log "INFO" "This may take a minute, please be patient..."

    # Run the test with its output going to a log file
    local perf_log="$LOG_DIR/performance_test_$(date +%Y%m%d_%H%M%S).log"
    $BUILD_DIR/performance_test $a_addr $output_file > $perf_log 2>&1

    if [ $? -eq 0 ]; then
        log "SUCCESS" "Performance test completed successfully. Results saved to $output_file"
        log "INFO" "Test log saved to $perf_log"
    else
        log "ERROR" "Performance test failed. Check log at $perf_log"
        return 1
    fi

    return 0
}

# Test 1: Single Node - Shared Memory communication with cache
function test_single_node_shm_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 1]${RESET} Single Node - Shared Memory Communication with Cache"
    cleanup_local

    if ! start_overlay_single_node; then
        log "ERROR" "Failed to start overlay network. Skipping this test."
        return 1
    fi

    # Start A with cache and local B address (shared memory)
    local a_log="$LOG_DIR/A_server_shm_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with shared memory communication to B (log: $a_log)..."
    $BUILD_DIR/A_server $SINGLE_A_ADDR $LOCAL_B_ADDR $A_DATA 300 100 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $SINGLE_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/single_node_shm_cache_results.csv"
    if ! run_performance_test $SINGLE_A_ADDR $output_file "Single Node - Shared Memory with Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 1 completed successfully!"
    return 0
}

# Test 2: Single Node - gRPC communication with cache
function test_single_node_grpc_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 2]${RESET} Single Node - gRPC Communication with Cache"
    cleanup_local

    if ! start_overlay_single_node; then
        log "ERROR" "Failed to start overlay network. Skipping this test."
        return 1
    fi

    # Start A with cache and B address using IP address (gRPC)
    local a_log="$LOG_DIR/A_server_grpc_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with gRPC communication to B (log: $a_log)..."
    $BUILD_DIR/A_server $SINGLE_A_ADDR $SINGLE_B_ADDR $A_DATA 300 100 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $SINGLE_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/single_node_grpc_cache_results.csv"
    if ! run_performance_test $SINGLE_A_ADDR $output_file "Single Node - gRPC with Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 2 completed successfully!"
    return 0
}

# Test 3: Single Node - Shared Memory communication without cache
function test_single_node_shm_no_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 3]${RESET} Single Node - Shared Memory Communication without Cache"
    cleanup_local

    if ! start_overlay_single_node; then
        log "ERROR" "Failed to start overlay network. Skipping this test."
        return 1
    fi

    # Start A without cache and local B address (shared memory)
    local a_log="$LOG_DIR/A_server_shm_no_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with shared memory communication to B (log: $a_log)..."
    $BUILD_DIR/A_server $SINGLE_A_ADDR $LOCAL_B_ADDR $A_DATA 1 1 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $SINGLE_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/single_node_shm_no_cache_results.csv"
    if ! run_performance_test $SINGLE_A_ADDR $output_file "Single Node - Shared Memory without Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 3 completed successfully!"
    return 0
}

# Test 4: Single Node - gRPC communication without cache
function test_single_node_grpc_no_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 4]${RESET} Single Node - gRPC Communication without Cache"
    cleanup_local

    if ! start_overlay_single_node; then
        log "ERROR" "Failed to start overlay network. Skipping this test."
        return 1
    fi

    # Start A without cache and B address using IP address (gRPC)
    local a_log="$LOG_DIR/A_server_grpc_no_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with gRPC communication to B (log: $a_log)..."
    $BUILD_DIR/A_server $SINGLE_A_ADDR $SINGLE_B_ADDR $A_DATA 1 1 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $SINGLE_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/single_node_grpc_no_cache_results.csv"
    if ! run_performance_test $SINGLE_A_ADDR $output_file "Single Node - gRPC without Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 4 completed successfully!"
    return 0
}

# Test 5: Multi Node - gRPC communication with cache
function test_multi_node_grpc_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 5]${RESET} Multi Node - gRPC Communication with Cache"
    cleanup_local

    # Provide instructions for starting remote servers
    print_multi_node_instructions

    # Verify remote servers
    if ! verify_remote_servers; then
        log "ERROR" "Failed to verify remote servers. Aborting test."
        return 1
    fi

    # Start A with cache and remote B address
    local a_log="$LOG_DIR/A_server_multi_node_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with gRPC communication to remote B (log: $a_log)..."
    $BUILD_DIR/A_server $MULTI_A_ADDR $MULTI_B_ADDR $A_DATA 300 100 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $MULTI_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/multi_node_grpc_cache_results.csv"
    if ! run_performance_test $MULTI_A_ADDR $output_file "Multi Node - gRPC with Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 5 completed successfully!"
    return 0
}

# Test 6: Multi Node - gRPC communication without cache
function test_multi_node_grpc_no_cache {
    log "INFO" "${YELLOW}${BOLD}[TEST 6]${RESET} Multi Node - gRPC Communication without Cache"
    cleanup_local

    # Verify remote servers (assume they're still running from previous test)
    if ! verify_remote_servers; then
        log "ERROR" "Failed to verify remote servers. Aborting test."
        return 1
    fi

    # Start A without cache and remote B address
    local a_log="$LOG_DIR/A_server_multi_node_no_cache_$(date +%Y%m%d_%H%M%S).log"
    log "INFO" "Starting Server A with gRPC communication to remote B (log: $a_log)..."
    $BUILD_DIR/A_server $MULTI_A_ADDR $MULTI_B_ADDR $A_DATA 1 1 > $a_log 2>&1 &
    A_PID=$!

    # Wait for A to be ready
    if ! wait_for_server $(echo $MULTI_A_ADDR | tr ':' ' ') "A_server" $MAX_WAIT_TIME $a_log; then
        log "ERROR" "Failed to start Server A. Aborting test."
        return 1
    fi

    # Run performance test
    local output_file="$RESULTS_DIR/multi_node_grpc_no_cache_results.csv"
    if ! run_performance_test $MULTI_A_ADDR $output_file "Multi Node - gRPC without Cache"; then
        log "ERROR" "Performance test failed. Aborting test."
        return 1
    fi

    log "SUCCESS" "Test 6 completed successfully!"
    return 0
}

# Run all tests or selected tests
log "INFO" "${BOLD}Starting performance tests...${RESET}"
log "INFO" "Using MAX_WAIT_TIME=$MAX_WAIT_TIME seconds for server startup"

# Verify tools are available
if ! command -v nc &>/dev/null; then
    log "ERROR" "netcat (nc) is not installed. Please install it for server connectivity checks."
    exit 1
fi

# Store test results
declare -A test_results

# Run single node tests
#test_single_node_shm_cache
test_results["Test 1"]=$?

#test_single_node_grpc_cache
test_results["Test 2"]=$?

#test_single_node_shm_no_cache
test_results["Test 3"]=$?

#test_single_node_grpc_no_cache
test_results["Test 4"]=$?

# Ask user if they want to run multi-node tests
log "INFO" "${YELLOW}${BOLD}Do you want to run multi-node tests?${RESET}"
echo "These tests require setting up servers on multiple machines."
echo "Enter 'y' to continue or any other key to skip: "
read multi_node_choice

if [[ "$multi_node_choice" == "y" || "$multi_node_choice" == "Y" ]]; then
#    test_multi_node_grpc_cache


    test_multi_node_grpc_no_cache
    test_results["Test 6"]=$?
else
    log "INFO" "Skipping multi-node tests."
fi

# Generate analysis if python script exists
if [ -f "analyze_performance.py" ]; then
    log "INFO" "Running performance analysis..."
    python3 analyze_performance.py
    if [ $? -eq 0 ]; then
        log "SUCCESS" "Analysis completed successfully!"
    else
        log "ERROR" "Analysis script failed."
    fi
else
    log "WARNING" "Analysis script (analyze_performance.py) not found. Skipping analysis."
fi

# Final cleanup
cleanup_local




# Calculate and print execution time
END_TIME=$(date +%s)
EXECUTION_TIME=$((END_TIME - START_TIME))
HOURS=$((EXECUTION_TIME / 3600))
MINUTES=$(( (EXECUTION_TIME % 3600) / 60 ))
SECONDS=$((EXECUTION_TIME % 60))

log "INFO" "${BOLD}Total execution time: ${HOURS}h ${MINUTES}m ${SECONDS}s${RESET}"
log "INFO" "${GREEN}${BOLD}All tests completed! ${RESET}Results in $RESULTS_DIR/, logs in $LOG_DIR/"