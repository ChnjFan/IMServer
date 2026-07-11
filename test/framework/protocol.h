#ifndef IMSERVER_PROTOCOL_H
#define IMSERVER_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>
#include <json/json.h>

#pragma pack(push, 1)
struct FrameHeader {
    uint16_t msgId;
    uint16_t bodyLen;
};
#pragma pack(pop)

struct DecodedFrame {
    uint16_t msgId;
    Json::Value body;
};

// Encode msgId + JSON body into a binary frame (suitable for direct send).
// Uses big-endian (network byte order) to match ChatServer's protocol.
std::string encode(uint16_t msgId, const Json::Value& body);

// Decode binary buffer into complete frames, handling split/sticky packets.
// Incomplete data is kept internally (static buffer) for the next call.
// Uses big-endian to match ChatServer's protocol.
std::vector<DecodedFrame> decode(const char* buf, size_t len);

// Test helper: clear the internal reassembly buffer
void protocol_reset_buffer();

#endif // IMSERVER_PROTOCOL_H
