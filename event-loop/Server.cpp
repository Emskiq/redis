#include <algorithm>
#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <netinet/ip.h>

#include "Connection.h"
#include "ConnectionStates.h"
#include "Utils.h"

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		die("socket()");
	}

	// setting some options for the socket
	int val = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	// bind, this is the syntax that deals with IPv4 addresses
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(1234); // converting to big endian
	addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0

	int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
	if (rv) {
		die("bind()");
	}

	// ------------- End

	// ------- Listening/Connecting part
	rv = listen(fd, SOMAXCONN);
	if (rv) {
		die("listen()");
	}

	ConnectionsMap fdToConnection;
	fdSetNonBlocking(fd);

	// the event loop
	std::vector<struct pollfd> pollArgs;
	while (true) {
		// prepare the arguments of the poll()
		pollArgs.clear();

		// for convenience, the listening fd is put in the first position
		struct pollfd pfd = {fd, POLLIN, 0};
		pollArgs.push_back(pfd);

		// connection fds
		for (auto [connFD, conn] : fdToConnection) {
			if (conn == nullptr) {
				continue;
			}

			struct pollfd pfd = {};
			pfd.fd = conn->fd;
			pfd.events = (conn->state == ConnectionState::Request) ? POLLIN : POLLOUT;
			pfd.events = pfd.events | POLLERR;
			pollArgs.push_back(pfd);
		}

		// poll for active fds
		// the timeout argument doesn't matter here
		// TODO: change to use epoll
		int rv = poll(pollArgs.data(), (nfds_t)pollArgs.size(), 1000);
		if (rv < 0) {
			die("poll");
		}

		// process active connections
		for (size_t i = 1; i < pollArgs.size(); ++i) {
			if (pollArgs[i].revents) {
				Connection *conn = fdToConnection[pollArgs[i].fd];

				connectionIO(conn);
				if (conn->state == ConnectionState::End) {
					// client closed normally, or something bad happened.
					// destroy this connection
					fdToConnection[conn->fd] = nullptr;
					(void)close(conn->fd);
					free(conn);
				}
			}
		}

		// try to accept a new connection if the listening fd is active
		if (pollArgs[0].revents) {
			acceptNewConnection(fdToConnection, fd);
		}
	}
}
