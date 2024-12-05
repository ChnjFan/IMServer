//
// Created by fan on 24-12-5.
//

#include "Exception.h"

Base::Exception::Exception(const std::string what) : message(std::move(what)) {

}

const char *Base::Exception::what() const noexcept {
    return message.c_str();
}
