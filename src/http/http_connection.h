#ifndef HTTP_CONNECTION
#define HTTP_CONNECTION

#include "http_response.h"
#include "http_request.h"
#include <string>
#include <vector>
#include <map>
#include <tcp/async_server.h>
#include "http_request_handler.h"

namespace http {
    struct http_connection {

        http_connection(tcp::async_socket *);
        void to_string(http_response);

        parse_condition condition;
        tcp::async_socket *client;
        std::vector<char> response;
        size_t sent = 0;

        std::string request;
        bool need_body;
        http_request_title title;
        http_headers headers;
        http_body body;
    private:
    };

}

#endif // HTTP_CONNECTION
