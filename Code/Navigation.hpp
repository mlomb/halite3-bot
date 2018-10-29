#pragma once

#include <vector>
#include <queue>
#include <set>

#include "Command.hpp"
#include "Player.hpp"
#include "Map.hpp"

class Strategy;

struct OptimalPathCell {
	int haliteCost = INF;
	int turns = INF;
	bool expanded = false;
	bool added = false;
	bool blocked = false;

	double ratio() const {
		return (turns * 100) + haliteCost;
	}

	bool operator<(const OptimalPathCell& rhs) const
	{
		/*
		if (turns == rhs.turns)
			return haliteCost < rhs.haliteCost;
		else
			return turns < rhs.turns;
		*/
		return ratio() < rhs.ratio();
	}
};
struct OptimalPathMap {
	OptimalPathCell cells[64][64];
};

class Navigation {
public:
	Navigation(Strategy* strategy);

	void PathMinCostFromMap(Position start, OptimalPathMap& map);
	OptimalPathCell PathMinCost(Position start, Position end);

	void Clear();
	void InitMap(OptimalPathMap& map);
	void Navigate(std::vector<Ship*> ships, std::vector<Command>& commands);

	Strategy* strategy;
	Game* game;
	bool hits[64][64];
	std::map<Position, OptimalPathMap> minCostCache;
};