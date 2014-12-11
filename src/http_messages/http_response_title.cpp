#include "http_response_title.h"
using namespace http;


http_response_title::http_response_title() :version(), status() {}

http_response_title::http_response_title(std::string s) {
    unsigned long idx = s.find(' ');
    std::string curr = s.substr(0, idx);
    s = s.substr(idx, s.size() - idx);
    version = http_version(curr);
    idx = s.find(' ');
    curr = s.substr(0, idx);
    int code = std::stoi(curr);
    s = s.substr(idx, s.size() - idx);
    status = http_status(code, s);}


void http_response_title::set_status(http_status s) {
    status = s;
}

void http_response_title::set_version(http_version v) {
    version = v;
}

http_status http_response_title::get_status() {
    return status;
}

http_version http_response_title::get_version() {
    return version;
}