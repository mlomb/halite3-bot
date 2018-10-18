#include "Map.hpp"

#include "IO.hpp"

Map::Map()
{
}

void Map::Initialize()
{
	in::GetSStream() >> width >> height;
	
	cells.resize(height);
	for (int y = 0; y < height; y++) {
		auto in = in::GetSStream();

		cells[y].resize(width);
		for (int x = 0; x < width; x++) {
			int halite;
			in >> halite;

			Cell* c = new Cell();
			c->pos = { x, y };
			c->halite = halite;

			cells[y][x] = c;
		}
	}
}

void Map::Update()
{
	int update_count;
	in::GetSStream() >> update_count;

	for (int i = 0; i < update_count; ++i) {
		Position pos;
		int halite;
		in::GetSStream() >> pos.x >> pos.y >> halite;
		GetCell(pos)->halite = halite;
	}
}

Cell* Map::GetCell(Position pos)
{
#ifdef DEBUG
	if (pos.x < 0 || pos.y < 0 || pos.x >= width || pos.y >= height) {
		out::Log("GetCell out of bounds: " + pos.str() + " -- Crash incoming");
	}
#endif
	return cells[pos.y][pos.x];
}
