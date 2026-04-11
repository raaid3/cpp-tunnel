#pragma once

#include <sys/types.h>

int open_tun(const char* if_name);
void print_packet(const unsigned char* buf, ssize_t len);
