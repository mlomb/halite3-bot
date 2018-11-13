#include "Navigation.hpp"

#include "Game.hpp"
#include "Strategy.hpp"

Navigation::Navigation(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

void Navigation::MinCostBFS(Position start, OptimalPathMap& map)
{
	Player& me = game->GetMyPlayer();
	Map* game_map = game->map;

	std::queue<Position> q;

	map.cells[start.x][start.y].haliteCost = 0;
	map.cells[start.x][start.y].turns = 0;
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

			double cost = floor(0.1 * (double)game_map->GetCell(p).halite);
			if (cost < 15) // free
				cost = 0;

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + cost;
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			if (hits[new_pos.x][new_pos.y] == BlockedCell::STATIC || hits[new_pos.x][new_pos.y] == BlockedCell::GHOST) {
				new_state.turns += 4;
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

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
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
	if (strategy->closestDropoffDist[s->pos.x][s->pos.y] <= 2) {
		policy = EnemyPolicy::NONE;
	}

	std::vector<NavigationOption> options;

	OptimalPathMap map = {};
	MinCostBFS(target, map);

	std::vector<Direction> dirs;
	if (!me.IsDropoff(s->pos) || strategy->allow_dropoff_collision) {
		dirs.push_back(Direction::STILL);
	}
	for (Direction d : DIRECTIONS)
		dirs.push_back(d);

	for (const Direction d : dirs) {
		Position pp = s->pos.DirectionalOffset(d);

		//if (collided[pp.x][pp.y])
		//	continue;

		Cell& c = game_map->GetCell(target);
		Cell& moving_cell = game_map->GetCell(pp);
		bool hit_free = IsHitFree(pp);
		Ship* other_ship = moving_cell.ship_on_cell;
		bool is_other_enemy = other_ship && other_ship->player_id != me.id;
		AreaInfo& info_3 = c.near_info[3];
		AreaInfo& moving_cell_info = moving_cell.near_info[3];

		double optionCost = map.cells[pp.x][pp.y].turns * 100000 + map.cells[pp.x][pp.y].tor_dist / 100.0;
		bool possibleOption = false;

		int dist_2nd_enemy = moving_cell_info.enemy_ships_dist.size() >= 2 ? moving_cell_info.enemy_ships_dist[1] : INF;
		int dist_2nd_ally = moving_cell_info.ally_ships_not_dropping_dist.size() >= 2 ? moving_cell_info.ally_ships_not_dropping_dist[1] : INF;

		if (hit_free) {
			possibleOption = true;
			if (moving_cell.enemy_reach_halite != -1) {
				if (policy == EnemyPolicy::DODGE) {
					optionCost += INF + (constants::MAX_HALITE - moving_cell.enemy_reach_halite);
				}
				else if (policy == EnemyPolicy::ENGAGE) {
					// si la nave mas cercana enemiga esta mas cerca que la nave mas cercana aliada don't go.
					if (moving_cell_info.num_enemy_ships > moving_cell_info.num_ally_ships_not_dropping) {
						optionCost += INF + (constants::MAX_HALITE - moving_cell.enemy_reach_halite);
					}
					if (dist_2nd_enemy <= dist_2nd_ally) {
						optionCost += INF + (constants::MAX_HALITE - moving_cell.enemy_reach_halite);
					}
				}
			}
		}
		else {
			if (is_other_enemy && policy == EnemyPolicy::ENGAGE) {
				if (moving_cell_info.num_ally_ships_not_dropping > moving_cell_info.num_enemy_ships + 1 &&
					dist_2nd_enemy >= dist_2nd_ally &&
					s->halite < 750 &&
					(other_ship->halite > 500)) {
					optionCost = 1;
					possibleOption = true;
				}
			}
		}

		if (possibleOption) {
			if (me.IsDropoff(pp) && strategy->allow_dropoff_collision) {
				optionCost = 0;
			}

			options.push_back({
				0,
				optionCost,
				pp,
				d,
				s
			});
		}
	}

	if (options.size() == 0) {
		out::Log("No NavigationOption found for " + std::to_string(s->ship_id) + ".");
		options.push_back({
			0,
			0,
			s->pos,
			Direction::STILL,
			s
		});
	}

	std::sort(options.begin(), options.end(), [](const NavigationOption& a, const NavigationOption& b) {
		return a.optionCost < b.optionCost;
	});

	int option_index = 1;
	double last = -1;

	//out::Log("### SHIP " + std::to_string(s->ship_id));

	for (NavigationOption& option : options) {
		//out::Log("OPTION " + std::to_string(static_cast<int>(option.direction)) + ": " + std::to_string(option.optionCost));
		if (option.optionCost > last && last != -1) {
			option_index++;
		}
		option.option_index = option_index;
		last = option.optionCost;
	}

	return options;
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	out::Log("****************************");
	out::Log("Navigating...");
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();

	auto it = ships.begin();
	for (; it != ships.end(); ) {
		Ship* s = *it;
		if (s->halite < floor(game->map->GetCell(s->pos).halite * (1.0 / constants::MOVE_COST_RATIO))) {
			hits[s->pos.x][s->pos.y] = BlockedCell::TRANSIENT;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			it = ships.erase(it);
			continue;
		}
		if(s->pos.ToroidalDistanceTo(s->task.position) <= 1 && !me.IsDropoff(s->task.position)) {
			hits[s->task.position.x][s->task.position.y] = BlockedCell::GHOST;
		}

		it++;
	}

	static std::uniform_real_distribution<double> random_01(0.0, 1.0);
	static std::mt19937_64& mersenne_twister = mt();
	static std::vector<NavigationOption> possible_options;
	static std::map<Ship*, std::vector<int>> possible_options_by_ship; // (indices)

	possible_options.clear();
	possible_options_by_ship.clear();
	mersenne_twister.seed(constants::GAME_SEED + game->turn);

	for (Ship* s : ships) {
		auto options = NavigationOptionsForShip(s);
		for (NavigationOption& no : options) {
			possible_options_by_ship[s].push_back(possible_options.size());
			possible_options.push_back(no);
		}
	}

	struct Simulation {
		Simulation() {
			cost = 0;
			for (int x = 0; x < constants::MAP_WIDTH; x++) {
				for (int y = 0; y < constants::MAP_HEIGHT; y++) {
					option_map[x][y] = -1;
				}
			}
		}
		Simulation(const Simulation& other) {
			cost = other.cost;
			memcpy(option_map, other.option_map, 64 * 64 * sizeof(int));
			picked_options = other.picked_options;
		}

		long double cost;
		int option_map[64][64];
		std::map<Ship*, int> picked_options;

		void computeCost() {
			cost = 0;
			for (auto& so : picked_options) {
				NavigationOption& picked_option = possible_options[so.second];
				cost += (picked_option.option_index-1) * static_cast<int>(picked_option.ship->task.type) * picked_option.ship->task.priority;
			}
		}

		int optionAt(Position p) {
			return option_map[p.x][p.y];
		}

		bool select(int option_index) {
			NavigationOption& option = possible_options[option_index];

			int oldOption = optionAt(option.pos);

			if (oldOption == option_index) return true;

			Ship* dangling_ship = nullptr;
			if (oldOption != -1) {
				static Game* g = Game::Get();
				bool allow_collision = g->GetMyPlayer().IsDropoff(possible_options[oldOption].pos) && g->strategy->allow_dropoff_collision;
				if (!allow_collision) {
					dangling_ship = deselect(oldOption);
				}
			}

			if (picked_options.find(option.ship) != picked_options.end()) {
				deselect(picked_options[option.ship]);
			}

			// finally select it
			option_map[option.pos.x][option.pos.y] = option_index;
			picked_options[option.ship] = option_index;

			if (dangling_ship) {
				return solve_conflict(dangling_ship);
			}

			return true;
		}

		Ship* deselect(int option_index) {
			if (option_index == -1) return nullptr;

			NavigationOption& option = possible_options[option_index];
			option_map[option.pos.x][option.pos.y] = -1;
			picked_options[option.ship] = -1;

			return option.ship;
		}

		bool solve_conflict(Ship* ship) {
			std::vector<int> non_conflict;
			for (int oi : possible_options_by_ship[ship]) {
				if (optionAt(possible_options[oi].pos) == -1) {
					non_conflict.push_back(oi);
				}
			}

			if (non_conflict.size() == 0) {
				int subind = std::floor(random_01(mersenne_twister) * possible_options_by_ship[ship].size());
				return select(possible_options_by_ship[ship][subind]);
			}
			else {
				int subind = std::floor(random_01(mersenne_twister) * non_conflict.size());
				return select(non_conflict[subind]);
			}
		}
	};

	Player::SortByTaskPriority(ships);

	Simulation simulation;
	for (auto it = ships.rbegin(); it != ships.rend(); it++) { // less important to most important
		simulation.select(possible_options_by_ship[*it][0]);
	}
	simulation.computeCost();
	out::Log("Initial cost: " + std::to_string(simulation.cost));

	Simulation best_simulation = Simulation(simulation);

	if (possible_options.size() > ships.size()) {
		// Explore
		int T = 0;
		int max = possible_options.size() * 100;

#ifndef HALITE_LOCAL
		max = INF;
#endif

		while (T < max && game->MsTillTimeout() > 50) {
			T++;

			Simulation new_simulation = Simulation(simulation);

			//out::Log("Picking...");

			int random_option_index = -1;
			int guard = 0;
			do {
				if (guard++ > possible_options.size() * 5) break;
				random_option_index = std::floor(random_01(mersenne_twister) * possible_options.size());
			} while (new_simulation.optionAt(possible_options[random_option_index].pos) == random_option_index);

			if (random_option_index == -1) {
				out::Log("Guard break");
				break;
			}

			//out::Log("Picked " + std::to_string(random_option_index));

			new_simulation.select(random_option_index);
			//out::Log("Selected");
			new_simulation.computeCost();
			//out::Log("Computed");

			bool accept = false;// random_01(mersenne_twister) < 0.0;// 0.65;

			if (new_simulation.cost < best_simulation.cost) {
				accept = true;
				best_simulation = Simulation(new_simulation);
				out::Log("Lower navigation cost found (" + std::to_string(T) + "): " + std::to_string(best_simulation.cost));
			}

			if (accept) {
				//out::Log("Accepted - cost: " + std::to_string(new_simulation.cost));
				simulation = Simulation(new_simulation);
			}

			if (simulation.cost == 0) break;
		}
	}

	for (auto& so : best_simulation.picked_options) {
		NavigationOption& option = possible_options[so.second];
		hits[option.pos.x][option.pos.y] = option.pos == option.ship->task.position && !me.IsDropoff(option.pos) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;
		commands.push_back(MoveCommand(option.ship->ship_id, option.direction));
		//out::Log("### SHIP " + std::to_string(option.ship->ship_id) + " CHOOSE OPTION " + std::to_string(static_cast<int>(option.direction)) + ": " + std::to_string(option.optionCost));
	}

	out::Log("****************************");
}
