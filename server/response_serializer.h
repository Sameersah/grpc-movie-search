#ifndef RESPONSE_SERIALIZER_H
#define RESPONSE_SERIALIZER_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "movie.grpc.pb.h"

/**
 * Utility class for serializing and deserializing SearchResponse objects.
 * This is needed for storing the responses in shared memory.
 */
class ResponseSerializer {
public:
    /**
     * Serialize a SearchResponse to a byte vector
     * @param response The response to serialize
     * @return Serialized binary data
     */
    static std::vector<uint8_t> serialize(const movie::SearchResponse& response) {
        // Get the size needed for serialization
        size_t serializedSize = response.ByteSizeLong();
        
        // Create buffer with enough space
        std::vector<uint8_t> buffer(serializedSize);
        
        // Serialize to the buffer
        if (!response.SerializeToArray(buffer.data(), static_cast<int>(buffer.size()))) {
            throw std::runtime_error("Failed to serialize response");
        }
        
        return buffer;
    }
    
    /**
     * Deserialize a byte vector back to a SearchResponse
     * @param data The serialized data
     * @param response The response object to populate
     * @return Whether deserialization was successful
     */
    static bool deserialize(const std::vector<uint8_t>& data, movie::SearchResponse& response) {
        return response.ParseFromArray(data.data(), static_cast<int>(data.size()));
    }
};

#endif // RESPONSE_SERIALIZER_H