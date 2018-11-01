#pragma once

#include <vector>

#include "Types.hpp"

class Game;
class Ship;

struct AreaInfo {
	int halite;
	double avgHalite;
	int num_ally_ships;
	int num_enemy_ships;
};

struct Cell {
	Position pos;
	int halite;
	bool inspiration;
	AreaInfo near_info;
	Ship* ship_on_cell;
	int enemy_reach_halite;
};

class Map {
public:
	Map(Game* game);

	void Initialize();
	void Update();

	Cell* GetCell(Position pos);
	AreaInfo GetAreaInfo(Position p, int max_manhattan_distance);

	Game* game;
	int width, height;
	int halite_remaining;
	double map_avg_halite;
	std::vector<std::vector<Cell*>> cells;
};