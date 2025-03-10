#include <array>
#include <iostream>
#include <string>
#include <cstdlib>

#include <sys/socket.h>

static constexpr size_t maxMessageSize = 4096;

template <size_t N>
using Buffer = std::array<char, N>;

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
int32_t writeAll(int fd, const Buffer<N>& buf, size_t n, size_t off = 0) {
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

template<size_t N>
int messageToBuffer(const std::string message, Buffer<N>& buf)
{
	int32_t len = (int32_t)message.length();

	if (len > maxMessageSize) {
		return -1;
	}

	auto* lenBegin{static_cast<char*>(static_cast<void*>(&len))};
	auto* lenEnd{static_cast<char*>(static_cast<void*>(&len)) + sizeof(len)};

	std::copy(lenBegin, lenEnd, buf.data());
	std::copy(message.begin(), message.end(), buf.data() + sizeof(len));

	return 0;
}
