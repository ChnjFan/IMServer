//
// Created by fan on 24-12-12.
//

#ifndef IMSERVER_STRING_H
#define IMSERVER_STRING_H

#include <vector>
#include <string>
#include <regex>

namespace Base {

class String {
public:
    static void split(std::vector<std::string> &tokens, std::string str, char delimiter) {
        std::regex reg((std::string)"([^" + std::string(1, delimiter) + "]+)");
        std::sregex_token_iterator it(str.begin(), str.end(), reg, -1);
        std::sregex_token_iterator reg_end;

        while (it != reg_end) {
            tokens.push_back(*it);
            ++it;
        }

    }

};

}
#endif //IMSERVER_STRING_H
