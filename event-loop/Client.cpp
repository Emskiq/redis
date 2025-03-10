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

#include "Utils.h"
#include "Connection.h"

int32_t sendRequest(int fd, const std::string& text) {
	Buffer<4 + maxMessageSize> writeBuffer{};

	if (int err = writeToBuf(text, writeBuffer)) {
		return err;
	}

	if (int32_t err = writeAll(fd, writeBuffer, 4 + text.length())) {
		return err;
	}

	return 0;
}

int32_t sendRequest(int fd, const std::vector<std::string>& commands) {
	uint32_t len = 4;
	for (const std::string &s : commands) {
		len += 4 + s.size();
	}
	if (len > maxMessageSize) {
		return -1;
	}

	Buffer<4 + maxMessageSize> writeBuffer{};
	writeToBuf(len, writeBuffer);

	const uint32_t n = commands.size();
	writeToBuf(n, writeBuffer, 4);

	// memcpy(&wbuf[4], &n, 4);

	size_t cur = 8;
	for (const std::string &s : commands) {
		// uint32_t p = (uint32_t)s.size();
		writeToBuf(s, writeBuffer, cur);
		// memcpy(&wbuf[cur], &p, 4);
		// memcpy(&wbuf[cur + 4], s.data(), s.size());
		cur += 4 + s.size();
	}
	return writeAll(fd, writeBuffer, 4 + len);
}

// TODO: A little rework
int32_t readResponse(int fd) {
	// // 4 bytes header
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
	// readBuffer[4 + len] = '\0';
	// std::cout << "server reply: " << readBuffer.data() + 4 << std::endl;

	// print the result
	uint32_t rescode = 0;
	if (len < 4) {
		msg("bad response");
		return -1;
	}
	readFromBuf(readBuffer, rescode, sizeof(len));
	printf("server says: [%u] %.*s\n", rescode, len - 4, &readBuffer[8]);

	return 0;
}

int main(int argc, char **argv)
{
	// -- Some configuration
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

	// multiple pipelined requests
	// const std::vector<std::string> messages = {"hello1", "hello2", "hello3"};
	// for (const auto& message : messages) {
	// 	int32_t err = sendRequest(fd, message);
	// 	if (err) {
	// 		close(fd);
	// 		return 0;
	// 	}
	// }
	//
	// for (size_t i = 0; i < messages.size(); ++i) {
	// 	int32_t err = readResponse(fd);
	// 	if (err) {
	// 		close(fd);
	// 		return 0;
	// 	}
	// }

	std::vector<std::string> cmd;
	for (int i = 1; i < argc; ++i) {
		cmd.push_back(argv[i]);
	}

	int32_t err = sendRequest(fd, cmd);
	if (err) {
		close(fd);
		return 1;
	}

	err = readResponse(fd);
	if (err) {
		close(fd);
		return 1;
	}

}
