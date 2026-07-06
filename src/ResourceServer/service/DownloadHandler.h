//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_DOWNLOADHANDLER_H
#define IMSERVER_DOWNLOADHANDLER_H

#include <string>

#include "net/HttpConnection.h"

class DownloadHandler {
public:
    void handle(std::shared_ptr<HttpConnection> conn);

private:
    std::string extractResourceId(const UrlParams& params, bool& wantThumb);
    std::string getMimeType(const std::string& filename);
    void sendError(std::shared_ptr<HttpConnection> conn, int error,
                   const std::string& message, boost::beast::http::status status);
    void serveFile(std::shared_ptr<HttpConnection> conn, const std::string& filePath,
                   const std::string& fileName, const std::string& md5);
};


#endif //IMSERVER_DOWNLOADHANDLER_H
