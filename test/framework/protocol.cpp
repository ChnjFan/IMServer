#include "protocol.h"
#include <cstring>
#include <sstream>
#include <boost/endian/conversion.hpp>

// Internal buffer for split-packet reassembly
static std::string s_recvBuffer;

std::string encode(uint16_t msgId, const Json::Value& body) {
    Json::StreamWriterBuilder writer;
    std::string bodyStr = Json::writeString(writer, body);

    FrameHeader header;
    // Server uses network byte order (big-endian) — match it
    header.msgId = boost::endian::native_to_big(msgId);
    header.bodyLen = boost::endian::native_to_big(static_cast<uint16_t>(bodyStr.size()));

    std::string frame;
    frame.reserve(sizeof(FrameHeader) + bodyStr.size());
    frame.append(reinterpret_cast<const char*>(&header), sizeof(FrameHeader));
    frame.append(bodyStr);
    return frame;
}

std::vector<DecodedFrame> decode(const char* buf, size_t len) {
    s_recvBuffer.append(buf, len);
    std::vector<DecodedFrame> frames;

    while (s_recvBuffer.size() >= sizeof(FrameHeader)) {
        FrameHeader header;
        std::memcpy(&header, s_recvBuffer.data(), sizeof(FrameHeader));
        header.msgId = boost::endian::big_to_native(header.msgId);
        header.bodyLen = boost::endian::big_to_native(header.bodyLen);

        if (s_recvBuffer.size() < sizeof(FrameHeader) + header.bodyLen) {
            break; // incomplete — wait for more data
        }

        DecodedFrame frame;
        frame.msgId = header.msgId;
        std::string bodyStr = s_recvBuffer.substr(sizeof(FrameHeader), header.bodyLen);

        Json::CharReaderBuilder reader;
        std::istringstream bodyStream(bodyStr);
        std::string errs;
        Json::parseFromStream(reader, bodyStream, &frame.body, &errs);

        frames.push_back(std::move(frame));
        s_recvBuffer.erase(0, sizeof(FrameHeader) + header.bodyLen);
    }
    return frames;
}

// Reset internal buffer — for tests only
void protocol_reset_buffer() {
    s_recvBuffer.clear();
}
