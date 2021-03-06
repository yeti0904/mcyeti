#pragma once
#include <string>
#include <cstdint>
#include <dotproperties.hh>
using std::string;

struct vec3 {
	int16_t x, y, z;
	uint8_t yaw, pitch;
};

struct player {
	int sock;
	string username;
	bool online;
	vec3 pos;
	Properties props;
};