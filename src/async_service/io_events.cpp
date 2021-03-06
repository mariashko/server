#include <sys/types.h>
#include <tcp/async_socket.h>
#include <string.h>
#include <iostream>
#include "io_events.h"

using namespace tcp;
using namespace std;

read_buffer::read_buffer(bool all, size_t t, function<void(int, async_socket*, size_t, void *)> function) {
    read_all = all;
    needed = t;
    done = 0;
    ::memset(buf, 0, MAX_BUFFER_SIZE);
    call = function;
}

write_buffer::write_buffer(void *pVoid, size_t t, function<void(int, async_socket*)> function) {
    ::memset(buf, 0, MAX_BUFFER_SIZE);
    ::memcpy(buf, pVoid, t);
    needed = t;
    done = 0;
    call = function;
}

connect_buffer::connect_buffer(char const *string, int p, function< void(int, async_socket*)> function) {
    ip = string;
    port = p;
    call = function;
}

accept_buffer::accept_buffer(function<void(int, async_socket*)> function) {
    call = function;
}

io_events::io_events(async_socket* s)
        : sock(s) {
    fd = s->get_fd();
}

void io_events::add_accept(accept_buffer buffer) {
    acceptors.push_back(buffer);
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
    return !acceptors.empty();
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

async_socket* io_events::get(int fd) {
   auto it = clients.begin();
    for (; it != clients.end() ; ++it) {
        if ((*it)->get_fd() == fd)
            return (*it).get();
    }
    return nullptr;
}

bool io_events::run_accept() {
    cerr << "Trying to accept! " << fd <<"\n";
    accept_buffer& now = acceptors.front();
    sockaddr_in addr;
    socklen_t addr_size;
    int flag = ::accept4(fd, (sockaddr *) &addr, &addr_size, SOCK_NONBLOCK);

    if (flag < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = errno;
            return true;
        }
        cerr << "We need some time!\n";
    } else {
        clients.emplace(new async_socket(flag));
        now.client = get(flag);
        cerr << "Success! Client " << flag << "\n";
        return true;
    }
    return false;
}

bool io_events::run_connect() {
    cerr << "Trying to connect! " << fd << "\n";
    connect_buffer& now = connectors.front();
    sockaddr_in addr;
    const char *ip = now.ip;
    int port = now.port;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::inet_addr(ip);
    addr.sin_port = ::htons((uint16_t) port);

    int flag = ::connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (flag < 0) {
        if (errno != EALREADY && errno != EINPROGRESS) {
            error = errno;
            return true;
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
    read_buffer& now = readers.front();
    char buffer[MAX_BUFFER_SIZE];
    size_t idx = now.done;
    size_t idx2 = now.needed;
    ssize_t r = ::recv(fd, buffer, idx2 - idx, MSG_DONTWAIT);

    if (r < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = errno;
            return true;
        }
        cerr << "We need some time!\n";
    } else {
        ::memcpy(now.buf + idx, buffer, (size_t) r);
        cerr << "Success!\n";
        cerr << "We've read " << r << "!\n";
        now.done += r;

        if (now.done == now.needed) {
            return true;
        }
    }
    return !now.read_all;
}

bool io_events::run_write() {
    cerr << "Trying to write! " << fd <<"\n";
    write_buffer& now = writers.front();

    const char *buffer = now.buf;
    size_t idx = now.done;
    size_t idx2 = now.needed;
    ssize_t w = ::send(fd, buffer + idx, idx2 - idx, MSG_DONTWAIT);

    if (w < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error = errno;
            return true;
        }
        cerr << "We need some time!\n";
    } else {
        cerr << "Success!\n";
        cerr << "We've written " << w << "!\n";
        now.done += w;

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
    accept_buffer now = acceptors.front();

    acceptors.pop_front();
    now.call(error, now.client);
}

void io_events::read_call_back() {
    read_buffer now = readers.front();

    readers.pop_front();
    now.call(error, sock, now.done, (void*)now.buf);
}

io_events::io_events() {
    fd = -1;
}

size_t io_events::get_writers() {
    return writers.size() + connectors.size();
}

size_t io_events::get_readers() {
    return readers.size() + acceptors.size();
}

io_events::io_events(int i) {
    fd = i;
}

io_events::~io_events() {
}

void io_events::remove_client(async_socket *asyncSocket) {
    auto it = clients.begin();
    for (; it != clients.end(); ++it) {
        if ((*it).get() == asyncSocket) {
            clients.erase(it);
            return;
        }
    }
}

