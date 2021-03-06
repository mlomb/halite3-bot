#pragma once


#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "Navigation.hpp"


class Game;

class Strategy {
public:
	Strategy(Game* game);

	void Initialize();

	void AssignTasks(std::vector<Command>& commands);
	void Execute(std::vector<Command>& commands);

	double CalcFriendliness(Ship*, Position p);
	std::vector<Position> BestDropoffSpots();
	bool ShouldSpawnShip();

	void FillClosestDropoffDist();

	Game* game;
	Navigation* navigation;

	bool allow_dropoff_collision;
	int reserved_halite;

	std::vector<Ship*> shipsToNavigate;
	int closestDropoffDist[MAX_MAP_SIZE][MAX_MAP_SIZE];
};