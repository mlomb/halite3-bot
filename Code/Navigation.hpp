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

	double ratio() const {
		return (turns * 1000.0) + tor_dist + haliteCost / 10000.0;
	}

	bool operator<(const OptimalPathCell& rhs) const
	{
		return ratio() < rhs.ratio();
	}
};
struct OptimalPathMap {
	OptimalPathCell cells[MAX_MAP_SIZE][MAX_MAP_SIZE];
};

struct NavigationOption {
	double optionCost;
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