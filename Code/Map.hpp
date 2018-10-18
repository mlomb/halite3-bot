#pragma once

#include <vector>

#include "Types.hpp"

struct Cell {
	Position pos;
	int halite;
};

class Map {
public:
	Map();

	void Initialize();
	void Update();

	Cell* GetCell(Position pos);

	int width, height;
	std::vector<std::vector<Cell*>> cells;
};