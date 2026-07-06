//
// Created by Fan on 2026/7/3.
//

#ifndef IMSERVER_URLPARSER_H
#define IMSERVER_URLPARSER_H

#include <string>
#include <unordered_map>

using UrlParams = std::unordered_map<std::string, std::string>;

class UrlParser {
public:
    UrlParser() = default;
    void parse(const std::string& url);
    [[nodiscard]] const std::string& getPath() const;
    [[nodiscard]] const UrlParams& getParams() const;
    [[nodiscard]] bool hasParam(const std::string& key) const;
    [[nodiscard]] std::string getParam(const std::string& key, const std::string& defaultValue = "") const;
private:
    std::string path_;
    UrlParams params_;
    static std::string urlDecode(const std::string& encoded);
};

#endif //IMSERVER_URLPARSER_H
