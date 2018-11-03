#pragma once

#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "Navigation.hpp"

class Game;

struct Task {
	Position position;
	TaskType type;
	EnemyPolicy policy;

	std::vector<Ship*> assigned;
	int max_ships;
};

class Strategy {
public:
	Strategy(Game* game);

	void GenerateTasks();
	void AssignTasks();
	void Execute(std::vector<Command>& commands);

	double ShipTaskPriority(Ship* s, Task* t);
	Position BestDropoffSpot();
	bool ShouldSpawnShip();

	Ship* GetShipWithHighestPriority(std::vector<Ship*>& ships);

	Game* game;
	Navigation* navigation;

	Stage stage;
	std::vector<Ship*> shipsAvailable;
	std::vector<Task*> tasks;
};