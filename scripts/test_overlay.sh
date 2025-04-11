#!/bin/bash

BOLD=$(tput bold)
GREEN=$(tput setaf 2)
RED=$(tput setaf 1)
YELLOW=$(tput setaf 3)
RESET=$(tput sgr0)

# Define server addresses
A_IP="127.0.0.1"
B_IP="127.0.0.1"
C_IP="127.0.0.1"
D_IP="127.0.0.1"
E_IP="127.0.0.1"

A_PORT="50001"
B_PORT="50002"
C_PORT="50003"
D_PORT="50004"
E_PORT="50005"

DATA_DIR="../data"
BUILD_DIR="."

# Check if data directory exists
if [ ! -d "$DATA_DIR" ]; then
    echo "${RED}${BOLD}[ERROR]${RESET} Data directory $DATA_DIR not found"
    echo "Create the directory and add your CSV files there"
    exit 1
fi

# Start servers in reverse order: E, D, C, B, A
echo "${BOLD}===== Starting Movie Search Overlay =====${RESET}"

# Start Server E
echo "${YELLOW}${BOLD}[STARTING E]${RESET} on $E_IP:$E_PORT"
$BUILD_DIR/E_server $E_IP:$E_PORT $DATA_DIR/E_data.csv > logs_E.txt 2>&1 &
E_PID=$!
sleep 2
if ps -p $E_PID > /dev/null; then
    echo "${GREEN}${BOLD}[SUCCESS]${RESET} Server E started with PID $E_PID"
else
    echo "${RED}${BOLD}[ERROR]${RESET} Failed to start Server E"
    exit 1
fi

# Start Server D
echo "${YELLOW}${BOLD}[STARTING D]${RESET} on $D_IP:$D_PORT"
$BUILD_DIR/D_server $D_IP:$D_PORT $E_IP:$E_PORT $DATA_DIR/D_data.csv > logs_D.txt 2>&1 &
D_PID=$!
sleep 2
if ps -p $D_PID > /dev/null; then
    echo "${GREEN}${BOLD}[SUCCESS]${RESET} Server D started with PID $D_PID"
else
    echo "${RED}${BOLD}[ERROR]${RESET} Failed to start Server D"
    exit 1
fi

# Start Server C
echo "${YELLOW}${BOLD}[STARTING C]${RESET} on $C_IP:$C_PORT"
$BUILD_DIR/C_server $C_IP:$C_PORT $E_IP:$E_PORT $DATA_DIR/C_data.csv > logs_C.txt 2>&1 &
C_PID=$!
sleep 2
if ps -p $C_PID > /dev/null; then
    echo "${GREEN}${BOLD}[SUCCESS]${RESET} Server C started with PID $C_PID"
else
    echo "${RED}${BOLD}[ERROR]${RESET} Failed to start Server C"
    exit 1
fi

# Start Server B
echo "${YELLOW}${BOLD}[STARTING B]${RESET} on $B_IP:$B_PORT"
$BUILD_DIR/B_server $B_IP:$B_PORT $C_IP:$C_PORT $D_IP:$D_PORT $DATA_DIR/B_data.csv > logs_B.txt 2>&1 &
B_PID=$!
sleep 2
if ps -p $B_PID > /dev/null; then
    echo "${GREEN}${BOLD}[SUCCESS]${RESET} Server B started with PID $B_PID"
else
    echo "${RED}${BOLD}[ERROR]${RESET} Failed to start Server B"
    exit 1
fi

# Start Server A
echo "${YELLOW}${BOLD}[STARTING A]${RESET} on $A_IP:$A_PORT"
$BUILD_DIR/A_server $A_IP:$A_PORT $B_IP:$B_PORT $DATA_DIR/A_data.csv > logs_A.txt 2>&1 &
A_PID=$!
sleep 2
if ps -p $A_PID > /dev/null; then
    echo "${GREEN}${BOLD}[SUCCESS]${RESET} Server A started with PID $A_PID"
else
    echo "${RED}${BOLD}[ERROR]${RESET} Failed to start Server A"
    exit 1
fi

echo "${BOLD}===== All Servers Started Successfully =====${RESET}"
echo
echo "Testing C++ client:"
echo "${YELLOW}${BOLD}[RUNNING]${RESET} C++ client with query 'inception'"
$BUILD_DIR/client $A_IP:$A_PORT <<< "inception"

echo
echo "Testing Python client:"
if command -v python3 &> /dev/null; then
    echo "${YELLOW}${BOLD}[RUNNING]${RESET} Python client with query 'inception'"
    python3 ./python_client.py $A_IP:$A_PORT inception
else
    echo "${RED}${BOLD}[SKIPPED]${RESET} Python not found"
fi

echo
echo "${BOLD}===== All Servers Running =====${RESET}"
echo "To stop all servers, press Ctrl+C"
echo "Log files are saved as logs_A.txt, logs_B.txt, etc."

# Wait for Ctrl+C
trap "echo; echo '${YELLOW}${BOLD}[STOPPING]${RESET} Shutting down all servers...'; \
      kill $A_PID $B_PID $C_PID $D_PID $E_PID 2>/dev/null; \
      echo '${GREEN}${BOLD}[DONE]${RESET} All servers stopped'; exit" INT

# Keep script running until Ctrl+C
while true; do
    sleep 1
done
