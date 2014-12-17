
#ifndef IO_SERVICE
#define IO_SERVICE

#include <functional>
#include <map>
#include <sys/signalfd.h>
#include "epoll.h"
#include "io_events.h"

namespace tcp {

    struct io_service {

        io_service();
        io_service(const io_service&) = delete;
        io_service(io_service&&) = default;
        bool operator < (const io_service&) const;

        void run();
        void stop();
        void clean_stop();


        ~io_service();

    private:
        friend class async_server;
        friend class async_socket;
        friend class io_events;

        void read_waiter(async_socket*, size_t, std::function <void(int, async_socket*, void*)>);
        void read_some_waiter(async_socket*, size_t, std::function <void(int, async_socket*, void*)>);
        void write_waiter(async_socket*, void*, size_t, std::function <void(int, async_socket*)>);
        void accept_waiter(struct async_server*, std::function <void(int, async_socket*)>);
        void connect_waiter(async_socket*, const char*, int, std::function <void(int, async_socket*)>);

        bool clean;
        epoll* efd;
        volatile int stopper;
        volatile int clean_stopper;

        std::map <int, io_events> data;
    };
}

#endif // IO_SERVICE