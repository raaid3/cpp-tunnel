#include "tun.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

int open_tun(const char* if_name) {
	// open and configure a TUN interface
	int fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		std::cerr << "Error opening /dev/net/tun: " << strerror(errno) << "\n";
		return -1;
	}

	struct ifreq ifr {};
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

	// attach the named TUN interface
	if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
		std::cerr << "error with ioctl: " << strerror(errno) << "\n";
		close(fd);
		return -1;
	}

	std::cout << "Attached to interface: " << ifr.ifr_name << "\n";
	return fd;
}

// packet is going to be in the format of: [version + IHL][...][protocol][...][src IP][dst IP]
// parse the packet header and print info
void print_packet(const unsigned char* buf, ssize_t len) {
	if (len < 20) {
		std::cout << "Packet too short\n";
		return;
	}

	int version = buf[0] >> 4;
	if (version != 4) {
		std::cout << "Not IPv4\n";
		return;
	}

	char src[INET_ADDRSTRLEN];
	char dst[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, buf + 12, src, sizeof(src));
	inet_ntop(AF_INET, buf + 16, dst, sizeof(dst));

	int proto = buf[9];

	std::cout << "Packet: "
			  << "src=" << src
			  << " dst=" << dst
			  << " proto=" << proto
			  << " len=" << len
			  << "\n";
}
