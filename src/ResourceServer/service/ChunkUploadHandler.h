//
// Created by Fan on 2026/7/3.
//

#ifndef IMSERVER_CHUNKUPLOADHANDLER_H
#define IMSERVER_CHUNKUPLOADHANDLER_H

#include "net/HttpConnection.h"

class ChunkUploadHandler {
public:
    // POST /r/upload  body: { conv_id, file_name, file_md5, file_size, chunk_size }
    static void handleInit(std::shared_ptr<HttpConnection> conn);

    // POST /r/upload/chunk  headers: X-Upload-Id, X-Chunk-Index, X-Chunk-Md5
    //                           body: 原始二进制分片 (≤ chunk_size)
    static void handleChunk(std::shared_ptr<HttpConnection> conn);

    // POST /r/upload/status  body: { upload_id }
    static void handleStatus(std::shared_ptr<HttpConnection> conn);

    // POST /r/upload/finalize  headers: X-Upload-Id
    static void handleFinalize(std::shared_ptr<HttpConnection> conn);

private:
    static void sendJsonResponse(std::shared_ptr<HttpConnection> conn,
                                  http::status status, const Json::Value& body);
    static std::string getUploadRootPath();
    static std::string getTmpRootPath();
};

#endif //IMSERVER_CHUNKUPLOADHANDLER_H
