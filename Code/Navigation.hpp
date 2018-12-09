#pragma once

#include <vector>
#include <random>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"

class Strategy;

enum BlockedCell {
	EMPTY = 0,
	GHOST = 1,
	TRANSIENT = 2,
	STATIC = 3
};

struct OptimalPathCell {
	int haliteCost = INF;
	int turns = INF;
	int tor_dist = 0;
	bool expanded = false;
	bool added = false;

	long long int cost() const {
		// -10-10-1000-
		return (long long int)turns * 1000000 + (long long int)tor_dist * 10000 + (long long int)haliteCost;
	}

	bool operator<(const OptimalPathCell& rhs) const
	{
		return cost() < rhs.cost();
	}
};
struct OptimalPathMap {
	OptimalPathCell cells[MAX_MAP_SIZE][MAX_MAP_SIZE];
};

struct NavigationOption {
	long long int optionCost;
	Position pos;
	Direction direction;
	Ship* ship;
};

class Navigation {
public:
	Navigation(Strategy* strategy);

	void MinCostBFS(Position start, OptimalPathMap& map);

	std::vector<NavigationOption> NavigationOptionsForShip(Ship* ship);

	void Clear();
	bool IsHitFree(const Position pos);
	void Navigate(std::vector<Ship*> ships, std::vector<Command>& commands);

	Strategy* strategy;
	Game* game;

	BlockedCell hits[MAX_MAP_SIZE][MAX_MAP_SIZE];
};