#ifndef HTTP_SERVER_HANDLER
#define HTTP_SERVER_HANDLER

#include <functional>
#include <set>
#include <map>
#include "http_response.h"
#include "http_request.h"

namespace http {
    struct http_request_handler {

        http_request_handler(std::function<http::http_response(http::http_request)>,
                std::function<http::http_response(http::http_request)>, std::function<void()>); // set get and head
        std::function<http::http_response(http::http_request)> get(method_name);
        bool is_implemented(method_name);
        void set(method_name, std::function<http::http_response(http::http_request)>);
        void on_terminate();
    private:
        std::map <http::method_name, std::function<http::http_response(http::http_request)> > handler_map;
        std::function<void()> on_term;
    };
}

#endif //HTTP_SERVER_HANDLER

