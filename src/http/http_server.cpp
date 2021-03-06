#include <string.h>
#include <iostream>
#include "http_server.h"

using namespace http;
using namespace tcp;

http::http_server::http_server(char const *ip, int port, http::http_request_handler* h)
        : handler(h) {

    if (!h->is_implemented(GET) || !h->is_implemented(HEAD))
        throw std::logic_error("HEAD or GET not implemented");

    service.reset(new io_service);
    server.reset(new async_server);
    server->bind(ip, port);
    server->listen();

    http_server::on_connect = [&](int error, async_socket *asyncSocket) {
        if (handle_error(error)) return;

        connection_map[asyncSocket].reset(new http_connection(asyncSocket));
        asyncSocket->read_some(service.get(), MAX_BUFFER_SIZE, on_read_some);
        server->get_connection(service.get(), on_connect);
    };

    for (int i = 0; i < MAX_CONNECTIONS; ++i)
        server->get_connection(service.get(), on_connect);

    http_server::on_send = [&](int error, async_socket *asyncSocket) {
        if (handle_error(error)) return;
        if (connection_map[asyncSocket]->condition == OUT) {
            service->del_client(server.get(), asyncSocket);
            connection_map.erase(asyncSocket);
        } else {
            asyncSocket->read_some(service.get(), MAX_BUFFER_SIZE, on_read_some);
        }
    };

    http_server::on_read_some = [&](int error, async_socket *asyncSocket, size_t size, void const *buf) {
        if (handle_error(error)) return;
        bool flag = false;
        if (size == 0) {
            connection_failed(asyncSocket);
            return;
        }

        connection_map[asyncSocket]->request += std::string((char const *) buf, (char const *) buf + size);

        if (connection_map[asyncSocket]->condition != IN_BODY) {
            on_no_body_data(asyncSocket);
        }
        if (connection_map[asyncSocket]->condition == IN_BODY && connection_map[asyncSocket]->need_body) {
            flag = on_body_data(asyncSocket, size);
        }
        if (flag) return;

        if (connection_map[asyncSocket]->condition == IN_BODY && !connection_map[asyncSocket]->need_body)
            return on_request(asyncSocket, true);

        if (connection_map[asyncSocket]->condition == IN_BODY
                && handler->is_on_some(connection_map[asyncSocket]->title.get_method().get_method_name()))
            on_request(asyncSocket, false);
        else
            asyncSocket->read_some(service.get(), MAX_BUFFER_SIZE, on_read_some);
    };

    http_server::on_write_some = [&](int error, async_socket *s) {
        std::cerr << connection_map.count(s) << std::endl;
        auto connection1 = connection_map[s].get();
        char c1[MAX_BUFFER_SIZE];
        ::memset(c1, 0, MAX_BUFFER_SIZE);

        size_t idx1 = connection1->sent;
        size_t idx2 = std::min(connection1->sent + MAX_BUFFER_SIZE, connection1->response.size());

        for (int j = 0; j < idx2 - idx1; ++j)
            c1[j] = connection1->response[idx1 + j];

        connection1->sent += idx2 - idx1;

        if (connection1->response.size() <= connection1->sent) {
            connection1->client->write(service.get(), c1, idx2 - idx1, on_send);
        } else {
            connection1->client->write(service.get(), c1, MAX_BUFFER_SIZE, on_write_some);
        }
    };
}

bool http_server::on_body_data(async_socket* asyncSocket, size_t) {

    auto curr = connection_map[asyncSocket].get();
    std::string cr_lf_server = "\r\n";

    char *cr_lf_p = (char *) cr_lf_server.c_str();
    unsigned long idx = curr->request.find(cr_lf_p);

    if (idx == std::string::npos) {
        curr->body.add(curr->request);
        curr->request = "";
        if (curr->headers.is_there("Content-Length")) {
            int i = std::stoi(curr->headers.get_value("Content-Length"), 0, 10);
            if (i == curr->body.size()) {
                curr->condition = OUT;
                on_request(asyncSocket, true);
                return true;
            }
        }
    } else {
        curr->body.add(curr->request);
        curr->request = "";
        curr->condition = OUT;
        on_request(asyncSocket, true);
        return true;
    }
    return false;
}

void http_server::on_no_body_data(async_socket* asyncSocket) {
    char const *cr_lf_p = "\r\n";
    auto curr = connection_map[asyncSocket].get();
    unsigned long idx = curr->request.find(cr_lf_p);

    while (idx != std::string::npos) {
        if (curr->condition == OUT) {

            curr->condition = IN_TITLE;
            curr->title = http_request_title(curr->request.substr(0, idx));
            curr->need_body = curr->title.get_method().get_method_name() == PUT
                    || curr->title.get_method().get_method_name() == POST;

            idx += 2;
            curr->request = curr->request.substr(idx, curr->request.size() - idx);

        } else if (curr->condition == IN_TITLE || curr->condition == IN_HEADER) {

            curr->condition = IN_HEADER;
            if (idx != 0) {
                curr->headers.add_header(curr->request.substr(0, idx));
                idx += 2;
                curr->request = curr->request.substr(idx, curr->request.size() - idx);
            } else {
                idx += 2;
                curr->request = curr->request.substr(idx, curr->request.size() - idx);
                curr->condition = IN_BODY;
                if (curr->need_body)
                    curr->body = http_body(curr->request, curr->headers.get_value("Content-Type"));
                else
                    curr->body = http_body();
                curr->request = "";
            }
        }
        idx = curr->request.find(cr_lf_p);
    }
}

void http::http_server::start() {
    service->run();
}

void http::http_server::stop() {
    service->stop();
}

bool http_server::handle_error(int error) {
    return handler->run_error_handler(error);
}

void http_server::on_request(tcp::async_socket* s, bool all) {

    http_connection* connection = connection_map[s].get();
    http_request request = http_request(connection->title, connection->headers, connection->body);
    http_response response;
    if (handler->is_implemented(connection->title.get_method().get_method_name())) {
        try {
            response = handler->get(connection->title.get_method().get_method_name())(request, all);
        } catch (std::exception e) {
            response = internal_error();
        }
    } else {
        response = not_implemented_response();
    }
    if (response.get().size() == 0) {
        s->read_some(service.get(), MAX_BUFFER_SIZE, on_read_some);
        return;
    }
    connection ->sent = 0;
    connection->to_string(response);
    if (connection->response.size() <= MAX_BUFFER_SIZE) {
        char c[MAX_BUFFER_SIZE];
        ::memset(c, 0, MAX_BUFFER_SIZE);
        for (int i = 0; i < connection->response.size(); ++i) {
            c[i] = connection->response[i];
        }
        connection->client->write(service.get(), c, connection->response.size(), on_send);
    } else {
        char c[MAX_BUFFER_SIZE];
        ::memset(c, 0, MAX_BUFFER_SIZE);
        for (int i = 0; i < MAX_BUFFER_SIZE; ++i) {
            c[i] = connection->response[i];
        }
        connection->sent += MAX_BUFFER_SIZE;
        connection->client->write(service.get(), c, MAX_BUFFER_SIZE, on_write_some);
    }
    if (all)
        connection->condition = OUT;
}

http_server::~http_server() {
}


http::http_response http_server::not_implemented_response() {
    http_response_title title;
    title.set_status(http_status(501, "Not Implemented"));
    http_headers headers;
    headers.add_header("Content-Length", "19");
    headers.add_header("Content-Type", "text/plain");
    http_body body("501 Not Implemented");

    return http_response(title, headers, body);
}

http_response http_server::internal_error() {
    http_response_title title;
    title.set_status(http_status(500, "Internal Server Error"));
    http_headers headers;
    headers.add_header("Content-Length", "25");
    headers.add_header("Content-Type", "text/plain");
    http_body body("500 Internal Server Error");
    return http_response(title, headers, body);
}

void http_server::connection_failed(async_socket *asyncSocket) {
    service->del_client(server.get(), asyncSocket);
    connection_map.erase(asyncSocket);
}
