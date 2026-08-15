//
// Created by Fan on 2026/7/3.
//

#include <sstream>
#include "UrlParser.h"

void UrlParser::parse(const std::string& url) {
    path_.clear();
    params_.clear();

    if (size_t queryPos = url.find('?'); queryPos != std::string::npos) {
        path_ = url.substr(0, queryPos);
        std::string queryString = url.substr(queryPos + 1);

        std::stringstream ss(queryString);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            if (size_t eqPos = pair.find('='); eqPos != std::string::npos) {
                std::string key = urlDecode(pair.substr(0, eqPos));
                std::string value = urlDecode(pair.substr(eqPos + 1));
                params_[key] = value;
            } else if (!pair.empty()) {
                params_[urlDecode(pair)] = "";
            }
        }
    } else {
        path_ = url;
    }
}

const std::string& UrlParser::getPath() const {
    return path_;
}

const UrlParams& UrlParser::getParams() const {
    return params_;
}

bool UrlParser::hasParam(const std::string& key) const {
    return params_.find(key) != params_.end();
}

std::string UrlParser::getParam(const std::string& key, const std::string& defaultValue) const {
    if (const auto it = params_.find(key); it != params_.end()) {
        return it->second;
    }
    return defaultValue;
}

std::string UrlParser::urlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            char hex1 = encoded[i + 1];
            char hex2 = encoded[i + 2];
            int value = 0;

            if (hex1 >= '0' && hex1 <= '9') value += (hex1 - '0') << 4;
            else if (hex1 >= 'A' && hex1 <= 'F') value += (hex1 - 'A' + 10) << 4;
            else if (hex1 >= 'a' && hex1 <= 'f') value += (hex1 - 'a' + 10) << 4;

            if (hex2 >= '0' && hex2 <= '9') value += hex2 - '0';
            else if (hex2 >= 'A' && hex2 <= 'F') value += hex2 - 'A' + 10;
            else if (hex2 >= 'a' && hex2 <= 'f') value += hex2 - 'a' + 10;

            decoded += static_cast<char>(value);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}
