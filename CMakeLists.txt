        cmake_minimum_required(VERSION 3.15)
        project(MovieSearch)

        set(CMAKE_CXX_STANDARD 17)
        set(Protobuf_PROTOC_EXECUTABLE /opt/homebrew/bin/protoc)

        find_package(gRPC REQUIRED)
        find_package(Protobuf REQUIRED)
        find_package(Threads REQUIRED)

        include_directories(
        ${Protobuf_INCLUDE_DIRS}
        ${gRPC_INCLUDE_DIRS}
        generated/cpp
        generated/cpp/proto
        .
        )

        # Determine the platform
        if(UNIX AND NOT APPLE)
                set(LINUX TRUE)
        endif()

        

        # Common files
        set(COMMON_SOURCES
        generated/cpp/proto/movie.pb.cc
        generated/cpp/proto/movie.grpc.pb.cc
        generated/cpp/proto/movie.grpc.pb.h
        )

        # Header-only libraries
        set(HEADERS
        server/movie_struct.h
        server/cache.h
        server/posix_shared_memory.h
        server/response_serializer.h
        )

        # Generate proto files
        add_custom_command(
        OUTPUT
                ${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp/proto/movie.pb.cc
                ${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp/proto/movie.pb.h
                ${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp/proto/movie.grpc.pb.cc
                ${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp/proto/movie.grpc.pb.h
        COMMAND
                ${Protobuf_PROTOC_EXECUTABLE}
                --grpc_out=${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp
                --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp
                -I${CMAKE_CURRENT_SOURCE_DIR}
                --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
                ${CMAKE_CURRENT_SOURCE_DIR}/proto/movie.proto
        DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/proto/movie.proto
        )

        # Client executable
        add_executable(client
        client/client.cpp
        ${COMMON_SOURCES}
        )

        target_link_libraries(client
        gRPC::grpc++
        protobuf::libprotobuf
        )

        # Server A executable
        add_executable(A_server
        server/A_server.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
                server/ab_communication.h
                server/ab_communication.cpp
        )

        target_link_libraries(A_server
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Server B executable
        add_executable(B_server
        server/B_server.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
        )

        target_link_libraries(B_server
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Server C executable
        add_executable(C_server
        server/C_server.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
        )

        target_link_libraries(C_server
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Server D executable
        add_executable(D_server
        server/D_server.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
        )

        target_link_libraries(D_server
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Server E executable
        add_executable(E_server
        server/E_server.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
        )

        target_link_libraries(E_server
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Unit test executable
        add_executable(test_movie_search
        scripts/test_movie_search.cpp
        ${HEADERS}
        )

        # Cache and shared memory test
        add_executable(test_cache_shm
        scripts/test_cache_shm.cpp
        ${COMMON_SOURCES}
        ${HEADERS}
        )

        target_link_libraries(test_cache_shm
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
        )

        # Make the test runnable with 'make test'
        # enable_testing()
        # add_test(NAME movie_search_test COMMAND test_movie_search)


        # Performance test executable
        add_executable(performance_test
                scripts/performance_test.cpp
                ${COMMON_SOURCES}
        )

        target_link_libraries(performance_test
                gRPC::grpc++
                protobuf::libprotobuf
                Threads::Threads
        )