#pragma once

#include <vector>
#include <queue>
#include <set>

/*
#include <dlib/svm.h>
using namespace dlib;
*/

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "Navigation.hpp"

class Game;

enum Stage {
	// Most of the game, just mining and avoiding enemies
	MINING,
	// There is no more halite to pick up, try to collision
	// with other turtles and steal their halite
	STEALING,
	// All ships should go to the nearest dropoff and collide
	SUICIDE
};

enum TaskType {
	MINE = 1,
	DROP = 2,
	TRANSFORM_INTO_DROPOFF = 3
};

struct Task {
	TaskType type;
	Position pos;

	// Misc --
	bool assigned = false;
	int dist_to_dropoff;
};

class Strategy {
public:
	Strategy(Game* game);

	void CreateTasks();
	void AssignTasks();
	void Execute(std::vector<Command>& commands);

	double ShipTaskPriority(Ship* s, Task* t);
	bool ShouldSpawnShip();

	Ship* GetShipWithHighestPriority(std::vector<Ship*>& ships);

	Game* game;
	Navigation* navigation;
	std::vector<Task*> tasks;
	std::vector<Ship*> shipsAvailable;
	std::map<Position, OptimalPathMap> minCostCache;

	Stage stage;
};