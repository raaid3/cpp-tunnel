#pragma once

#include <cstdint>
#include <string>

struct TunnelConfig {
    std::string if_name = "tun0";
    std::string peer_ip = "127.0.0.1";
    std::uint16_t local_udp_port = 40000;
    std::uint16_t peer_udp_port = 40000;
};
