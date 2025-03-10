#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <cstring>

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "utils.h"

int32_t queryMsg(int fd, const std::string& message) {
	Buffer<4 + maxMessageSize> writeBuffer;

	if (int err = messageToBuffer(message, writeBuffer)) {
		return err;
	}

	if (int32_t err = writeAll(fd, writeBuffer, 4 + message.length())) {
		return err;
	}

	// 4 bytes header
	Buffer<4 + maxMessageSize + 1> readBuffer;
	errno = 0;
	int32_t err = readFull(fd, readBuffer, 4);
	if (err) {
		if (errno == 0) {
			msg("EOF");
		} else {
			msg("read() error");
		}
		return err;
	}

	uint32_t len{0};
	std::memcpy(&len, readBuffer.data(), 4);
	if (len > maxMessageSize) {
		msg("too long");
		return -1;
	}
	// reply body
	err = readFull(fd, readBuffer, len, 4);
	if (err) {
		msg("read() error");
		return err;
	}
	// do something
	readBuffer[4 + len] = '\0';
	std::cout << "server reply: " << readBuffer.data() + 4 << std::endl;

	return 0;
}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		die("socket()");
	}

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(1234);
	addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);  // 127.0.0.1
	// -----

	// ---- Connect and write sth
	int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
	if (rv) {
		die("connect");
	}

	// send multiple requests
	int32_t err = queryMsg(fd, "hello1");
	if (err) {
		close(fd);
		die("quering message failed!");
	}
	err = queryMsg(fd, "hello2");
	if (err) {
		close(fd);
		die("quering message failed!");
	}
	err = queryMsg(fd, "hello3");
	if (err) {
		close(fd);
		die("quering message failed!");
	}

	close(fd);
}
