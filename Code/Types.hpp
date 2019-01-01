#pragma once

#include <string>
#include <vector>
#include <random>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <chrono>
#include <bitset>
#include <unordered_set>

#include "Constants.hpp"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

class Ship;

const int INF = 1e9;
const long long int BIG_INF = 1e17;
const int MAX_MAP_SIZE = 64;

typedef int PlayerID;
typedef int EntityID;
typedef std::string Command;

enum class Direction : char {
	NORTH = 'n',
	EAST = 'e',
	SOUTH = 's',
	WEST = 'w',
	STILL = 'o',
};

const std::vector<Direction> DIRECTIONS = {
	Direction::EAST,
	Direction::WEST,
	Direction::NORTH,
	Direction::SOUTH
};

const std::vector<Direction> DIRECTIONS_WITH_STILL = {
	Direction::EAST,
	Direction::WEST,
	Direction::NORTH,
	Direction::SOUTH,
	Direction::STILL
};

enum class TaskType {
	NONE = 0,
	MINE = 1,
	BLOCK_DROPOFF = 2,
	ATTACK = 3,
	DROP = 4,
	TRANSFORM_INTO_DROPOFF = 5
};

enum class EnemyPolicy {
	NONE = 0,
	DODGE = 1,
	ENGAGE = 2
};

struct Position {
	int x, y;

	Position() : x(0), y(0) {}
	Position(int x, int y) : x(x), y(y) { Wrap(); }

	bool operator==(const Position& other) const { return x == other.x && y == other.y; }
	bool operator!=(const Position& other) const { return x != other.x || y != other.y; }
	bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
	
	std::string str() const { return "{ " + std::to_string(x) + ", " + std::to_string(y) + " }"; }

	inline Position DirectionalOffset(const Direction d) const {
		auto dx = 0;
		auto dy = 0;
		switch (d) {
		case Direction::NORTH:
			dy = -1;
			break;
		case Direction::SOUTH:
			dy = 1;
			break;
		case Direction::EAST:
			dx = 1;
			break;
		case Direction::WEST:
			dx = -1;
			break;
		case Direction::STILL:
			// No move
			break;
		}
		return Position(x + dx, y + dy);
	}
	inline int DistanceTo(const Position& other) const {
		return std::abs(x - other.x) + std::abs(y - other.y);
	}
	inline int ToroidalDistanceTo(const Position& other) const {
		int dx = std::abs(x - other.x);
		int dy = std::abs(y - other.y);

		int toroidal_dx = std::min(dx, constants::MAP_WIDTH - dx);
		int toroidal_dy = std::min(dy, constants::MAP_HEIGHT - dy);

		return toroidal_dx + toroidal_dy;
	}
	inline void Wrap() {
		x = (x + constants::MAP_WIDTH) % constants::MAP_WIDTH;
		y = (y + constants::MAP_HEIGHT) % constants::MAP_HEIGHT;
	}
};

struct ShipTask {
	Position position;
	TaskType type;
	double priority;
	bool override = false;
};

std::mt19937_64& mt();
std::uniform_real_distribution<double> rand_01();
double GetRandom01();
void SetRandom01Seed(long long seed);

// https://stackoverflow.com/questions/28409469/all-possible-combinations-bits
template <std::size_t N> bool increase(std::bitset<N>& bs) {
	for (std::size_t i = 0; i != bs.size(); ++i) {
		if (bs.flip(i).test(i) == true) {
			return true;
		}
	}
	return false; // overflow
}
