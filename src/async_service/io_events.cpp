#include <sys/types.h>
#include <tcp/async_socket.h>
#include <string.h>
#include <iostream>
#include "io_events.h"

using namespace tcp;
using namespace std;

tcp::read_buffer::read_buffer(size_t t, function<void(std::string, async_socket*, void *)> function) {
    needed = t;
    done = 0;
    call = function;
}

write_buffer::write_buffer(void *pVoid, size_t t, function<void(std::string, async_socket*)> function) {
    ::memcpy(buf, pVoid, t);
    needed = t;
    done = 0;
    call = function;
}

connect_buffer::connect_buffer(char const *string, int p, function< void(std::string, async_socket*)> function) {
    ip = string;
    port = p;
    call = function;
}

accept_buffer::accept_buffer(function<void(std::string, async_socket*)> function) {
    call = function;
}

io_events::io_events(async_socket* s)
        : sock(s) {
    fd = s->get_fd();
    correct = true;
}

void io_events::add_accept(accept_buffer buffer) {
    accepters.push_back(buffer);
}

void io_events::add_connect(connect_buffer buffer) {
    connectors.push_back(buffer);
}

void io_events::add_read(read_buffer buffer) {
    readers.push_back(buffer);
}

void io_events::add_write(write_buffer buffer) {
    writers.push_back(buffer);
}

bool io_events::want_accept() {
    return !accepters.empty();
}

bool io_events::want_connect() {
    return !connectors.empty();
}

bool io_events::want_read() {
    return !readers.empty();
}

bool io_events::want_write() {
    return !writers.empty();
}

bool io_events::run_accept() {
    cerr << "Trying to accept! " << fd <<"\n";
    accept_buffer now = accepters.front();
    sockaddr_in addr;
    socklen_t addr_size;
    int flag = ::accept4(fd, (sockaddr *) &addr, &addr_size, SOCK_NONBLOCK);

    if (flag < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = strerror(errno);
            correct = false;
        }
        cerr << "We need some time!\n";
    } else {
        now.client = new async_socket(flag);
        accepters.pop_front();
        accepters.push_front(now);
        cerr << "Success! Client " << flag << "\n";
        return true;
    }
    return false;
}

bool io_events::run_connect() {
    cerr << "Trying to connect! " << fd << "\n";
    connect_buffer now = connectors.front();
    sockaddr_in addr;
    const char *ip = now.ip;
    int port = now.port;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::inet_addr(ip);
    addr.sin_port = ::htons((uint16_t) port);

    int flag = ::connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (flag < 0) {
        if (errno != EALREADY && errno != EINPROGRESS) {
            error = strerror(errno);
            correct = false;
        }
        cerr << "We need some time!\n";
    } else {
        cerr << "Success!\n";
        return true;
    }
    return false;
}

bool io_events::run_read() {
    cerr << "Trying to read! " << fd << "\n";
    read_buffer now = readers.front();
    char buffer[256];
    size_t idx = now.done;
    size_t idx2 = now.needed;
    ssize_t r = ::recv(fd, buffer, idx2 - idx, MSG_DONTWAIT);

    if (r < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = strerror(errno);
            correct = false;
        }
        cerr << "We need some time!\n";
    } else {
        ::memcpy(now.buf + idx, buffer, r);
        cerr << "Success!\n";
        cerr << "We've read " << r << "!\n";
        now.done += r;

        readers.pop_front();
        readers.push_front(now);
        if (now.done == now.needed) {
            return true;
        }
    }
    return false;
}

bool io_events::run_write() {
    cerr << "Trying to write! " << fd <<"\n";
    write_buffer now = writers.front();

    const char *buffer = now.buf;
    size_t idx = now.done;
    size_t idx2 = now.needed;
    ssize_t w = ::send(fd, buffer + idx, idx2 - idx, MSG_DONTWAIT);

    if (w < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = strerror(errno);
            correct = false;
        }
        cerr << "We need some time!\n";

    } else {
        cerr << "Success!\n";
        cerr << "We've written " << w << "!\n";
        now.done += w;
        writers.pop_front();
        writers.push_front(now);

        if (now.done == now.needed)
            return true;
    }
    return false;
}

void io_events::write_call_back() {
    write_buffer now = writers.front();

    writers.pop_front();
    now.call(error, sock);
}

void io_events::connect_call_back() {
    connect_buffer now = connectors.front();

    connectors.pop_front();
    now.call(error, sock);
}

void io_events::accept_call_back() {
    accept_buffer now = accepters.front();

    accepters.pop_front();
    now.call(error, now.client);
}

void io_events::read_call_back() {
    read_buffer now = readers.front();

    readers.pop_front();
    now.call(error, sock, (void*)now.buf);
}

io_events::io_events() {
    fd = -1;
    correct = false;
}

size_t io_events::get_writers() {
    return writers.size() + connectors.size();
}

size_t io_events::get_readers() {
    return readers.size() + accepters.size();
}

io_events::io_events(int i) {
    fd = i;
}
