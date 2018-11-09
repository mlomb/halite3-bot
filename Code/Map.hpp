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
	int num_ally_ships_not_dropping;

	std::vector<int> enemy_ships_dist;
	std::vector<int> ally_ships_not_dropping_dist;
};

struct Cell {
	Position pos;
	int halite;
	bool inspiration;
	AreaInfo near_info_2;
	AreaInfo near_info_3;
	AreaInfo near_info_4;
	Ship* ship_on_cell;
	int enemy_reach_halite; // min
	PlayerID dropoff_owned;
};

class Map {
public:
	Map(Game* game);

	void Initialize();
	void Update();

	inline Cell* GetCell(const Position& pos) {
#ifdef HALITE_LOCAL
		/*
		if (pos.x < 0 || pos.y < 0 || pos.x >= width || pos.y >= height) {
		out::Log("GetCell out of bounds: " + pos.str() + " -- Crash incoming");
		}
		*/
#endif
		return cells[pos.y][pos.x];
	};
	AreaInfo GetAreaInfo(Position p, int max_manhattan_distance);

	Game* game;
	int width, height;
	int halite_remaining;
	double map_avg_halite;
	std::vector<std::vector<Cell*>> cells;
};