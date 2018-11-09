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
	Map* game_map = game->map;

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
			case NONE:
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
			new_state.haliteCost = r.haliteCost + floor(0.1 * (double)game_map->GetCell(p)->halite);
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			//new_state.ships_on_path = r.ships_on_path + (me.ShipAt(new_pos) ? 1 : 0);
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			// TODO Sure?
			switch (hits[new_pos.x][new_pos.y]) {
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

void Navigation::Clear()
{
	Player& me = game->GetMyPlayer();

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			hits[x][y] = BlockedCell::EMPTY;
			collided[x][y] = false;
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
	Map* game_map = game->map;

	Position target = s->task.position;
	EnemyPolicy policy = s->task.policy;

	switch (s->task.type) {
	case TaskType::NONE:
		target = s->pos;
		break;
	}
	if (strategy->closestDropoffDist[s->pos.x][s->pos.y] <= 1) {
		policy = EnemyPolicy::NONE;
	}

	std::vector<NavigationOption> options;

	OptimalPathMap map = {};
	PathMinCostFromMap(target, policy, map);

	std::vector<Direction> dirs = DIRECTIONS;
	if (!me.IsDropoff(s->pos) || strategy->allow_dropoff_collision) {
		//if (target.ToroidalDistanceTo(s->pos) <= 1 || strategy->stage == Stage::SUICIDE) { // if target and is not a dropoff
			dirs.push_back(Direction::STILL);
		//}
	}

	int option_index = 0;

	for (const Direction d : dirs) {
		Position pp = s->pos.DirectionalOffset(d);

		if (collided[pp.x][pp.y])
			continue;

		Cell* c = game_map->GetCell(target);
		Cell* moving_cell = game_map->GetCell(pp);
		bool hit_free = IsHitFree(pp);
		Ship* other_ship = moving_cell->ship_on_cell;
		bool is_other_enemy = other_ship && other_ship->player_id != me.id;
		AreaInfo& info_3 = c->near_info_3;
		AreaInfo& moving_cell_info = moving_cell->near_info_2;

		double optionCost = map.cells[pp.x][pp.y].ratio();
		bool possibleOption = false;

		int dist_2nd_enemy = moving_cell_info.enemy_ships_dist.size() >= 2 ? moving_cell_info.enemy_ships_dist[1] : INF;
		int dist_2nd_ally = moving_cell_info.ally_ships_not_dropping_dist.size() >= 2 ? moving_cell_info.ally_ships_not_dropping_dist[1] : INF;

		if (hit_free) {
			// Need to dodge?
			/*
			if (policy == EnemyPolicy::DODGE) {
				// (the enemy ship to us)
				int rh = moving_cell->enemy_reach_halite;
				if (rh != -1) {
					if (ShouldAttack(rh, info_3.num_enemy_ships, s->halite + c->halite, info_3.num_ally_ships) ||
						(3 * s->halite > rh && s->halite >= 450)) {
						optionCost = INF;
					}
				}
			}

			*/
			possibleOption = true;
			if (moving_cell->enemy_reach_halite != -1) {
				if (policy == EnemyPolicy::DODGE) {
					optionCost += INF + (hlt::constants::MAX_HALITE - moving_cell->enemy_reach_halite);
				}
				else if (policy == EnemyPolicy::ENGAGE) {
					// si la nave mas cercana enemiga esta mas cerca que la nave mas cercana aliada don't go.
					if (moving_cell_info.num_enemy_ships > moving_cell_info.num_ally_ships_not_dropping) {
						optionCost += INF + (hlt::constants::MAX_HALITE - moving_cell->enemy_reach_halite);
					}
					if (dist_2nd_enemy <= dist_2nd_ally) {
						optionCost += INF + (hlt::constants::MAX_HALITE - moving_cell->enemy_reach_halite);
					}
					if (moving_cell_info.num_enemy_ships == moving_cell_info.num_ally_ships_not_dropping) {

					}
				}
			}
		}
		else {
			if (is_other_enemy && policy == EnemyPolicy::ENGAGE) {
				//if () { // if the target is to collide with that ship ( cost = 1 )
				//
				//}

				/*
				if (ShouldAttack(s->halite, info_3.num_ally_ships, other_ship->halite + c->halite, info_3.num_enemy_ships)) {
				}
				*/
				if (moving_cell_info.num_ally_ships_not_dropping > moving_cell_info.num_enemy_ships + 1 &&
					dist_2nd_enemy >= dist_2nd_ally &&
					s->halite < 750 &&
					(other_ship->halite > 500)) {
					optionCost = 1;
					possibleOption = true;
				}
				//if (moving_cell_info.num_ally_ships_not_dropping > moving_cell_info.num_enemy_ships + 1) {
				//	optionCost = 1;
				//	possibleOption = true;
				//}
			}
		}

		if (possibleOption) {
			options.push_back({
				option_index++,
				optionCost,
				optionCost / (double)(option_index + 1),
				pp,
				d,
				});
		}
	}

	if (options.size() == 0) {
		out::Log("No NavigationOption found for " + std::to_string(s->ship_id) + ".");
		options.push_back({
			0,
			0,
			0,
			s->pos,
			Direction::STILL,
			});
	}

	return options;
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	if (ships.size() == 0) return;

	out::Log("Navigating...");
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();
	std::uniform_real_distribution<double> random_01(0.0, 1.0);
	std::mt19937_64& mersenne_twister = mt();
	int navigation_order = 0;

	std::vector<Ship*>::iterator it = ships.begin();
	while (it != ships.end()) {
		Ship* s = *it;
		
		if (s->halite < floor(game->map->GetCell(s->pos)->halite * 0.1)) {
			hits[s->pos.x][s->pos.y] = BlockedCell::TRANSIENT;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			it = ships.erase(it);

#ifdef HALITE_LOCAL
			out::LogShip(s->ship_id, {
				{ "task_type", s->task.type },
				{ "task_x", s->task.position.x },
				{ "task_y", s->task.position.y },
				{ "task_priority", s->task.priority },
				{ "navigation_order", navigation_order++ },
				});
#endif
			continue;
		}
		++it;
	}

	// Prevent dropoff deadlock
	for (Position dropoff : game->GetMyPlayer().dropoffs) {
		std::vector<Ship*> ships_near_dropoff; // (with target the dropoff)
		int blocked = 0;
		for (const Direction d : DIRECTIONS) {
			Ship* s = me.ShipAt(dropoff.DirectionalOffset(d));
			if (s) {
				blocked++;
				if (s->task.position == dropoff)
					ships_near_dropoff.push_back(s);
			}
		}

		if (ships_near_dropoff.size() > 1) {
			Ship* s = strategy->GetShipWithHighestPriority(ships_near_dropoff);
			if (s)
				s->task.type = TaskType::OVERRIDE;
		}
		else if (blocked == DIRECTIONS.size()) {
			Ship* s = me.ShipAt(dropoff);
			if (s)
				s->task.type = TaskType::OVERRIDE;
		}
	}

	// sort, just why not
	strategy->GetShipWithHighestPriority(ships);

	struct NavigationCell {
		double costRank = 0;

		void operator+=(const NavigationOption& no) {
			costRank += no.optionCostByRank;
		}
		void operator-=(const NavigationOption& no) {
			costRank -= no.optionCostByRank;
		}
	};
	NavigationCell navigationMap[64][64];

	// ideal options
	for (Ship* s : ships) {
		auto idealOptions = NavigationOptionsForShip(s);
		for (NavigationOption& no : idealOptions) {
			navigationMap[no.pos.x][no.pos.y] += no;
			if (no.option_index == 0 && no.pos == s->task.position && !me.IsDropoff(no.pos)) {
				hits[no.pos.x][no.pos.y] = BlockedCell::GHOST;
			}
		}
	}
	
	while (!ships.empty()) {
		// Take the one with the highest priority
		Ship* s = strategy->GetShipWithHighestPriority(ships);
		ships.erase(std::find(ships.begin(), ships.end(), s));

		if (strategy->allow_dropoff_collision) {
			for (Position dropoff : game->GetMyPlayer().dropoffs) {
				hits[dropoff.x][dropoff.y] = BlockedCell::EMPTY;
			}
		}

		// ----- Navigate
		// Generate the new options given the modified map
		auto options = NavigationOptionsForShip(s);
		int salt = game->turn + game->map->halite_remaining;

		std::sort(options.begin(), options.end(), [s, &salt, &navigationMap, &mersenne_twister, &random_01](const NavigationOption& a, const NavigationOption& b) {
			if (fabs(a.optionCost - b.optionCost) <= 700) { // almost equal
				double mpA = navigationMap[a.pos.x][a.pos.y].costRank - a.optionCostByRank;
				double mpB = navigationMap[b.pos.x][b.pos.y].costRank - b.optionCostByRank;

				if (fabs(mpA - mpB) <= 200) {
					mt().seed(salt + s->ship_id * (a.option_index + 1) * (b.option_index + 1));
					return random_01(mersenne_twister) >= 0.5;
				}
				else
					return mpA < mpB;
			}
			else
				return a.optionCost < b.optionCost;
		});

		NavigationOption& option = *options.begin();
		hits[option.pos.x][option.pos.y] = (option.direction == Direction::STILL || option.pos == s->task.position) && !me.IsDropoff(s->task.position) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;
		navigationMap[option.pos.x][option.pos.y] -= option;
		commands.push_back(MoveCommand(s->ship_id, option.direction));

		// Collision chain
		Ship* shipInMovedLocation = game->GetShipAt(option.pos);
		if (shipInMovedLocation) {
			if (shipInMovedLocation->player_id == me.id) {
				// this ship should be the next to be processed to avoid possible self collisions
				shipInMovedLocation->task.type = TaskType::OVERRIDE;
			}
			else {
				collided[option.pos.x][option.pos.y] = true;
			}
		}

#ifdef HALITE_LOCAL
		out::LogShip(s->ship_id, {
			{ "task_type", s->task.type },
			{ "task_x", s->task.position.x },
			{ "task_y", s->task.position.y },
			{ "task_priority", s->task.priority },
			{ "navigation_order", std::to_string(navigation_order++) },
		});
#endif
	}
}
