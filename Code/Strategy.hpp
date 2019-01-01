#pragma once


#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "Navigation.hpp"
#include "Combat.hpp"

class Game;

class Strategy {
public:
	Strategy(Game* game);

	void Initialize();

	void AssignTasks(std::vector<Command>& commands);
	void Execute(std::vector<Command>& commands);

	std::vector<Position> BestDropoffSpots();
	bool ShouldSpawnShip();

	void FillClosestDropoffDist();
	void SimulateMining(int& cell_halite, bool inspired, int ship_halite, int& halite_mined, int& turns);

	Game* game;
	Navigation* navigation;
	Combat* combat;

	bool allow_dropoff_collision;
	int reserved_halite;

	std::vector<Ship*> shipsToNavigate;
	int closestDropoffDist[MAX_MAP_SIZE][MAX_MAP_SIZE];
};