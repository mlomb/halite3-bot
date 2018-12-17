#include "Navigation.hpp"

#include "Game.hpp"
#include "Strategy.hpp"
#include "Optimizer.hpp"

Navigation::Navigation(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

void Navigation::MinCostBFS(Position start, OptimalPathMap& map)
{
	Player& me = game->GetMyPlayer();
	Map* game_map = game->map;

	std::queue<Position> q;

	map.cells[start.x][start.y].haliteCost = game_map->GetCell(start).MoveCost();
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

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + game_map->GetCell(new_pos).MoveCost();
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			if (hits[new_pos.x][new_pos.y] == BlockedCell::TRANSIENT) {
				new_state.turns += 4;
			}
			/*
			if (hits[new_pos.x][new_pos.y] == BlockedCell::STATIC ||
				hits[new_pos.x][new_pos.y] == BlockedCell::GHOST) {
				new_state.turns += 4;
			}
			*/

			if (new_state < map.cells[new_pos.x][new_pos.y]) {
				if (!map.cells[new_pos.x][new_pos.y].added)
					q.push(new_pos);
				map.cells[new_pos.x][new_pos.y] = new_state;
			}
		}
	}

	/*
	out::Log("------------");
	out::Log("BFS FROM " + start.str());
	for (int y = 0; y < constants::MAP_HEIGHT; y++) {
		std::string l;
		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			l += std::to_string(map.cells[x][y].cost()) + " ";
		}
		out::Log(l);
	}
	out::Log("------------");
	*/
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

		// enemy dropoffs
		for (Position dp : enemy_player.dropoffs) {
			if (strategy->allow_dropoff_collision ||
				// prevent ships colliding into enemy dropoffs if enemies close
				game->map->GetCell(dp).near_info[1].num_enemy_ships > 0) {
				hits[dp.x][dp.y] = BlockedCell::STATIC;
			}
		}

		// enemy ships
		for (auto& es : enemy_player.ships) {
			if (me.IsDropoff(es.second->pos)) {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::EMPTY;
			}
			else {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::STATIC;
			}
		}
	}

	// ally ships
	for (auto& sp : me.ships) {
		if (!sp.second->CanMove()) {
			hits[sp.second->pos.x][sp.second->pos.y] = BlockedCell::TRANSIENT;
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
	Cell& current_cell = game_map->GetCell(ship->pos);
	Position target = ship->task.position;

	OptimalPathMap map = {};
	MinCostBFS(target, map);
	
	std::vector<NavigationOption> options;
	bool threat = false;
	
	for (Direction direction : DIRECTIONS_WITH_STILL) {
		Position p = ship->pos.DirectionalOffset(direction);
		Cell& c = game->map->GetCell(p);
		int friendliness = strategy->combat->FriendlinessNew(me, p, ship);
		int enemy_reach = strategy->combat->EnemyReachHalite(me, p);

		if (direction == Direction::STILL && me.IsDropoff(p)) {
			// do not stay still in a dropoff ever.
			continue;
		}
		if (game->turn < 10 && me.IsDropoff(p) && !ship->dropping) {
			// early fix to bug
			continue;
		}

		if (IsHitFree(p)) {
			NavigationOption option;
			option.direction = direction;
			option.pos = p;
			option.ship = ship;


			bool dodge = false;

			if (enemy_reach != -1) {
				if (ship->dropping) {
					dodge = true;
				}
				else {
					Ship* ship_there = game->GetShipAt(p);
					bool enemy_ship = ship_there && ship_there->player_id != me.id;

					if (enemy_ship) {
						dodge = p != ship->task.position;
					}
					else {
						dodge = friendliness <= features::d /* 0 */;
					}

				}
			}
			out::Log("ship: " + std::to_string(ship->ship_id) + " dir: " + std::to_string((int)(direction)) + " friendliness: " + std::to_string(friendliness) + " enemy reach: " + std::to_string(enemy_reach));

			long long int cost = 0;
			if (dodge) {
				cost += 10000000;

				cost += 1000000 * c.near_info[0].num_enemy_ships;
				cost +=  100000 * c.near_info[1].num_enemy_ships;
				cost +=   10000 * c.near_info[2].num_enemy_ships;
				cost +=    1000 * c.near_info[3].num_enemy_ships;
			}
			else {
				cost += (5 - static_cast<int>(ship->task.type)) * 1000000;
				cost += map.cells[p.x][p.y].cost(); // max 100000
			}
			/*
			int allies_at_d1 = c.near_info[1].num_ally_ships - c.near_info[0].num_ally_ships_not_dropping;
			int enemies_at_d1 = c.near_info[1].num_enemy_ships - c.near_info[0].num_enemy_ships;
			if (enemy_reach >= 0 && enemies_at_d1 >= allies_at_d1) {
				cost += 10000000 * enemies_at_d1;
				//cost += 100000 * (30.0 - (friendliness + 15.0));
			}
			*/
			//if (friendliness < -2.5) {
			//	cost += 10000000 * (30.0 - (friendliness + 15.0));
			//}
			/*

			// only apply costs if cell is not an ally dropoff
			// and we are not blocking an enemy dropoff or attacking
			if (!me.IsDropoff(p) && ship->task.type != TaskType::BLOCK_DROPOFF && ship->task.type != TaskType::ATTACK) {
				bool inminent_attack = strategy->combat->WillReceiveImminentAttack(me, p);
				bool free_to_move = strategy->combat->FreeToMove(me, p);
				bool preserve = false;
				double friendliness = strategy->combat->Friendliness(me, p);

				//out::Log("inminent_attack: " + std::to_string(inminent_attack) + ", free_to_move: " + std::to_string(free_to_move) + ", friendliness: " + std::to_string(friendliness));

				if (inminent_attack) {
					cost += 10000000;
				}
				else {
					if (c.halite > 500 &&
						direction == Direction::STILL &&
						c.near_info[3].num_enemy_ships > 0 &&
						friendliness < -0.3) {
						// preserve halite
						for (auto& kv : me.ships) {
							if (kv.second->task.position == p && kv.second->pos.ToroidalDistanceTo(ship->pos) > 1 ) {
								// preserve
								preserve = true;
							}
						}
					}
				}


				if (preserve) {
					cost = 0;
				}
				else {
					int enemy_reach_halite = strategy->combat->EnemyReachHalite(me, p);
					if (enemy_reach_halite >= 0 && !strategy->allow_dropoff_collision) {

						if (!free_to_move) {
							cost += 100000;
						}

						int friendliness_05 = 30 - std::ceil((friendliness + 15.0) / 0.5);
						cost += friendliness_05 * 10000;
					}
				}
			}

			*/
			if (strategy->combat->EnemyReachHalite(me, p) >= 0) {
				threat = true;
			}

			option.optionCost = cost;

			options.push_back(option);
		}
	}

	/* TEST */
	/*
	if (threat) {
		std::string key = "";
		for (int x = -3; x <= 3; x++) {
			for (int y = -3; y <= 3; y++) {
				Position p = { ship->pos.x + x , ship->pos.y + y };
				Ship* k = game->GetShipAt(p);
				if (p.ToroidalDistanceTo(ship->pos) > 3)
					key += 'x';
				else
					key += k ? (k->player_id == ship->player_id ? '+' : '-') : '*';
			}
		}
		out::Log("KEYTEST " + key);
	}
	*/
	/* TEST */

	return options;
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	out::Log("****************************");
	out::Log("Navigating " + std::to_string(ships.size()) + " ships...");
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();

	Optimizer<Ship*, Position> optimizer;
	std::vector<Direction> all_dirs = DIRECTIONS;

	Player::SortByTaskPriority(ships);

	for (Ship* s : ships) {
		if (!s->CanMove()) continue;

		if (strategy->allow_dropoff_collision) {
			bool go_crash = false;
			for (Direction d : DIRECTIONS_WITH_STILL) {
				Position p = s->pos.DirectionalOffset(d);
				if (me.IsDropoff(p)) {
					commands.push_back(MoveCommand(s->ship_id, d));
					go_crash = true;
					break;
				}
			}
			if (go_crash)
				continue;
		}

		auto options = NavigationOptionsForShip(s);

		for (NavigationOption& no : options) {
			//out::Log("Ship #" + std::to_string(s->ship_id) + " option " + no.pos.str() + ": " + std::to_string(no.optionCost));
			optimizer.InsertEdge(s, no.pos, no.optionCost);
		}
	}

	auto result = optimizer.OptimizeOptimal(OptimizerMode::MINIMIZE);
	out::Log("Navigation cost: " + std::to_string(result.total_value));

	for (auto& so : result.assignments) {
		Ship* s = so.first;
		Position p = so.second;
		for (Direction d : DIRECTIONS_WITH_STILL) {
			if (s->pos.DirectionalOffset(d) == p) {
				commands.push_back(MoveCommand(s->ship_id, d));
				hits[p.x][p.y] = p == s->task.position && !me.IsDropoff(p) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;

				//out::Log("### SHIP " + std::to_string(s->ship_id) + " CHOOSE OPTION " + std::to_string(static_cast<int>(d)) + " WITH TARGET: " + s->task.position.str());
				break;
			}
		}
	}
}
