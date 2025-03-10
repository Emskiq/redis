#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <vector>
#include <string>
#include <map>

#include <string.h>

#include "HashTable.h"

// TODO: Rework to be C++ friendly
enum {
	RES_OK = 0,
	RES_ERR = 1,
	RES_NX = 2,
};

// The data structure for the key space. This is just a placeholder
// until we implement a hashtable in the next chapter.
// static std::map<std::string, std::string> g_map;

static HashTable<std::string> g_map{1000};


static uint32_t doGet(const std::vector<std::string> &cmd, char *res, uint32_t *reslen)
{
	if (!g_map.contains(cmd[1])) {
		return RES_NX;
	}
	const std::string &val = g_map[cmd[1]];
	assert(val.size() <= maxMessageSize);
	std::memcpy(res, val.data(), val.size());
	*reslen = (uint32_t)val.size();
	return RES_OK;
}

static uint32_t doSet(const std::vector<std::string> &cmd, char *res, uint32_t *reslen)
{
	(void)res;
	(void)reslen;
	g_map.insert(cmd[1], cmd[2]);
	// g_map[cmd[1]] = cmd[2];
	return RES_OK;
}

static uint32_t doDel(const std::vector<std::string> &cmd, char *res, uint32_t *reslen)
{
	(void)res;
	(void)reslen;
	g_map.remove(cmd[1]);
	return RES_OK;
}

static bool cmdIs(const std::string &word, const char *cmd) {
	return 0 == strcasecmp(word.c_str(), cmd);
}
