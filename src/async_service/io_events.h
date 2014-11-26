#include <sys/types.h>
#include <deque>
#include <functional>
#include <tcp/async_socket.h>
#include "async_type.h"

#ifndef IO_EVENT
#define IO_EVENT

namespace tcp {
    struct read_buffer {
        char buf[256];
        size_t needed;
        size_t done;
        std::function <void(async_type<async_socket>, async_type<void*>)> call;
        read_buffer() = default;

        read_buffer(size_t, std::function <void(async_type<async_socket>, async_type<void*>)>);
    };

    struct write_buffer {
        char buf[256];
        size_t needed;
        size_t done;
        std::function <void(async_type<async_socket>)> call;

        write_buffer() = default;

        write_buffer(void *, size_t, std::function <void(async_type<async_socket>)>);
    };

    struct connect_buffer {
        const char *ip;
        int port;
        std::function <void(async_type<async_socket>)> call;

        connect_buffer() = default;
        connect_buffer(const char *, int, std::function <void(async_type<async_socket>)>);
    };

    struct accept_buffer {
        int client;
        std::function <void(async_type<async_socket>)> call;

        accept_buffer() = default;
        accept_buffer(std::function <void(async_type<async_socket>)>);
    };

    struct io_events {
        io_events();
        io_events(int);

        size_t get_writers();
        size_t get_readers();

        void add_accept(accept_buffer);
        void add_connect(connect_buffer);
        void add_read(read_buffer);
        void add_write(write_buffer);

        bool want_accept();
        bool want_connect();
        bool want_read();
        bool want_write();

        bool run_accept();
        bool run_connect();
        bool run_read();
        bool run_write();

        void accept_call_back();
        void connect_call_back();
        void read_call_back();
        void write_call_back();

    private:
        int fd;
        bool correct;
        std::exception e;
        std::deque <read_buffer> readers;
        std::deque <write_buffer> writers;
        std::deque <connect_buffer> connectors;
        std::deque <accept_buffer> accepters;
    };

}

#endif