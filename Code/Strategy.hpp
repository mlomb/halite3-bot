#pragma once

#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "Navigation.hpp"

class Game;

enum TaskType {
	MINE = 1,
	DROP = 2,
	TRANSFORM_INTO_DROPOFF = 3
};

struct Task {
	TaskType type;
	Position pos;

	// TRANSFORM_INTO_DROPOFF
	AreaInfo areaInfo;

	// Misc --
	int dist_to_dropoff;
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
	void Execute(std::vector<Command>& commands);

	OptimalMiningResult MineMaxProfit(int shipHalite, int base_haliteCost, int base_turns, int cellHalite, bool cellInspired);
	double CalculatePriority(Position start, Position destination, int shipHalite);
	double ShipTaskPriority(Ship* s, Task* t);

	Ship* GetShipWithHighestPriority(std::vector<Ship*>& ships);

	Game* game;
	Navigation* navigation;
	std::vector<Task*> tasks;
	std::vector<Ship*> shipsAvailable;
	std::map<Position, OptimalPathMap> minCostCache;

	bool suicide_stage;
};