#pragma once

#include <vector>

struct Cell {
	int x, y;
	int halite;
};

class Map {
public:
	Map();

	void Initialize();
	void Update();

	int width, height;
	std::vector<std::vector<Cell>> cells;
};