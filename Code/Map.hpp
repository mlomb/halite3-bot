#pragma once

#include <vector>

#include "Types.hpp"

class Game;

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
	std::vector<std::vector<Cell*>> cells;
};