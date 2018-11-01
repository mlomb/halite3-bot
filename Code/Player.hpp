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

	double priority;
	Position target;
	EnemyPolicy policy;

	// STRATEGY-SPECIFIC
	Task* task;
	bool dropping = false;
};

class Player {
public:
	Player() {};
	Player(PlayerID id, Position shipyard_position);

	void Update(int num_ships, int num_dropoffs, int halite, Game* game);

	int TotalHalite();
	bool IsDropoff(const Position pos);
	Position ClosestDropoff(const Position pos);
	int DistanceToClosestDropoff(const Position pos);
	Ship* ShipAt(const Position pos);

	PlayerID id;
	Position shipyard_position;
	int halite;
	int carrying_halite;
	
	std::map<EntityID, Ship*> ships;
	std::vector<Position> dropoffs;

	static void SortByTaskPriority(std::vector<Ship*>& ships);
};