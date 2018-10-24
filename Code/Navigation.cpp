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

	map.cells[start.x][start.y] = { 0, 1, false, true, false };
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
			new_state.blocked = false;

			if (map.cells[new_pos.x][new_pos.y].blocked) {
				new_state.turns += 100;
				// TODO Sure?
				//continue;
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
	OptimalPathMap map;
	InitMap(map);

	PathMinCostFromMap(start, map);
	minCostCache.emplace(start, map);

	return map.cells[end.x][end.y];
}

void Navigation::Clear()
{
	minCostCache.clear();

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			hits[x][y] = false;
		}
	}

	// fill hit map with enemies
	for (auto& ep : game->players) {
		Player& enemy_player = ep.second;
		if (enemy_player.id == game->my_id) continue;

		for (Position ed : enemy_player.dropoffs)
			hits[ed.x][ed.y] = true;
		for (auto& es : enemy_player.ships)
			hits[es.second->pos.x][es.second->pos.y] = true;
	}
}

void Navigation::InitMap(OptimalPathMap& map)
{
	map = {};

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			map.cells[x][y].blocked = hits[x][y];
		}
	}

	if (strategy->suicide_stage) {
		for (Position dropoff : game->GetMyPlayer().dropoffs) {
			hits[dropoff.x][dropoff.y] = false;
		}
	}
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();

	OptimalPathMap map;

	while (!ships.empty()) {
		// Take the one with the highest priority
		Ship* s = strategy->GetShipWithHighestPriority(ships);
		ships.erase(std::find(ships.begin(), ships.end(), s));

		// ----- Navigate
		Position target = s->target;
		int target_dist = s->pos.ToroidalDistanceTo(target);

		InitMap(map); // clear and reinit map
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
			bool getting_closer = pp.ToroidalDistanceTo(target) <= target_dist;
			double optionCost;
			if (getting_closer) {
				optionCost = map.cells[pp.x][pp.y].haliteCost;
			}
			else {
				optionCost = 99999999;
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

			if (!hits[mov_pos.x][mov_pos.y]) {
				d = option.direction;
				break;
			}
		}

		auto mov_pos = s->pos.DirectionalOffset(d);
		hits[mov_pos.x][mov_pos.y] = true;

		// Collision chain
		Ship* shipInMovedLocation = me.ShipAt(mov_pos);
		if (shipInMovedLocation) {
			if(std::find(ships.begin(), ships.end(), shipInMovedLocation) != ships.end()) {
				// this ship should be the next to be processed
				shipInMovedLocation->priority += 100000;
			}
		}

		commands.push_back(MoveCommand(s->ship_id, d));
	}
}
