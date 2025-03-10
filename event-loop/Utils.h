#pragma once

#include <array>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>

#include "Connection.h"

void die(const std::string& msg) {
	int err = errno;
	std::cerr << err << " - " << msg << std::endl;
	exit(err);
}

void msg(const std::string& msg) {
	std::cerr << msg << std::endl;
}

template <size_t N>
int32_t readFull(int fd, Buffer<N>& buf, size_t n, size_t off = 0) {
	while (n > 0) {
		ssize_t rv = read(fd, buf.data() + off, n);
		if (rv <= 0) {
			return -1;  // error, or unexpected EOF
		}
		assert((size_t)rv <= n);
		n -= (size_t)rv;
		off += rv;
	}
	return 0;
}

template <size_t N>
int32_t writeAll(int fd, const Buffer<N> buf, size_t n, size_t off = 0) {
	while (n > 0) {
		ssize_t rv = write(fd, buf.data() + off, n);
		if (rv <= 0) {
			return -1;  // error
		}
		assert((size_t)rv <= n);
		n -= (size_t)rv;
		off += rv;
	}
	return 0;
}

static void fdSetNonBlocking(int fd) {
	errno = 0;
	int flags = fcntl(fd, F_GETFL, 0);
	if (errno) {
		die("fcntl error");
		return;
	}

	flags |= O_NONBLOCK;

	errno = 0;
	(void)fcntl(fd, F_SETFL, flags);
	if (errno) {
		die("fcntl error");
	}
}

int32_t acceptNewConnection(ConnectionsMap& fd2conn, int fd) {
	// accept
	struct sockaddr_in client_addr = {};
	socklen_t socklen = sizeof(client_addr);
	int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
	if (connfd < 0) {
		msg("accept() error");
		return -1;  // error
	}

	// set the new connection fd to nonblocking mode
	fdSetNonBlocking(connfd);

	// creating the struct Conn
	Connection* conn = new Connection(connfd);
	if (!conn) {
		// Should not happen
		close(connfd);
		return -1;
	}

	conn->state = ConnectionState::Request;
	conn->rbufSize = 0;
	conn->wbufSize = 0;
	conn->wbufSent = 0;

	fd2conn[connfd] = conn;

	return 0;
}

template <typename T, size_t N>
bool readFromBuf(const Buffer<N>& buf, T& dest, size_t offset = 0) {
	if (buf.size() < sizeof(T) + offset) {
		return false;
	}
	std::memcpy(&dest, buf.data() + offset, sizeof(T));
	return true;
}

template <size_t N>
bool readFromBuf(const Buffer<N>& buf, std::string& dest, size_t offset = 0) {
	constexpr size_t lenSize = sizeof(int32_t); // Size of the message length header
	if (buf.size() < lenSize + offset) {
		return false;
	}

	// Read the length of the string
	int32_t len;
	if (!readFromBuf(buf, len, offset)) {
		return false;
	}

	if (len < 0 || len > maxMessageSize || len > buf.size() - lenSize - offset) {
		return false;
	}

	dest.resize(len);
	std::memcpy(dest.data(), buf.data() + lenSize + offset, len);

	return true;
}

template<size_t N>
int writeToBuf(const std::string& message, Buffer<N>& buf, size_t offset = 0)
{
	constexpr size_t lenSize = sizeof(int32_t); // Size of the message length header
	const int32_t len = (int32_t)message.length();

	if (len > maxMessageSize) {
		return -1;
	}

	std::memcpy(buf.data() + offset, &len, lenSize);
	std::memcpy(buf.data() + offset + lenSize, message.data(), len);

	return 0;
}

template<typename T, size_t N>
int writeToBuf(const T& message, Buffer<N>& buf, size_t offset = 0)
{
	constexpr size_t typeSize = sizeof(T);

	if (typeSize > maxMessageSize) {
		return -1;
	}

	std::memcpy(buf.data() + offset, &message, typeSize);

	return 0;
}

template<size_t N>
int advanceBuf(Buffer<N>& buf, size_t advance, size_t remaining)
{
	// remove the request from the buffer.
	// note: frequent memmove is inefficient.
	// note: need better handling for production code.
	// TODO: Rework
	std::memmove(buf.data(), &buf[advance], remaining);
	return 0;
}

// TODO: Delete, just a helper
void printVec(const std::vector<std::string>& vec)
{
	for (const auto& s : vec){
		std::cerr << s << " ";
	}
}
