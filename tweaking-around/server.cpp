#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "utils.h"

// convert it to c++ not c
static int32_t oneRequest(int connfd) {
	// 4 bytes header
	Buffer<4 + maxMessageSize + 1> readBuffer;
	errno = 0;

	int32_t err = readFull(connfd, readBuffer, 4);
	if (err) {
		if (errno == 0) {
			msg("EOF");
		} else {
			msg("read() error");
		}
		return err;
	}

	uint32_t len = 0;
	std::memcpy(&len, readBuffer.data(), 4);  // assume little endian
	if (len > maxMessageSize) {
		msg("too long");
		return -1;
	}
	// request body
	err = readFull(connfd, readBuffer, len, 4);
	if (err) {
		msg("read() error");
		return err;
	}

	// do something
	readBuffer[4 + len] = '\0';
	std::cout << "client says: " << readBuffer.data() + 4 << std::endl;

	// reply using the same protocol
	const std::string responseMsg{"world"};
	Buffer<4 + maxMessageSize> writeBuffer;

	if (int err = messageToBuffer(responseMsg, writeBuffer)) {
		return err;
	}
	return writeAll(connfd, writeBuffer, 4 + responseMsg.length());
}

int main()
{
	// ------ Configuration part
	//
	// creating the socket - protocol (protocol family, type, protocol)
	int socketFD = socket(AF_INET, SOCK_STREAM, 0);

	// setting some options for the socket
	int val = 1;
	setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	// bind, this is the syntax that deals with IPv4 addresses
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(1234); // converting to big endian
	addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0

	int rv = bind(socketFD, (const sockaddr *)&addr, sizeof(addr));
	if (rv) {
		die("bind()");
	}

	// ------------- End

	// ------- Listening/Connecting part
	rv = listen(socketFD, SOMAXCONN);
	if (rv) {
		die("listen()");
	}


	// Infinive Loop to accept and proccess each new connection
	while (true) {
		struct sockaddr_in client_addr = {};
		socklen_t addrlen = sizeof(client_addr);
		int connfd = accept(socketFD, (struct sockaddr *)&client_addr, &addrlen);
		if (connfd < 0) {
			continue;   // error
		}

		// doSomething(connfd); - replacing with infinete loop
		//			   simulating a connection
		// only serves one client connection at once
		while (true) {
			int32_t err = oneRequest(connfd);
			if (err) {
				break;
			}
		}
		close(connfd);
	}
}
