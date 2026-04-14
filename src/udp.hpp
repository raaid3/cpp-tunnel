#pragma once

#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <sys/types.h>

int create_udp_socket_and_bind(std::uint16_t local_port);
int make_udp_peer(const char* ip, std::uint16_t port, sockaddr_in* out_peer);
ssize_t send_udp_packet(int udp_fd, const sockaddr_in& peer, const unsigned char* data, size_t len);
