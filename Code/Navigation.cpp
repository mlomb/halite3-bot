#include "Navigation.hpp"

#include "Game.hpp"
#include "Strategy.hpp"

Navigation::Navigation(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

void Navigation::PathMinCostFromMap(Position start, OptimalPathMap& map)
{
	std::queue<Position> q;

	map.cells[start.x][start.y].haliteCost = 0;
	map.cells[start.x][start.y].turns = 0;
	map.cells[start.x][start.y].expanded = false;
	map.cells[start.x][start.y].added = false;
	q.push(start);

	while (!q.empty()) {
		Position p = q.front();
		q.pop();

		OptimalPathCell& r = map.cells[p.x][p.y];

		r.added = false;
		if (r.expanded) continue;
		r.expanded = true;

		const std::vector<Direction> dirs = {
			Direction::EAST,
			Direction::WEST,
			Direction::NORTH,
			Direction::SOUTH,
		};

		for (const Direction d : dirs) {
			Position new_pos = p.DirectionalOffset(d);

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + floor(0.1 * game->map->GetCell(p)->halite);
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;

			// TODO Sure?
			switch(hits[new_pos.x][new_pos.y]) {
			case BlockedCell::FIXED:
				new_state.turns += 4;
				break;
			}

			if (new_state < map.cells[new_pos.x][new_pos.y]) {
				if (!map.cells[new_pos.x][new_pos.y].added)
					q.push(new_pos);
				map.cells[new_pos.x][new_pos.y] = new_state;
			}
		}
	}
}

OptimalPathCell Navigation::PathMinCost(Position start, Position end)
{
	auto it = minCostCache.find(start);
	if (it != minCostCache.end()) {
		return (*it).second.cells[end.x][end.y];
	}

	// not cached
	OptimalPathMap map = {};

	PathMinCostFromMap(start, map);
	minCostCache.emplace(start, map);

	return map.cells[end.x][end.y];
}

void Navigation::Clear()
{
	Player& me = game->GetMyPlayer();

	minCostCache.clear();

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			hits[x][y] = BlockedCell::EMPTY;
		}
	}

	// fill hit map with enemies
	for (auto& ep : game->players) {
		Player& enemy_player = ep.second;
		if (enemy_player.id == game->my_id) continue;

		for (Position ed : enemy_player.dropoffs)
			hits[ed.x][ed.y] = BlockedCell::FIXED; // avoid enemy dropoffs
		for (auto& es : enemy_player.ships) {
			if (me.IsDropoff(es.second->pos)) {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::EMPTY;
			}
			else {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::FIXED;
			}
		}
	}
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();

	for (Position dropoff : game->GetMyPlayer().dropoffs) {
		std::vector<Ship*> ships_near_dropoff; // (with target the dropoff)
		for (const Direction d : DIRECTIONS) {
			Ship* s = me.ShipAt(dropoff.DirectionalOffset(d));
			if (s && s->target == dropoff)
				ships_near_dropoff.push_back(s);
		}

		if (ships_near_dropoff.size() > 1) {
			Ship* s = strategy->GetShipWithHighestPriority(ships_near_dropoff);
			if (s) {
				s->priority += 1000000000;
			}
		}
	}

	int navigation_order = 0;

	while (!ships.empty()) {
		// Take the one with the highest priority
		Ship* s = strategy->GetShipWithHighestPriority(ships);
		ships.erase(std::find(ships.begin(), ships.end(), s));


		if (strategy->suicide_stage) {
			for (Position dropoff : game->GetMyPlayer().dropoffs) {
				hits[dropoff.x][dropoff.y] = BlockedCell::EMPTY;
			}
		}

		// ----- Navigate
		Position target = s->target;
		int target_dist = s->pos.ToroidalDistanceTo(target);

		OptimalPathMap map = {};
		PathMinCostFromMap(target, map);

		struct Option {
			double optionCost;
			Direction direction;
		};

		std::vector<Option> options;

		std::vector<Direction> dirs = DIRECTIONS;
		if (!me.IsDropoff(s->pos)) {
			dirs.push_back(Direction::STILL);
		}

		for (const Direction d : dirs) {
			Position pp = s->pos.DirectionalOffset(d);
			double optionCost = 999999999;
			if (hits[pp.x][pp.y] == BlockedCell::EMPTY) {
				if (strategy->suicide_stage) {
					optionCost = map.cells[pp.x][pp.y].turns * 10000 + map.cells[pp.x][pp.y].haliteCost;
				}
				else {
					optionCost = map.cells[pp.x][pp.y].ratio();
				}
			}

			options.push_back(Option{
				optionCost,
				d
			});
		}

		std::sort(options.begin(), options.end(), [](const Option& a, const Option& b) {
			return a.optionCost < b.optionCost;
		});


		Direction d = Direction::STILL;
		for (const Option& option : options) {
			auto mov_pos = s->pos.DirectionalOffset(option.direction);

			if (hits[mov_pos.x][mov_pos.y] == BlockedCell::EMPTY) {
				d = option.direction;
				break;
			}
		}

#ifdef HALITE_LOCAL
		double optionMap[64][64];
		for (int x = 0; x < game->map->width; x++) {
			for (int y = 0; y < game->map->height; y++) {
				optionMap[x][y] = map.cells[x][y].turns;
			}
		}

		for (const Option& option : options) {
			//auto mov_pos = s->pos.DirectionalOffset(option.direction);
			//optionMap[mov_pos.x][mov_pos.y] = hits[mov_pos.x][mov_pos.y] == BlockedCell::EMPTY ? map.cells[mov_pos.x][mov_pos.y].turns : -3;
		}
		json data_map;
		for (int y = 0; y < game->map->height; y++) {
			json data_row;
			for (int x = 0; x < game->map->width; x++) {
				data_row.push_back(optionMap[x][y]);
			}
			data_map.push_back(data_row);
		}
		out::LogFluorineDebug({
			{ "type", "priority" },
			{ "position_x", s->pos.x },
			{ "position_y", s->pos.y }
			}, data_map);
#endif

		auto mov_pos = s->pos.DirectionalOffset(d);
		hits[mov_pos.x][mov_pos.y] = mov_pos == s->target && !me.IsDropoff(s->target) ? BlockedCell::FIXED : BlockedCell::TRANSIENT;

		// Collision chain
		Ship* shipInMovedLocation = me.ShipAt(mov_pos);
		if (shipInMovedLocation) {
			if(std::find(ships.begin(), ships.end(), shipInMovedLocation) != ships.end()) {
				// this ship should be the next to be processed
				shipInMovedLocation->priority += 100000000000;
			}
		}

#ifdef HALITE_LOCAL
		out::LogShip(s->ship_id, {
			{ "task_type", s->task ? s->task->type : 0 },
			{ "task_x", s->target.x },
			{ "task_y", s->target.y },
			{ "task_priority", std::to_string(s->priority) },
			{ "navigation_order", std::to_string(navigation_order) },
		});
#endif

		commands.push_back(MoveCommand(s->ship_id, d));
		navigation_order++;
	}
}
