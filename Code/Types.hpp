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

#include "Constants.hpp"

class Ship;

const int INF = 99999999;
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

enum Stage {
	// Most of the game, just mining and avoiding enemies
	MINING,
	// There is no more halite to pick up, try to collision
	// with other turtles and steal their halite
	STEALING,
	// All ships should go to the nearest dropoff and collide
	SUICIDE
};

enum class TaskType {
	NONE = 0,
	MINE = 1,
	ATTACK = 2,
	DROP = 3,
	TRANSFORM_INTO_DROPOFF = 4
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
	EnemyPolicy policy;
	double priority;
	bool override = false;
};

std::mt19937_64& mt();
std::uniform_real_distribution<double> rand_01();
double GetRandom01();
void SetRandom01Seed(long long seed);