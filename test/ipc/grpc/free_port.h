#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace dross_test {

// A TCP port on the loopback address that nothing listens at just now, as
// "127.0.0.1:<port>". Another process may take it before it is used, which
// a test sees as a failure to listen. Empty when no port could be found.
inline std::string free_loopback_address()
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return {};
    }
    sockaddr_in where{};
    where.sin_family = AF_INET;
    where.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    where.sin_port = 0;
    socklen_t length = sizeof(where);
    const bool found = (::bind(fd, reinterpret_cast<const sockaddr*>(&where), sizeof(where)) == 0) && (::getsockname(fd, reinterpret_cast<sockaddr*>(&where), &length) == 0);
    ::close(fd);
    return found ? "127.0.0.1:" + std::to_string(ntohs(where.sin_port)) : std::string{};
}

}  // namespace dross_test
