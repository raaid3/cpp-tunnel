#include <cerrno>
#include <cstring>
#include <iostream>
#include "tun.hpp"
#include <unistd.h>

int main() {
    int tun_fd = open_tun("tun0");
    if (tun_fd < 0) {
        return 1;
    }

    unsigned char buf[1500];
    ssize_t len;
    while (true) {
        len = read(tun_fd, buf, sizeof(buf));
        if (len == 0) {
            std::cout << "EOF\n";
            break;
        }
        if (len < 0) {
            std::cerr << "Error reading from TUN: " << strerror(errno) << "\n";
            continue;
        }
        print_packet(buf, len);
    }

    return 0;
}
