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

			cells[y].push_back(Cell { x, y, halite });
		}
	}
}

void Map::Update()
{
	int update_count;
	in::GetSStream() >> update_count;

	for (int i = 0; i < update_count; ++i) {
		int x;
		int y;
		int halite;
		in::GetSStream() >> x >> y >> halite;
		cells[y][x].halite = halite;
	}
}
