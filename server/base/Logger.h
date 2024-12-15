//
// Created by fan on 24-12-15.
//

#ifndef IMSERVER_LOGGER_H
#define IMSERVER_LOGGER_H

#include "Poco/Logger.h"
#include "Poco/FileChannel.h"
#include "Poco/AutoPtr.h"

namespace Base {

class IMLogger {
public:
    IMLogger(std::string& loggerName, std::string& file) : pChannel(new Poco::FileChannel), loggerName(loggerName) {
        pChannel->setProperty("path", file);
        pChannel->setProperty("rotation", "2 K");
        pChannel->setProperty("archive", "timestamp");
        Poco::Logger::root().setChannel(pChannel);
    }

    void information(const std::string& info) {
        Poco::Logger& logger = Poco::Logger::get(loggerName);
        logger.information(info);
    }

private:
    static IMLogger *instance;
    Poco::AutoPtr<Poco::FileChannel> pChannel;
    std::string& loggerName;
};


}

#endif //IMSERVER_LOGGER_H
