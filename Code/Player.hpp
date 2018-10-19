#pragma once

#include <map>

#include "Types.hpp"

class Game;

struct Ship {
	bool dead;
	EntityID ship_id;
	Position pos;
	int halite;

	// STRATEGY-SPECIFIC
	int task_id = 0;
	double task_priority = 0;
	bool dropping = false;

	bool operator()(const Ship* a, const Ship* b) const
	{
		return a->task_priority > b->task_priority;
	}
};

class Player {
public:
	Player() {};
	Player(PlayerID id, Position shipyard_position);

	void Update(int num_ships, int num_dropoffs, int halite, Game* game);

	PlayerID id;
	Position shipyard_position;
	int halite;
	
	std::map<EntityID, Ship*> ships;
};