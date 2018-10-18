#pragma once

#include <string>
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

struct Position {
	int x, y;

	Position() : x(0), y(0) {}
	Position(int x, int y) : x(x), y(y) {}

	bool operator==(const Position& other) const { return x == other.x && y == other.y; }
	bool operator!=(const Position& other) const { return x != other.x || y != other.y; }
	bool operator<(const Position& other) const { return x < other.x || (x == other.x && y < other.y); }
	
	std::string str() const { return "{ " + std::to_string(x) + ", " + std::to_string(y) + " }"; }

	Position DirectionalOffset(const Direction d) const {
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
		return Position { x + dx, y + dy };
	}

	int DistanceTo(const Position& other) {
		return std::abs(x - other.x) + std::abs(y - other.y);
	}
	int ToroidalDistanceTo(const Position& other, const int wx, const int wy) {
		int dx = std::abs(x - other.x);
		int dy = std::abs(y - other.y);

		int toroidal_dx = std::min(dx, wx - dx);
		int toroidal_dy = std::min(dy, wy - dy);

		return toroidal_dx + toroidal_dy;
	}
	void Wrap(int wx, int wy) {
		x = (x + wx) % wx;
		y = (y + wy) % wy;
	}
};