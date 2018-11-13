#pragma once

#include <vector>

#include "Types.hpp"

#define MAX_CELL_NEAR_AREA_INFO 5 // inclusive

class Game;
class Ship;

struct AreaInfo {
	int halite;
	int cells;
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
	AreaInfo near_info[MAX_CELL_NEAR_AREA_INFO + 1]; // 0 to MAX_CELL_AREA_INFO
	Ship* ship_on_cell;
	int enemy_reach_halite; // min
	PlayerID dropoff_owned;

	int MoveCost() {
		return std::floor(halite * (1.0 / (double)constants::MOVE_COST_RATIO));
	}
};

class Map {
public:
	Map(Game* game);

	void Initialize();
	void Update();
	void Process();
	void CalculateNearInfo(Cell& c);

	inline Cell& GetCell(const Position& pos) {
#ifdef HALITE_DEBUG
		/*
		if (pos.x < 0 || pos.y < 0 || pos.x >= width || pos.y >= height) {
		out::Log("GetCell out of bounds: " + pos.str() + " -- Crash incoming");
		}
		*/
#endif
		return cells[pos.x][pos.y];
	};

	Game* game;
	int width, height;
	int halite_remaining;
	double map_avg_halite;
	Cell cells[MAX_MAP_SIZE][MAX_MAP_SIZE];
};