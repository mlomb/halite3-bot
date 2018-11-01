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
	EMPTY,
	GHOST,
	TRANSIENT,
	STATIC
};

struct OptimalPathCell {
	int haliteCost = INF;
	int turns = INF;
	int ships_on_path = 0;
	int tor_dist = 0;
	bool expanded = false;
	bool added = false;

	double ratio() const {
		return (turns * 10000) + (haliteCost * 10) + (100.0 - (double)tor_dist) / 100.0;
	}

	bool operator<(const OptimalPathCell& rhs) const
	{
		return ratio() < rhs.ratio();
	}
};
struct OptimalPathMap {
	OptimalPathCell cells[64][64];
};

struct NavigationOption {
	int option_index;
	double optionCost;
	Position pos;
	Direction direction;
};

class Navigation {
public:
	Navigation(Strategy* strategy);

	void PathMinCostFromMap(Position start, EnemyPolicy policy, OptimalPathMap& map);
	OptimalPathCell PathMinCost(Position start, Position end);

	void Clear();
	bool IsHitFree(const Position pos);
	std::vector<NavigationOption> NavigationOptionsForShip(Ship* s);
	void Navigate(std::vector<Ship*> ships, std::vector<Command>& commands);

	Strategy* strategy;
	Game* game;
	BlockedCell hits[64][64];
	std::map<Position, OptimalPathMap> minCostCache;
};