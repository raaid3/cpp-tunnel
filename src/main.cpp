#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

// open and configure a TUN interface
int open_tun(const char* if_name) {
    // open the TUN driver device
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        std::cerr << "Error opening /dev/net/tun: " << strerror(errno) << "\n";
        return -1;
    }

    struct ifreq ifr {};

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    // create or attach the named TUN interface
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        std::cerr << "error with ioctl: " << strerror(errno) << "\n";
        close(fd);
        return -1;
    }

    std::cout << "Attached to interface: " << ifr.ifr_name << "\n";
    return fd;
}


int main() {
    int tun_fd = open_tun("tun0");
    if (tun_fd < 0) {
        return 1;
    }

    return 0;
}
