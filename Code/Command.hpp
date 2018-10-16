#pragma once

#include <string>

#include "Player.hpp"

typedef std::string Command;

enum class Direction : char {
	NORTH = 'n',
	EAST = 'e',
	SOUTH = 's',
	WEST = 'w',
	STILL = 'o',
};

Command SpawnCommand();
Command MoveCommand(EntityID id, Direction direction);