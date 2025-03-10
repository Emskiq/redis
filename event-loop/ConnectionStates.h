#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <unistd.h>

#include "Connection.h"
#include "Commands.h"
#include "Utils.h"

static bool tryFillingTheBuffer(Connection *conn);
static bool tryFlushingTheBuffer(Connection *conn);

static void stateRequest(Connection *conn);
static void stateResponse(Connection *conn);

static bool tryOneRequest(Connection *conn);
static int32_t doRequest(const char *req, uint32_t reqlen, uint32_t *rescode, char *res, uint32_t *reslen);
static int32_t parseReq(const char *data, size_t len, std::vector<std::string> &out);

// The state machine
static void connectionIO(Connection *conn) {
	if (conn->state == ConnectionState::Request) {
		stateRequest(conn);
	} else if (conn->state == ConnectionState::Response) {
		stateResponse(conn);
	} else {
		assert(false);  // not expected
	}
}

static void stateRequest(Connection *conn) {
	while (tryFillingTheBuffer(conn)) {
		// Do nothing
	}
}

static void stateResponse(Connection *conn) {
	while (tryFlushingTheBuffer(conn)) {}
}

static bool tryFillingTheBuffer(Connection *conn) {
	// try to fill the buffer
	assert(conn->rbufSize < sizeof(conn->rbuf));
	ssize_t rv = 0;
	do {
		const size_t cap{conn->rbuf.size() - conn->rbufSize};
		rv = read(conn->fd, &conn->rbuf[conn->rbufSize], cap);
	} while (rv < 0 && errno == EINTR);

	if (rv < 0 && errno == EAGAIN) {
		// got EAGAIN, stop.
		return false;
	}
	if (rv < 0) {
		msg("read() error");
		conn->state = ConnectionState::End;
		return false;
	}
	if (rv == 0) {
		if (conn->rbufSize > 0) {
			msg("unexpected EOF");
		} else {
			msg("EOF");
		}
		conn->state = ConnectionState::End;
		return false;
	}

	conn->rbufSize += (size_t)rv;
	assert(conn->rbufSize <= sizeof(conn->rbuf));

	// Try to process requests one by one.
	// Why is there a loop? Please read the explanation of "pipelining".
	while (tryOneRequest(conn)) {}
	return (conn->state == ConnectionState::Request);
}

static bool tryFlushingTheBuffer(Connection *conn) {
	ssize_t rv = 0;
	do {
		const size_t remain{conn->wbufSize - conn->wbufSent};
		rv = write(conn->fd, &conn->wbuf[conn->wbufSent], remain);
	} while (rv < 0 && errno == EINTR);

	if (rv < 0 && errno == EAGAIN) {
		// got EAGAIN, stop.
		return false;
	}
	if (rv < 0) {
		msg("write() error");
		conn->state = ConnectionState::End;
		return false;
	}
	conn->wbufSent += (size_t)rv;
	assert(conn->wbufSent <= conn->wbufSize);
	if (conn->wbufSent == conn->wbufSize) {
		// response was fully sent, change state back
		conn->state = ConnectionState::Request;
		conn->wbufSent = 0;
		conn->wbufSize = 0;
		return false;
	}
	// still got some data in wbuf, could try to write again
	return true;
}

static bool tryOneRequest(Connection *conn) {
	uint32_t len = 0;

	if (!readFromBuf(conn->rbuf, len)) {
		// not enough data in the buffer. Will retry in the next iteration
		return false;
	}

	if (len > maxMessageSize) {
		msg("too long");
		conn->state = ConnectionState::End;
		return false;
	}
	if (4 + len > conn->rbufSize) {
		// not enough data in the buffer. Will retry in the next iteration
		return false;
	}

	// got one request, do something with it
	// std::cout << "client says: " << &conn->rbuf[4] << std::endl;
	//
	// // generating echoing response
	// std::string echoMsg{};
	// readFromBuf(conn->rbuf, echoMsg);

	uint32_t rescode = 0;
	uint32_t wlen = 0;
	int32_t err = doRequest(&conn->rbuf[4], len, &rescode, &conn->wbuf[4 + 4], &wlen);
	if (err) {
		conn->state = ConnectionState::End;
		return false;
	}
	wlen += 4;
	writeToBuf(wlen, conn->wbuf);
	writeToBuf(rescode, conn->wbuf, 4);

	conn->wbufSize = 4 + wlen;

	const size_t remain = conn->rbufSize - 4 - len;
	if (remain) {
		advanceBuf(conn->rbuf, 4 + len, remain);
	}
	conn->rbufSize = remain;

	// change state
	conn->state = ConnectionState::Response;
	stateResponse(conn);

	// continue the outer loop if the request was fully processed
	return (conn->state == ConnectionState::Request);
}

// TODO: Rework
static int32_t doRequest(const char *req, uint32_t reqlen, uint32_t *rescode, char *res, uint32_t *reslen)
{
	std::vector<std::string> cmd;
	if (0 != parseReq(req, reqlen, cmd)) {
		msg("bad req");
		return -1;
	}

	std::cerr << "Received commands: ";
	printVec(cmd);
	std::cerr << std::endl;

	if (cmd.size() == 2 && cmdIs(cmd[0], "get")) {
		*rescode = doGet(cmd, res, reslen);
	} else if (cmd.size() == 3 && cmdIs(cmd[0], "set")) {
		*rescode = doSet(cmd, res, reslen);
	} else if (cmd.size() == 2 && cmdIs(cmd[0], "del")) {
		*rescode = doDel(cmd, res, reslen);
	} else {
		// cmd is not recognized
		std::cerr << "UNKNSOW COMMANF\n";
		*rescode = RES_ERR;
		const std::string msg = "Unknown cmd";
		strcpy(res, msg.data());
		*reslen = msg.length();
	}

	return 0;
}

static int32_t parseReq(const char *data, size_t len, std::vector<std::string> &out)
{
	if (len < 4) {
		return -1;
	}
	uint32_t n = 0;
	memcpy(&n, &data[0], 4);
	if (n > maxMessageArgs) {
		return -1;
	}

	size_t pos = 4;
	while (n--) {
		if (pos + 4 > len) {
			return -1;
		}
		uint32_t sz = 0;
		memcpy(&sz, &data[pos], 4);
		if (pos + 4 + sz > len) {
			return -1;
		}
		out.push_back(std::string(&data[pos + 4], sz));
		pos += 4 + sz;
	}

	if (pos != len) {
		return -1;  // trailing garbage
	}

	return 0;
}
