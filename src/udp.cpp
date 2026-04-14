#include "udp.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>

int create_udp_socket_and_bind(std::uint16_t local_port) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		std::cerr << "error creating udp socket: " << strerror(errno) << "\n";
		return -1;
	}

	sockaddr_in local_addr {};
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(local_port);

	if (bind(fd, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0) {
		std::cerr << "error binding udp socket " << strerror(errno) << "\n";
		close(fd);
		return -1;
	}

	return fd;
}

int make_udp_peer(const char* ip, std::uint16_t port, sockaddr_in* out_peer) {
	if (out_peer == nullptr) {
		errno = EINVAL;
		return -1;
	}

	sockaddr_in peer {};
	peer.sin_family = AF_INET;
	peer.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &peer.sin_addr) != 1) {
		std::cerr << "Invalid peer IP address: " << ip << "\n";
		errno = EINVAL;
		return -1;
	}

	*out_peer = peer;
	return 0;
}

ssize_t send_udp_packet(int udp_fd, const sockaddr_in& peer, const unsigned char* data, size_t len) {
	return sendto(
		udp_fd,
		data,
		len,
		0,
		reinterpret_cast<const sockaddr*>(&peer),
		sizeof(peer)
	);
}
