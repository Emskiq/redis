#pragma once

#include <array>
#include <vector>
#include <memory>

static constexpr size_t maxMessageSize = 4096;
static constexpr size_t maxMessageArgs = 4096;


template <size_t N>
using Buffer = std::array<char, N>;

using VariableSizedBuffer = std::vector<char>;

enum class ConnectionState {
	Request,
	Response,
	End,
};

struct Connection {
	int fd{-1};
	ConnectionState state{ConnectionState::Request};

	size_t wbufSize{0};
	size_t wbufSent{0};
	Buffer<4 + maxMessageSize> wbuf;

	size_t rbufSize{0};
	Buffer<4 + maxMessageSize> rbuf;

	Connection() = default;
	Connection(int fd) : fd{fd} { }
};

using ConnectionsMap = std::unordered_map<int, Connection*>;
