#include "tun.hpp"
#include "udp.hpp"
#include "config.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <string>
#include <unistd.h>

namespace {

void print_usage(const char* prog_name) {
    std::cout << "usage: " << prog_name << " [options]\n"
              << "  --if <name>          TUN interface name (default: tun0)\n"
              << "  --local-port <port>  local UDP bind port (default: 40000)\n"
              << "  --peer-ip <ipv4>     peer IPv4 address (default: 127.0.0.1)\n"
              << "  --peer-port <port>   peer UDP port (default: 40000)\n"
              << "  --help               show this help\n";
}

bool parse_port(const char* value, std::uint16_t* out_port) {
    if (value == nullptr || out_port == nullptr) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > 65535UL) {
        return false;
    }

    *out_port = static_cast<std::uint16_t>(parsed);
    return true;
}

int parse_args(int argc, char* argv[], TunnelConfig* config) {
    if (config == nullptr) {
        return -1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 1;
        }

        if (i + 1 >= argc) {
            std::cerr << "missing value for option: " << arg << "\n";
            print_usage(argv[0]);
            return -1;
        }

        const char* value = argv[++i];
        if (arg == "--if") {
            config->if_name = value;
            continue;
        }

        if (arg == "--peer-ip") {
            config->peer_ip = value;
            continue;
        }

        if (arg == "--local-port") {
            if (!parse_port(value, &config->local_udp_port)) {
                std::cerr << "invalid local UDP port: " << value << "\n";
                return -1;
            }
            continue;
        }

        if (arg == "--peer-port") {
            if (!parse_port(value, &config->peer_udp_port)) {
                std::cerr << "invalid peer UDP port: " << value << "\n";
                return -1;
            }
            continue;
        }

        std::cerr << "unknown option: " << arg << "\n";
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    TunnelConfig config;
    int parse_result = parse_args(argc, argv, &config);
    if (parse_result > 0) {
        return 0;
    }
    if (parse_result < 0) {
        return 1;
    }

    int tun_fd = open_tun(config.if_name.c_str());
    if (tun_fd < 0) {
        return 1;
    }

    int udp_fd = create_udp_socket_and_bind(config.local_udp_port);
    if (udp_fd < 0) {
        close(tun_fd);
        return 1;
    }

    sockaddr_in peer_addr{};
    if (make_udp_peer(config.peer_ip.c_str(), config.peer_udp_port, &peer_addr) < 0) {
        close(udp_fd);
        close(tun_fd);
        return 1;
    }

    std::cout << "UDP forwarding enabled: " << "local=0.0.0.0:"
              << config.local_udp_port << " peer=" << config.peer_ip << ":" << config.peer_udp_port
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

            print_packet(udp_buf, received);

            ssize_t written = write(tun_fd, udp_buf, static_cast<size_t>(received));
            if (written < 0) {
                std::cerr << "error writing packet to TUN: " << strerror(errno)
                          << "\n";
                continue;
            }

            if (written != received) {
                std::cerr << "partial write to TUN: wrote=" << written
                          << " expected=" << received << "\n";
                continue;
            }

            std::cout << "TUN injected: bytes=" << written << "\n";
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
