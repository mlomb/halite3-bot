#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

#include <fdeep/fdeep.hpp>

const int DROP_THRESHOLD = 970;

Strategy::Strategy(Game* game) {
	this->game = game;
	this->navigation = new Navigation(this);
}

void Strategy::Initialize()
{
	// Load Models?
}

bool Strategy::ShouldSpawnShip()
{
	auto& me = game->GetMyPlayer();

	int enemy_halite = 0;
	int enemy_ships = 0;
	int my_halite = me.TotalHalite();
	int my_ships = me.ships.size();
	for (auto& pp : game->players) {
		if (pp.first != me.id) {
			enemy_halite += pp.second.TotalHalite();
			enemy_ships += pp.second.ships.size();
		}
	}

	// HARD MAX TURNS
	if (game->turn >= 0.8 * constants::MAX_TURNS)
		return false;

	// HARD MIN TURNS
	if (game->turn < 0.2 * constants::MAX_TURNS)
		return true;

	// HARD MAX COLLECTED
	if (game->map->halite_remaining / (double)(constants::MAP_WIDTH * constants::MAP_HEIGHT) < 60)
		return false;

	// HARD MAX SHIPS
	if (my_ships > 20 && my_ships > enemy_ships * 2)
		return false;

	double max = 0.65;
	switch (game->num_players) {
	case 2:
		switch (game->map->width) {
		case 32: max = 0.60; break;
		case 40: max = 0.62; break;
		case 48: max = 0.65; break;
		case 56: max = 0.68; break;
		case 64: max = 0.70; break;
		}
		break;
	case 4:
		switch (game->map->width) {
		case 32: max = 0.36; break;
		case 40: max = 0.40; break;
		case 48: max = 0.45; break;
		case 56: max = 0.52; break;
		case 64: max = 0.55; break;
		}
		break;
	}
	if (game->num_players == 4) {
		max *= 0.85;
	}
	else {
		max *= 1.15;
	}

	return game->turn <= max * constants::MAX_TURNS;
}

std::vector<Position> Strategy::BestDropoffSpots()
{
	auto& me = game->GetMyPlayer();

	Position best_dropoff;
	best_dropoff.x = -1;
	best_dropoff.y = -1;

	if (me.ships.size() >= features::dropoff_ships_needed * me.dropoffs.size() && game->turn <= 0.75 * constants::MAX_TURNS) {
		// find a good spot for a dropoff
		double bestRatio = -1;

		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			for (int y = 0; y < constants::MAP_HEIGHT; y++) {
				Position pos = { x, y };
				if (!game->IsDropoff(pos)) {
					Cell& c = game->map->GetCell(pos);
					double ratio = -1;

					if (c.halite > 3000) {
						ratio = INF + c.halite;
					}
					else {
						int distance_to_closest_dropoff = closestDropoffDist[x][y];
						if (distance_to_closest_dropoff > std::ceil(constants::MAP_WIDTH * features::dropoff_map_distance)) {
							AreaInfo& info = c.near_info[5];
							if (info.num_ally_ships > 2 && info.num_ally_ships >= info.num_enemy_ships) {
		 						if (info.avgHalite / game->map->map_avg_halite >= features::dropoff_avg_threshold) {
									ratio = info.avgHalite / (distance_to_closest_dropoff * distance_to_closest_dropoff);
								}
							}
						}
					}

					if (ratio > bestRatio) {
						bestRatio = ratio;
						best_dropoff = pos;
					}
				}
			}
		}
	}

	std::vector<Position> dropoffs;

	if (best_dropoff.x != -1) {
		dropoffs.push_back(best_dropoff);
	}

	return dropoffs;
}

double Strategy::CalcFriendliness(Ship* s, Position p) {
	Game* game = Game::Get();
	Map* game_map = game->map;
	Cell& cell = game_map->GetCell(p);

	const int DISTANCES = 4; // 0, 1, 2, 3
	double friendliness = 0;

	for (int d = 0; d < DISTANCES; d++) { // Dx
										  // ALLY
		for (auto& kv : cell.near_info[5].ally_ships_not_dropping_dist) {
			if (kv.second == s) continue;
			if (kv.first == d) {
				double contribution = DISTANCES - d;
				double i = 1.0 - ((double)kv.second->halite / (double)constants::MAX_HALITE);
				friendliness += i * contribution;
			}
		}
		// ENEMY
		for (auto& kv : cell.near_info[5].enemy_ships_dist) {
			if (kv.first == d) {
				double contribution = DISTANCES - d;
				double i = 1.0 - ((double)kv.second->halite / (double)constants::MAX_HALITE);
				friendliness -= i * contribution;
			}
		}
		// Ally Dropoffs are like ships with 0 halite
		for (auto& kv : cell.near_info[5].dropoffs_dist) {
			if (kv.first == d && kv.second == game->my_id) {
				double contribution = DISTANCES - d;
				double i = 1.0;
				friendliness += i * contribution;
			}
		}
	}

	return friendliness;
}

void Strategy::AssignTasks(std::vector<Command>& commands)
{
	reserved_halite = 0;
	allow_dropoff_collision = false;

	Player& me = game->GetMyPlayer();
	if (me.ships.size() == 0) return;

	out::Stopwatch s("Assign tasks");
	out::Log("Assigning tasks...");

	std::vector<Position> dropoffs = BestDropoffSpots();

	for (auto& sp : me.ships) {
		Ship* s = sp.second;

		// Reset
		s->assigned = false;
		s->task = {};

		if (me.IsDropoff(s->pos)) {
			s->dropping = false;
		}
		else {
			// Transform into dropoffs
			auto dit = std::find(dropoffs.begin(), dropoffs.end(), s->pos);
			if (dit != dropoffs.end()) {
				// transform into dropoff
				if (game->TransformIntoDropoff(s, commands)) {
					s->assigned = true;
					dropoffs.erase(dit);
				}
			}
		}
	}

	// 1. Dropoffs
	// 2. Drops
	// 3. Attacks
	// 4. Mining

	/* DROPOFFS */
	for (Position dropoff_spot : dropoffs) {
		Ship* dropoff_ship = me.ClosestShipAt(dropoff_spot);

		// TODO Fix for multiple dropoff spots
		if (!dropoff_ship || dropoff_ship->assigned) continue;

		dropoff_ship->assigned = true;

		dropoff_ship->task.position = dropoff_spot;
		dropoff_ship->task.type = TaskType::TRANSFORM_INTO_DROPOFF;
		dropoff_ship->task.policy = game->map->GetCell(dropoff_spot).halite > 3000 ? EnemyPolicy::ENGAGE : EnemyPolicy::DODGE;
		dropoff_ship->task.priority = dropoff_ship->halite;

		reserved_halite += std::max(constants::DROPOFF_COST - dropoff_ship->halite - game->map->GetCell(dropoff_spot).halite, 0);

		shipsToNavigate.push_back(dropoff_ship);
	}

	FillClosestDropoffDist();

	/* DROPS */
	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;

		if (s->halite < 300) {
			s->dropping = false;
		}

		int time_to_drop = closestDropoffDist[s->pos.x][s->pos.y];
		if (game->turn + std::ceil(time_to_drop * 1.255) >= constants::MAX_TURNS) {
			allow_dropoff_collision = true;
			s->dropping = true;
		}

		if (s->halite >= DROP_THRESHOLD || s->dropping) {
			s->dropping = true;

			Position closest_dropoff = me.ClosestDropoff(s->pos);

			s->assigned = true;

			s->task.position = closest_dropoff;
			s->task.type = TaskType::DROP;
			s->task.policy = EnemyPolicy::DODGE;
			s->task.priority = s->halite;

			shipsToNavigate.push_back(s);
		}
	}

	/* ATTACKS */
	if (!game->strategy->allow_dropoff_collision) {
		for (auto& pp : game->players) {
			if (pp.first == me.id) continue;

			for (auto& ss : pp.second.ships) {
				// For each enemy ship
				Cell& c = game->map->GetCell(ss.second->pos);
				double friendliness = CalcFriendliness(nullptr, ss.second->pos);
				if (friendliness > features::friendliness_should_attack) {
					Ship* near_ship = me.ClosestShipAt(c.pos);
					if (!near_ship || near_ship->assigned) continue;

					near_ship->assigned = true;

					near_ship->task.position = c.pos;
					near_ship->task.type = TaskType::ATTACK;
					near_ship->task.policy = EnemyPolicy::ENGAGE;
					near_ship->task.priority = ss.second->halite;

					shipsToNavigate.push_back(near_ship);
				}
			}
		}
	}

	/* MINING */
	struct Edge {
		Ship* s;

		Position position;
		double priority;
		int time_travel;
	};

	static std::vector<Edge> edges;
	edges.clear();
	edges.reserve(me.ships.size() * constants::MAP_WIDTH * constants::MAP_HEIGHT);

	// Create mining edges
	double threshold = std::min(35.0, game->map->map_avg_halite * 0.8);

	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;

		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			for (int y = 0; y < constants::MAP_HEIGHT; y++) {
				Position p = { x, y };
				if (!game->IsDropoff(p)) {
					Cell& c = game->map->GetCell(p);

					double friendliness = CalcFriendliness(nullptr, p);
					if (c.halite > threshold && friendliness > features::friendliness_mine_cell) { // -0.5
						int dist_to_cell = s->pos.ToroidalDistanceTo(p);
						int dist_to_dropoff = closestDropoffDist[p.x][p.y];

						double priority = 0;
						double profit = 0, time_cost = 0;

						/// --------------------
						profit = c.halite + (c.near_info[4].avgHalite / game->map->map_avg_halite) * 100.0;

						time_cost = dist_to_cell * 4.0 + dist_to_dropoff * 0.8;
						if (c.inspiration && constants::INSPIRATION_ENABLED) {
							profit *= 1 + constants::INSPIRED_BONUS_MULTIPLIER;
						}
						if (game->num_players == 2) {
							profit += std::min(0, c.near_info[4].num_ally_ships) * 15;
							profit -= c.near_info[4].num_enemy_ships * 25;
						}
						else {
							profit -= c.near_info[4].num_ally_ships * 35;
						}
						/// --------------------

						priority = profit / time_cost;

						if (priority > 0) {
							Edge edge;
							edge.s = s;
							edge.position = p;
							edge.priority = priority;
							edge.time_travel = dist_to_cell;
							edges.push_back(edge);
						}
					}
				}
			}
		}
	}

	out::Log("Edges: " + std::to_string(edges.size()));

	// Sort edges
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		if (std::fabs(a.priority - b.priority) < 0.05)
			return a.time_travel < b.time_travel;
		else
			return a.priority > b.priority;
	});

	// Assign mining
	static int mining_assigned[MAX_MAP_SIZE][MAX_MAP_SIZE];
	for (int x = 0; x < constants::MAP_WIDTH; x++)
		for (int y = 0; y < constants::MAP_HEIGHT; y++)
			mining_assigned[x][y] = 0;

	for (Edge& e : edges) {
		if (e.s->assigned) continue;

		Cell& c = game->map->GetCell(e.position);

		int max_ships = 1;
		/*
		if (c.halite > 300 && c.near_info[3].num_enemy_ships > 0) {
			max_ships = c.halite / (c.near_info[3].avgHalite * 2);
		}
		max_ships = std::min(2, std::max(1, max_ships));
		*/

		if (mining_assigned[e.position.x][e.position.y] <= max_ships) {
			e.s->assigned = true;

			e.s->task.position = e.position;
			e.s->task.type = TaskType::MINE;
			e.s->task.policy = EnemyPolicy::NONE;
			e.s->task.priority = e.priority;

			shipsToNavigate.push_back(e.s);

			mining_assigned[e.position.x][e.position.y]++;
		}
	}

	/* CHECK FOR UNASSIGNMENT */
	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;

		s->assigned = true;

		Position closest_dropoff = me.ClosestDropoff(s->pos);
		s->task.position = closest_dropoff;
		s->task.type = TaskType::DROP;
		s->task.policy = EnemyPolicy::NONE;
		s->task.priority = s->halite;

		shipsToNavigate.push_back(s);
	}
}

void Strategy::Execute(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();

	navigation->Clear();
	shipsToNavigate.clear();

	AssignTasks(commands);

	navigation->Navigate(shipsToNavigate, commands);

	//------------------------------- SHIP SPAWNING
	if (!allow_dropoff_collision && navigation->IsHitFree(me.shipyard_position)) {
		if (game->CanSpawnShip(reserved_halite)) {
			bool spawn_ship = ShouldSpawnShip();
			if (spawn_ship)
				commands.push_back(SpawnCommand());
			out::Log("We can spawn a ship. Spawned? " + std::to_string(spawn_ship));
		}
	}
}

void Strategy::FillClosestDropoffDist()
{
	Player& me = game->GetMyPlayer();

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			closestDropoffDist[x][y] = me.DistanceToClosestDropoff({ x, y });
		}
	}
}
