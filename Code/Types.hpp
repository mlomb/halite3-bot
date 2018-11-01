#pragma once

#include <string>
#include <vector>
#include <stdlib.h>
#include <algorithm>

const int INF = 99999999;

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

enum EnemyPolicy {
	IGNORE,
	DODGE,
	ENGAGE
};

const std::vector<Direction> DIRECTIONS = {
	Direction::EAST,
	Direction::WEST,
	Direction::NORTH,
	Direction::SOUTH
};

struct Position {
	int x, y;

	Position() : x(0), y(0) {}
	Position(int x, int y) : x(x), y(y) { Wrap(); }

	bool operator==(const Position& other) const { return x == other.x && y == other.y; }
	bool operator!=(const Position& other) const { return x != other.x || y != other.y; }
	bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
	
	std::string str() const { return "{ " + std::to_string(x) + ", " + std::to_string(y) + " }"; }

	Position DirectionalOffset(const Direction d) const;
	int DistanceTo(const Position& other) const;
	int ToroidalDistanceTo(const Position& other) const;
	void Wrap();
};