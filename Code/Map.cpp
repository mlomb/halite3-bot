#include "Map.hpp"

#include "IO.hpp"
#include "Game.hpp"
#include "Player.hpp"

Map::Map(Game* game) : game(game)
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

	// calc inspiration
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			Position p = { x, y };
			Cell* c = GetCell(p);

			int near_enemies = 0;

			for (auto& pp : game->players) {
				if (pp.first != game->my_id) {
					for (auto& sp : pp.second.ships) {
							if (sp.second->pos.ToroidalDistanceTo(p) <= hlt::constants::INSPIRATION_RADIUS) {
								near_enemies++;
							}
						if (near_enemies == hlt::constants::INSPIRATION_SHIP_COUNT) break;
					}
					if (near_enemies == hlt::constants::INSPIRATION_SHIP_COUNT) break;
				}
			}

			c->inspiration = near_enemies >= hlt::constants::INSPIRATION_SHIP_COUNT;
		}
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

AreaInfo Map::GetAreaInfo(Position p, int max_manhattan_distance)
{
	AreaInfo info;

	int affectedCells = 0;
	info.halite = 0;
	for (int xx = -max_manhattan_distance; xx <= max_manhattan_distance; xx++) {
		for (int yy = -max_manhattan_distance; yy <= max_manhattan_distance; yy++) {
			Position pos = { p.x + xx, p.y + yy };
			if (pos.ToroidalDistanceTo(p) <= max_manhattan_distance) {
				affectedCells++;
				info.halite += GetCell(pos)->halite;
			}
		}
	}
	info.avgHalite = (double)info.halite / (double)affectedCells;

	info.num_ally_ships = 0;
	info.num_enemy_ships = 0;
	for (auto& pp : game->players) {
		for (auto& sp : pp.second.ships) {
			if (sp.second->pos.ToroidalDistanceTo(p) <= max_manhattan_distance) {
				if (pp.first == game->my_id)
					info.num_ally_ships++;
				else
					info.num_enemy_ships++;
			}
		}
	}

	return info;
}
