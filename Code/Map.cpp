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

	constants::MAP_WIDTH = width;
	constants::MAP_HEIGHT = height;
	
	for (int y = 0; y < height; y++) {
		auto in = in::GetSStream();
		for (int x = 0; x < width; x++) {
			int halite;
			in >> halite;

			cells[x][y].pos = { x,y };
			cells[x][y].halite = halite;
		}
	}

	Process();
}

void Map::Update()
{
	int update_count;
	in::GetSStream() >> update_count;

	for (int i = 0; i < update_count; ++i) {
		Position pos;
		int halite;
		in::GetSStream() >> pos.x >> pos.y >> halite;
		GetCell(pos).halite = halite;
	}

	Process();
}

void Map::Process()
{
	out::Stopwatch s("Map Process");

	halite_remaining = 0;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			Cell& c = GetCell({ x, y });

			c.ship_on_cell = nullptr;
			c.enemy_reach_halite = -1;
			c.dropoff_owned = -1;

			halite_remaining += c.halite;
		}
	}

	map_avg_halite = halite_remaining / (double)(width * height);

	// fill ship_on_cell and dropoff_owned
	for (auto& pp : game->players) {
		for (auto& ss : pp.second.ships) {
			GetCell(ss.second->pos).ship_on_cell = ss.second;

			if (pp.first != game->my_id) {
				std::vector<Direction> dirs = DIRECTIONS;
				dirs.push_back(Direction::STILL);
				for (Direction d : dirs) {
					Position pd = ss.second->pos.DirectionalOffset(d);
					Cell& c = GetCell(pd);
					if (c.enemy_reach_halite == -1 || ss.second->halite < c.enemy_reach_halite) {
						c.enemy_reach_halite = ss.second->halite;
					}
				}
			}
		}
		for (Position d : pp.second.dropoffs) {
			GetCell(d).dropoff_owned = pp.first;
		}
	}

	// near info
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			Cell& c = GetCell({ x, y });

			CalculateNearInfo(c);
			c.inspiration = c.near_info[constants::INSPIRATION_RADIUS].num_enemy_ships >= constants::INSPIRATION_SHIP_COUNT;
		}
	}
}

void Map::CalculateNearInfo(Cell& c)
{
	PlayerID my_id = Game::Get()->my_id;

	for (int k = 0; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
		AreaInfo& info = c.near_info[k];
		info.halite = 0;
		info.cells = 0;
		info.avgHalite = 0;
		info.num_ally_ships = 0;
		info.num_enemy_ships = 0;
		info.num_ally_ships_not_dropping = 0;
		info.enemy_ships_dist.clear();
		info.ally_ships_not_dropping_dist.clear();
		info.dropoffs_dist.clear();
	}

	for (int xx = -MAX_CELL_NEAR_AREA_INFO; xx <= MAX_CELL_NEAR_AREA_INFO; xx++) {
		for (int yy = -MAX_CELL_NEAR_AREA_INFO; yy <= MAX_CELL_NEAR_AREA_INFO; yy++) {
			Position pos = { c.pos.x + xx, c.pos.y + yy };
			int d = pos.ToroidalDistanceTo(c.pos);
			d = std::min(d, MAX_CELL_NEAR_AREA_INFO);

			Cell& it_cell = GetCell(pos);

			for (int k = d; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
				AreaInfo& info = c.near_info[k];

				info.cells++;
				info.halite += GetCell(pos).halite;


				if (it_cell.ship_on_cell) {
					if (it_cell.ship_on_cell->player_id == my_id) {
						info.num_ally_ships++;
						if (!it_cell.ship_on_cell->dropping) {
							info.num_ally_ships_not_dropping++;
							info.ally_ships_not_dropping_dist.push_back(std::make_pair(d, it_cell.ship_on_cell));
						}
					}
					else {
						info.num_enemy_ships++;
						info.enemy_ships_dist.push_back(std::make_pair(d, it_cell.ship_on_cell));
					}
				}
				if (it_cell.dropoff_owned != -1) {
					info.dropoffs_dist.push_back(std::make_pair(d, it_cell.dropoff_owned));
				}
			}
		}
	}

	for (int k = 0; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
		AreaInfo& info = c.near_info[k];
		info.avgHalite = info.halite / (double)info.cells;
		std::sort(info.ally_ships_not_dropping_dist.begin(), info.ally_ships_not_dropping_dist.end());
		std::sort(info.enemy_ships_dist.begin(), info.enemy_ships_dist.end());
		std::sort(info.dropoffs_dist.begin(), info.dropoffs_dist.end());
	}
}