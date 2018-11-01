#include "Navigation.hpp"

#include "Game.hpp"
#include "Strategy.hpp"

Navigation::Navigation(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

void Navigation::PathMinCostFromMap(Position start, EnemyPolicy policy, OptimalPathMap& map)
{
	Player& me = game->GetMyPlayer();

	std::queue<Position> q;

	map.cells[start.x][start.y].haliteCost = 0;
	map.cells[start.x][start.y].turns = 0;
	map.cells[start.x][start.y].ships_on_path = 0;
	map.cells[start.x][start.y].tor_dist = 0;
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

		for (const Direction d : DIRECTIONS) {
			Position new_pos = p.DirectionalOffset(d);

			/*
			switch (policy) {
			case IGNORE:
			default:
				break;

			case DODGE:
				if (game->map->GetCell(new_pos)->enemy_reach_halite != -1) {
					continue;
				}
				break;
			}
			*/

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + floor(0.1 * (double)game->map->GetCell(p)->halite);
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.ships_on_path = r.ships_on_path + (me.ShipAt(new_pos) ? 1 : 0);
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			// TODO Sure?
			switch(hits[new_pos.x][new_pos.y]) {
			case BlockedCell::STATIC:
			case BlockedCell::GHOST:
				new_state.turns += 1000;
				break;
			//case BlockedCell::TRANSIENT:
			//	new_state.turns += 4;
			//	break;
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

	PathMinCostFromMap(start, EnemyPolicy::IGNORE, map);
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
			hits[ed.x][ed.y] = BlockedCell::STATIC; // avoid enemy dropoffs
		for (auto& es : enemy_player.ships) {
			if (me.IsDropoff(es.second->pos)) {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::EMPTY;
			}
			else {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::STATIC;
			}
		}
	}
}

bool Navigation::IsHitFree(const Position pos)
{
	return hits[pos.x][pos.y] == BlockedCell::EMPTY ||
		   hits[pos.x][pos.y] == BlockedCell::GHOST;
}

std::vector<NavigationOption> Navigation::NavigationOptionsForShip(Ship* s)
{
	Player& me = game->GetMyPlayer();

	std::vector<NavigationOption> options;

	OptimalPathMap map = {};
	PathMinCostFromMap(s->target, s->policy, map);

	std::vector<Direction> dirs = DIRECTIONS;
	if(!me.IsDropoff(s->pos)) {
		if (s->target.ToroidalDistanceTo(s->pos) <= 1 || strategy->stage == Stage::SUICIDE) { // if target and is not a dropoff
			dirs.push_back(Direction::STILL);
		}
	}

	int option_index = 0;

	for (const Direction d : dirs) {
		Position pp = s->pos.DirectionalOffset(d);
		if (IsHitFree(pp)) {
			double optionCost;
			optionCost = map.cells[pp.x][pp.y].ratio();

			switch (s->policy) {
			case IGNORE:
			default:
				break;

			case DODGE:
				if (s->halite > 150) {
					int rh = game->map->GetCell(pp)->enemy_reach_halite;
					if (rh != -1 && (s->halite >= game->map->GetCell(pp)->enemy_reach_halite || s->halite >= 650)) {
						optionCost = INF;
					}
				}
				break;
			}


			options.push_back({
				option_index++,
				optionCost,
				pp,
				d
			});
		}
	}

	return options;
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_real_distribution<double> random_01(0.0, 1.0);

	// Prevent dropoff deadlock
	for (Position dropoff : game->GetMyPlayer().dropoffs) {
		std::vector<Ship*> ships_near_dropoff; // (with target the dropoff)
		int blocked = 0;
		for (const Direction d : DIRECTIONS) {
			Ship* s = me.ShipAt(dropoff.DirectionalOffset(d));
			if (s) {
				blocked++;
				if(s->target == dropoff)
					ships_near_dropoff.push_back(s);
			}
		}

		if (ships_near_dropoff.size() > 1) {
			Ship* s = strategy->GetShipWithHighestPriority(ships_near_dropoff);
			if (s) {
				s->priority += 1000000000;
			}
		}
		else if (blocked == DIRECTIONS.size()) {
			Ship* s = me.ShipAt(dropoff);
			if(s)
				s->priority += 1000000000;
		}
	}

	// Generate optionsMap with the ideal options for each ship
	std::vector<std::vector<double>> optionsMap(game->map->width, std::vector<double>(game->map->height, 0));

	for (Ship* s : ships) {
		auto options = NavigationOptionsForShip(s);
		for (NavigationOption& no : options) {
			optionsMap[no.pos.x][no.pos.y] += no.optionCost / (no.option_index + 1);
			if(no.option_index == 0 && no.pos == s->target && !me.IsDropoff(no.pos)) {
				hits[no.pos.x][no.pos.y] = BlockedCell::GHOST;
			}
		}
	}

	int navigation_order = 0;

	while (!ships.empty()) {
		// Take the one with the highest priority
		Ship* s = strategy->GetShipWithHighestPriority(ships);
		ships.erase(std::find(ships.begin(), ships.end(), s));

		if (strategy->stage == Stage::SUICIDE) {
			for (Position dropoff : game->GetMyPlayer().dropoffs) {
				hits[dropoff.x][dropoff.y] = BlockedCell::EMPTY;
			}
		}

		// ----- Navigate
		// Generate the new options given the modified map
		auto options = NavigationOptionsForShip(s);
		int salt = game->turn + game->map->halite_remaining;

		std::sort(options.begin(), options.end(), [s, &salt, &optionsMap, &gen, &random_01](const NavigationOption& a, const NavigationOption& b) {
			const double COST_THRESHOLD = 700;
			if (fabs(a.optionCost - b.optionCost) <= COST_THRESHOLD) { // almost equal
				double mpA = optionsMap[a.pos.x][a.pos.y] - a.optionCost / (a.option_index + 1);
				double mpB = optionsMap[b.pos.x][b.pos.y] - b.optionCost / (b.option_index + 1);
				if (fabs(mpA - mpB) <= COST_THRESHOLD) {
					gen.seed(salt + s->ship_id * (a.option_index+1) * (b.option_index+1));
					return random_01(gen) >= 0.5;
				}
				else
					return mpA < mpB;
			} else
				return a.optionCost < b.optionCost;
		});

		if (options.size() == 0) {
			out::Log("No NavigationOption found for " + std::to_string(s->ship_id) + ".");
			options.push_back({
				0,
				0,
				s->pos,
				Direction::STILL
			});
		}
		NavigationOption& option = *options.begin();
		hits[option.pos.x][option.pos.y] = (option.direction == Direction::STILL || option.pos == s->target) && !me.IsDropoff(s->target) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;
		commands.push_back(MoveCommand(s->ship_id, option.direction));
		
		// Collision chain
		Ship* shipInMovedLocation = me.ShipAt(option.pos);
		if (shipInMovedLocation) {
			if (std::find(ships.begin(), ships.end(), shipInMovedLocation) != ships.end()) {
				// this ship should be the next to be processed
				shipInMovedLocation->priority += 100000000000;
			}
		}
		
		/*
#ifdef HALITE_LOCAL
		double optionMap[64][64];
		for (int x = 0; x < game->map->width; x++) {
			for (int y = 0; y < game->map->height; y++) {
				optionMap[x][y] = 0;// map.cells[x][y].turns;
			}
		}

		for (const NavigationOption& option : options) {
			optionMap[option.pos.x][option.pos.y] = option.optionCost;
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
		*/
		

#ifdef HALITE_LOCAL
		out::LogShip(s->ship_id, {
			{ "task_type", s->task ? s->task->type : 0 },
			{ "task_x", s->target.x },
			{ "task_y", s->target.y },
			{ "task_priority", std::to_string(s->priority) },
			{ "navigation_order", std::to_string(navigation_order++) },
		});
#endif

	}
}
