#pragma once

#include <vector>
#include <map>

#include "Types.hpp"

class Game;
class Task;

struct Ship {
	EntityID ship_id;
	Position pos;
	int halite;
	bool dead;

	Task* task;
	double priority;
	Position target;

	// STRATEGY-SPECIFIC
	bool dropping = false;
};

class Player {
public:
	Player() {};
	Player(PlayerID id, Position shipyard_position);

	void Update(int num_ships, int num_dropoffs, int halite, Game* game);

	bool IsDropoff(const Position pos);
	Position ClosestDropoff(const Position pos);
	Ship* ShipAt(const Position pos);

	PlayerID id;
	Position shipyard_position;
	int halite;
	
	std::map<EntityID, Ship*> ships;
	std::vector<Position> dropoffs;

	static void SortByTaskPriority(std::vector<Ship*>& ships);
};