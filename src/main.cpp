#include "tun.hpp"
#include "udp.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
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

    sockaddr_in peer_addr{};
    if (make_udp_peer(peer_ip, peer_udp_port, &peer_addr) < 0) {
        close(udp_fd);
        close(tun_fd);
        return 1;
    }

    std::cout << "UDP forwarding enabled: " << "local=0.0.0.0:"
              << local_udp_port << " peer=" << peer_ip << ":" << peer_udp_port
              << "\n";

    unsigned char tun_buf[1500];
    unsigned char udp_buf[1500];
    pollfd fds[2] = {
        {tun_fd, POLLIN, 0},
        {udp_fd, POLLIN, 0},
    };

    while (true) {
        int poll_result = poll(fds, 2, -1);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "poll failed: " << strerror(errno) << "\n";
            break;
        }

        if (fds[0].revents & POLLIN) {
            ssize_t len = read(tun_fd, tun_buf, sizeof(tun_buf));
            if (len == 0) {
                std::cout << "EOF\n";
                break;
            }
            if (len < 0) {
                std::cerr << "error reading from TUN: " << strerror(errno)
                          << "\n";
                continue;
            }
            print_packet(tun_buf, len);

            ssize_t sent = send_udp_packet(udp_fd, peer_addr, tun_buf,
                                           static_cast<size_t>(len));
            if (sent < 0) {
                std::cerr << "error sending UDP packet: " << strerror(errno)
                          << "\n";
                continue;
            }

            std::cout << "UDP sent: bytes=" << sent << "\n";
        }

        if (fds[1].revents & POLLIN) {
            sockaddr_in from_addr{};
            ssize_t received =
                recv_udp_packet(udp_fd, udp_buf, sizeof(udp_buf), &from_addr);
            if (received < 0) {
                std::cerr << "error receiving UDP packet: " << strerror(errno)
                          << "\n";
                continue;
            }

            char from_ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &from_addr.sin_addr, from_ip, sizeof(from_ip));
            std::cout << "UDP received: bytes=" << received
                      << " from=" << from_ip << ":" << ntohs(from_addr.sin_port)
                      << "\n";

            // for now, receive and inspect only
            print_packet(udp_buf, received);
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            std::cerr << "TUN fd signaled an error condition\n";
            break;
        }

        if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            std::cerr << "UDP fd signaled an error condition\n";
            break;
        }
    }

    close(udp_fd);
    close(tun_fd);

    return 0;
}
