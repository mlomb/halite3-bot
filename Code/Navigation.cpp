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

			double cost = game_map->GetCell(p).MoveCost();
			if (cost < 15) // treat as free
				cost = 0;

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + cost;
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			if (hits[new_pos.x][new_pos.y] == BlockedCell::STATIC ||
				hits[new_pos.x][new_pos.y] == BlockedCell::GHOST) {
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
		}
	}

	// fill hit map with enemies
	for (auto& ep : game->players) {
		Player& enemy_player = ep.second;
		if (enemy_player.id == game->my_id) continue;

		// avoid enemy shipyard
		hits[enemy_player.shipyard_position.x][enemy_player.shipyard_position.y] = BlockedCell::STATIC;

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

std::vector<NavigationOption> Navigation::NavigationOptionsForShip(Ship* ship)
{
	Player& me = game->GetMyPlayer();
	Map* game_map = game->map;

	Position target = ship->task.position;
	EnemyPolicy policy = ship->task.policy;

	switch (ship->task.type) {
	case TaskType::NONE:
		target = ship->pos;
		break;
	}
	if (strategy->closestDropoffDist[ship->pos.x][ship->pos.y] <= 2) {
		policy = EnemyPolicy::NONE;
	}

	std::vector<NavigationOption> options;

	OptimalPathMap map = {};
	MinCostBFS(target, map);

	std::vector<Direction> dirs = DIRECTIONS;
	if (!me.IsDropoff(ship->pos) || strategy->allow_dropoff_collision) {
		dirs.push_back(Direction::STILL);
	}

	for (const Direction direction : dirs) {
		Position pp = ship->pos.DirectionalOffset(direction);

		/// --------------------------------------------------------------
		Cell& c = game_map->GetCell(target);
		Cell& moving_cell = game_map->GetCell(pp);
		bool hit_free = IsHitFree(pp);
		Ship* other_ship = moving_cell.ship_on_cell;
		bool is_other_enemy = other_ship && other_ship->player_id != me.id;
		AreaInfo& info_3 = c.near_info[3];
		AreaInfo& moving_cell_info = moving_cell.near_info[3];

		double optionCost = map.cells[pp.x][pp.y].ratio();
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
					ship->halite < 750 &&
					(other_ship->halite > 500)) {
					optionCost = 1;
					possibleOption = true;
				}
			}
		}
		/// --------------------------------------------------------------

		if (possibleOption) {
			if (me.IsDropoff(pp) && strategy->allow_dropoff_collision) {
				optionCost = 0;
			}

			options.push_back({
				optionCost,
				pp,
				direction,
				ship
			});
		}
	}

	if (options.size() == 0) {
		out::Log("No NavigationOption found for " + std::to_string(ship->ship_id) + ".");
		options.push_back({
			0,
			ship->pos,
			Direction::STILL,
			ship
		});
	}

	std::sort(options.begin(), options.end(), [](const NavigationOption& a, const NavigationOption& b) {
		return a.optionCost < b.optionCost;
	});

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
		if (!s->CanMove()) {
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
			memcpy(option_map, other.option_map, MAX_MAP_SIZE * MAX_MAP_SIZE * sizeof(int));
			picked_options = other.picked_options;
		}

		long double cost;
		int option_map[MAX_MAP_SIZE][MAX_MAP_SIZE];
		std::map<Ship*, int> picked_options;

		void computeCost() {
			cost = 0;
			for (auto& so : picked_options) {
				NavigationOption& picked_option = possible_options[so.second];
				double oportunity_cost = (picked_option.optionCost - possible_options[possible_options_by_ship[picked_option.ship][0]].optionCost);
				cost += oportunity_cost * static_cast<int>(picked_option.ship->task.type) * picked_option.ship->task.priority;
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
	out::Log("Initial navigation cost: " + std::to_string(simulation.cost));

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

			new_simulation.select(random_option_index);
			new_simulation.computeCost();

			if (new_simulation.cost < simulation.cost) {
				simulation = Simulation(new_simulation);
				out::Log("Lower navigation cost found (" + std::to_string(T) + "): " + std::to_string(simulation.cost));
			}

			if (simulation.cost == 0) break;
		}
	}

	for (auto& so : simulation.picked_options) {
		NavigationOption& option = possible_options[so.second];
		hits[option.pos.x][option.pos.y] = option.pos == option.ship->task.position && !me.IsDropoff(option.pos) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;
		commands.push_back(MoveCommand(option.ship->ship_id, option.direction));

		//out::Log("### SHIP " + std::to_string(option.ship->ship_id) + " CHOOSE OPTION " + std::to_string(static_cast<int>(option.direction)) + ": " + std::to_string(option.optionCost));
	}

	out::Log("****************************");
}
