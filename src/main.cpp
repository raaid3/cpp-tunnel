#include <cerrno>
#include <cstring>
#include <iostream>
#include "tun.hpp"
#include "udp.hpp"
#include <unistd.h>

int main() {
    int tun_fd = open_tun("tun0");
    if (tun_fd < 0) {
        return 1;
    }

    // for now send every TUN packet to a fixed UDP peer
    constexpr std::uint16_t local_udp_port = 40000;
    constexpr std::uint16_t peer_udp_port = 40000;
    const char* peer_ip = "127.0.0.1";

    int udp_fd = create_udp_socket_and_bind(local_udp_port);
    if (udp_fd < 0) {
        close(tun_fd);
        return 1;
    }

    sockaddr_in peer_addr {};
    if (make_udp_peer(peer_ip, peer_udp_port, &peer_addr) < 0) {
        close(udp_fd);
        close(tun_fd);
        return 1;
    }

    std::cout << "UDP forwarding enabled: "
              << "local=0.0.0.0:" << local_udp_port
              << " peer=" << peer_ip << ":" << peer_udp_port
              << "\n";

    unsigned char buf[1500];
    ssize_t len;
    while (true) {
        len = read(tun_fd, buf, sizeof(buf));
        if (len == 0) {
            std::cout << "EOF\n";
            break;
        }
        if (len < 0) {
            std::cerr << "error reading from TUN: " << strerror(errno) << "\n";
            continue;
        }
        print_packet(buf, len);

        ssize_t sent = send_udp_packet(udp_fd, peer_addr, buf, static_cast<size_t>(len));
        if (sent < 0) {
            std::cerr << "error sending UDP packet: " << strerror(errno) << "\n";
            continue;
        }

        std::cout << "UDP sent: bytes=" << sent << "\n";
    }

    close(udp_fd);
    close(tun_fd);

    return 0;
}
