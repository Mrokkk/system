#pragma once

#include <sys/types.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_socket.h.html

#define SOCK_DGRAM      0
#define SOCK_RAW        1
#define SOCK_SEQPACKET  2
#define SOCK_STREAM     3

#define SOL_SOCKET      1

#define AF_INET         0
#define AF_INET6        1
#define AF_UNIX         2
#define AF_UNSPEC       3

typedef unsigned int    sa_family_t;
typedef size_t          socklen_t;

struct sockaddr
{
    sa_family_t sa_family;  // Address family
    char        sa_data[];  // Socket address (variable-length data)
};

struct sockaddr_storage
{
    sa_family_t ss_family;
};
