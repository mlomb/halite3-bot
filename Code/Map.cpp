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

	// calc inspiration & near avg
	halite_remaining = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			Position p = { x, y };
			Cell* c = GetCell(p);

			c->near_info_3 = GetAreaInfo(p, 3);
			c->near_info_4 = GetAreaInfo(p, hlt::constants::INSPIRATION_RADIUS);
			c->inspiration = c->near_info_4.num_enemy_ships >= hlt::constants::INSPIRATION_SHIP_COUNT;
			c->ship_on_cell = nullptr;
			c->enemy_reach_halite = -1;
			c->dropoff_owned = -1;

			halite_remaining += c->halite;
		}
	}

	map_avg_halite = halite_remaining / (double)(width * height);

	std::vector<Direction> dirs = DIRECTIONS;
	dirs.push_back(Direction::STILL);

	// fill ship_on_cell
	for (auto& pp : game->players) {
		for (auto& ss : pp.second.ships) {
			GetCell(ss.second->pos)->ship_on_cell = ss.second;

			if (pp.first != game->my_id) {
				for (Direction d : dirs) {
					Position p = ss.second->pos.DirectionalOffset(d);
					int& rh = GetCell(p)->enemy_reach_halite;
					if (rh == -1 || ss.second->halite < rh) {
						rh = ss.second->halite;
					}
				}
			}
		}
		for (Position& d : pp.second.dropoffs) {
			GetCell(d)->dropoff_owned = pp.first;
		}
	}
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
