#pragma once

#include <vector>
#include <map>

#include "Types.hpp"

class Game;
class Task;

struct Ship {
	PlayerID player_id;
	EntityID ship_id;
	Position pos;
	int halite;
	bool dead;

	// STRATEGY-SPECIFIC
	ShipTask task;
	bool assigned = false;
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
	Ship* ShipAt(const Position& pos);
	Ship* ClosestShipAt(const Position& pos);


	PlayerID id;
	Position shipyard_position;
	int halite;
	int carrying_halite;
	
	std::map<EntityID, Ship*> ships;
	std::vector<Position> dropoffs;

	static void SortByTaskPriority(std::vector<Ship*>& ships);
	static Ship* GetShipWithHighestPriority(std::vector<Ship*>& ships);
};