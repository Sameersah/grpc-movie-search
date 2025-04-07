cmake_minimum_required(VERSION 3.15)
project(MovieSearch)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

include_directories(
        ${Protobuf_INCLUDE_DIRS}
        ${gRPC_INCLUDE_DIRS}
        generated/cpp
)

add_executable(A_server
        server/A_server.cpp
        generated/cpp/movie.pb.cc
        generated/cpp/movie.grpc.pb.cc
        server/B_server.cpp
)

target_link_libraries(A_server
        gRPC::grpc++
        protobuf::libprotobuf
)


add_executable(B_server
        server/B_server.cpp
        generated/cpp/movie.pb.cc
        generated/cpp/movie.grpc.pb.cc
)

target_link_libraries(B_server
        gRPC::grpc++
        protobuf::libprotobuf
)