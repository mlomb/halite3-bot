#pragma once

#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"

class Game;

enum TaskType {
	MINE = 1,
	DROP = 2,
	TRANSFORM_INTO_DROPOFF = 3
};

struct Task {
	int id;
	TaskType type;
	Position pos;
	std::set<Ship*> ships;
	int max_ships;

	bool IsFull() {
		return max_ships != -1 && (int)ships.size() >= max_ships;
	}

	// TRANSFORM_INTO_DROPOFF
	AreaInfo areaInfo;
};

struct OptimalPathResult {
	int haliteCost = INF;
	int turns = INF;
	bool expanded = false;
	bool added = false;
	bool blocked = false;

	bool operator<(const OptimalPathResult& rhs) const
	{
		if (haliteCost == rhs.haliteCost)
			return turns < rhs.turns;
		else
			return haliteCost < rhs.haliteCost;
	}
};
struct OptimalPathMap {
	OptimalPathResult cells[64][64];
};
struct OptimalMiningResult {
	double profit_per_turn;
	int haliteMined;
	int turns;
};

class Strategy {
public:
	Strategy(Game* game);

	void CreateTasks();
	void AssignTasks();
	void Navigate(std::vector<Command>& commands);
	void Execute(std::vector<Command>& commands);

	void PathMinCostFromMap(Position start, OptimalPathMap& map);
	OptimalPathResult PathMinCost(Position start, Position end);
	OptimalMiningResult MineMaxProfit(int shipHalite, int base_haliteCost, int base_turns, int cellHalite);
	double CalculatePriority(Position start, Position destination, int shipHalite);

	Game* game;
	std::vector<Task*> tasks;
	std::map<Position, OptimalPathMap> minCostCache;

	bool suicide_stage;
};