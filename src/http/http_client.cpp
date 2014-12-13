#include <string.h>
#include <iostream>
#include "http_client.h"
using namespace tcp;
using namespace http;


http_client::http_client() {
    std::string cr_lf_client = "\r\n";
    std::cerr << "MOTHERFUCKER" << std::endl;
    service = new io_service;
    http_client::on_accept = [&](std::string error, async_socket *s) {
        error_handle(error);
        s->write(service, request, request_len, on_write);
    };

    http_client::on_write = [&](std::string error, async_socket *s) {
        error_handle(error);
        s->read_some(service, 10240, on_read_some);
    };

    http_client::on_read_some = [&](std::string error, async_socket *s, void *buf) {
        error_handle(error);

        if (parse != IN_BODY) {
            curr += std::string((char *) buf);
            char *cr_lf_p = (char *) cr_lf_client.c_str();
            unsigned long idx = curr.find(cr_lf_p);

            while (idx != std::string::npos) {
                if (parse == OUT) {

                    parse = IN_TITLE;
                    title = http_response_title(curr.substr(0, idx));

                    int code = title.get_status().get_code();
                    need_body = !(code == 204 || code == 304 || code / 100 == 1);

                    idx += 2;
                    curr = curr.substr(idx, curr.size() - idx);

                } else if (parse == IN_TITLE || parse == IN_HEADER) {

                    parse = IN_HEADER;
                    if (idx != 0) {
                        headers.add_header(curr.substr(0, idx));
                        idx += 2;
                        curr = curr.substr(idx, curr.size() - idx);
                    } else {

                        parse = IN_BODY;
                        body = http_body(curr.size(), (void *) curr.c_str(), headers.get_valuse("Content-Type"));
                    }
                }
                idx = curr.find(cr_lf_p);
            }
        } else if (need_body) {
            char *cr_lf_p = (char *) cr_lf_client.c_str();
            unsigned long idx = curr.find(cr_lf_p);

            if (idx == std::string::npos) {
                body.add(curr.size(), (void *) curr.c_str());

                if (std::string((char *) buf).size() == 0) {
                    return on_exit();
                }
                if (headers.is_there("Content-Length")) {
                    int i = std::stoi(headers.get_valuse("Content-Length"), 0, 10);
                    if (i == body.size()) {
                        return on_exit();
                    }
                }
            } else {
                std::string s2 = curr.substr(0, idx);
                body.add(s2.size(), (void *) s2.c_str());
                curr = curr.substr(idx, curr.size() - idx);
                return on_exit();
            }
        }
        s->read_some(service, 10240, on_read_some);
    };
}

void http_client::to_string(http_request r) {
    request_len = 0;
    headers = http_headers();

    std::string curr = r.get_title().get();
    ::memcpy(request, curr.c_str(), curr.length());
    request_len += curr.length();

    curr = r.get_headers().get();
    ::memcpy(request + request_len, curr.c_str(), curr.length());
    request_len += curr.length();

    ::memcpy(request + request_len, r.get_body().get(), r.get_body().size());
    request_len += r.get_body().size();
}

http_response http_client::send(char const *ip, int port, http::http_request r, std::function<void(http_response)> f) {
    client = new async_socket;
    need_body = r.get_title().get_method().get() != "HEAD";
    to_string(r);
    client->set_connection(service, ip, port, on_accept);
    on_response = f;
}

void http_client::error_handle(std::string error) {
    // TODO handle error
}




void http_client::on_exit(){
    http_response response(title, headers, body);
    headers = http_headers();
    curr = "";
    memset(request, 0, sizeof request);
    delete client;
    on_response(response);
}


