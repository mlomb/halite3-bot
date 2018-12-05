#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

#include <fdeep/fdeep.hpp>

Strategy::Strategy(Game* game) {
	this->game = game;
	this->navigation = new Navigation(this);
	this->allow_dropoff_collision = false;
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
	int all_ships = 0;
	for (auto& pp : game->players) {
		all_ships += pp.second.ships.size();
		if (pp.first != me.id) {
			enemy_halite += pp.second.TotalHalite();
			enemy_ships += pp.second.ships.size();
		}
	}

	int allowed_difference = game->num_players == 2 ? 5 : 10;

	if (game->turn < 0.2 * constants::MAX_TURNS)
		return true;

	return game->turn <= 0.75 * constants::MAX_TURNS &&
		   my_ships - enemy_ships <= allowed_difference &&
		   (double)game->map->halite_remaining / (double)all_ships > features::spawn_min_halite_per_ship;
}

std::vector<Position> Strategy::BestDropoffSpots()
{
	auto& me = game->GetMyPlayer();

	Position best_dropoff;
	bool found = false;

	if (me.ships.size() >= features::dropoff_per_ships * me.dropoffs.size() && game->turn <= 0.8 * constants::MAX_TURNS) {
		// find a good spot for a dropoff
		double bestRatio = -1;

		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			for (int y = 0; y < constants::MAP_HEIGHT; y++) {
				Position pos = { x, y };
				if (!game->IsDropoff(pos)) {
					Cell& c = game->map->GetCell(pos);
					AreaInfo& info_10 = c.near_info[10];
					AreaInfo& info_5 = c.near_info[5];

					double ratio = -1;
					int distance_to_closest_dropoff = closestDropoffDist[x][y];

					if (c.halite > 3600) {
						ratio = INF + c.halite;
					}
					else {
						if (distance_to_closest_dropoff > 15) {
							if (info_5.num_ally_ships > 1) {
								double to_map_avg = info_10.avgHalite / game->map->map_avg_halite;
								if (to_map_avg >= features::dropoff_to_map_avg && info_10.avgHalite >= features::dropoff_min_avg) {
									ratio = to_map_avg / (double)distance_to_closest_dropoff;
								}
							}
						}
					}

					if (ratio > bestRatio) {
						bestRatio = ratio;
						best_dropoff = pos;
						found = true;
					}
				}
			}
		}
	}
	
	std::vector<Position> dropoffs;

	if (found) {
		dropoffs.push_back(best_dropoff);
	}

	return dropoffs;
}

double Strategy::CalcFriendliness(Ship* s, Position p) {
	Game* game = Game::Get();
	Map* game_map = game->map;
	Cell& cell = game_map->GetCell(p);

	const int DISTANCES = 4; // 0, 1, 2, 3
	double contributions[DISTANCES] = { 6, 4, 2, 1 };
	double friendliness = 0;

	for (int d = 0; d < DISTANCES; d++) { // Dx
										  // ALLY
		for (auto& kv : cell.near_info[5].ally_ships_not_dropping_dist) {
			if (kv.second == s) continue;
			if (kv.first == d) {
				double i = 1.0 - ((double)kv.second->halite / (double)constants::MAX_HALITE);
				i = std::max(i, 0.1);
				friendliness += i * contributions[d];
			}
		}
		// ENEMY
		for (auto& kv : cell.near_info[5].enemy_ships_dist) {
			if (kv.first == d) {
				double contribution = DISTANCES - d;
				double i = 1.0 - ((double)kv.second->halite / (double)constants::MAX_HALITE);
				i = std::max(i, 0.1);
				friendliness -= i * contributions[d];
			}
		}
		// Ally Dropoffs are like ships with 0 halite
		for (auto& kv : cell.near_info[5].dropoffs_dist) {
			if (kv.first == d && kv.second == game->my_id) {
				double contribution = DISTANCES - d;
				double i = 1.0;
				friendliness += i * contributions[d];
			}
		}
	}

	return friendliness;
}

void Strategy::AssignTasks(std::vector<Command>& commands)
{
	reserved_halite = 0;

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
	// 2. Block dropoffs
	// 3. Drops
	// 4. Attacks
	// 5. Mining

	/* DROPOFFS */
	for (Position dropoff_spot : dropoffs) {
		Ship* dropoff_ship = me.ClosestShipAt(dropoff_spot);

		// TODO Fix for multiple dropoff spots
		if (!dropoff_ship || dropoff_ship->assigned) continue;

		dropoff_ship->assigned = true;

		dropoff_ship->task.position = dropoff_spot;
		dropoff_ship->task.type = TaskType::TRANSFORM_INTO_DROPOFF;
		dropoff_ship->task.priority = dropoff_ship->halite;

		reserved_halite += std::max(constants::DROPOFF_COST - dropoff_ship->halite - game->map->GetCell(dropoff_spot).halite, 0);

		shipsToNavigate.push_back(dropoff_ship);
	}

	FillClosestDropoffDist();

	/* BLOCK DROPOFFS */
	if (game->strategy->allow_dropoff_collision) {
		struct BlockEdge {
			Ship* s;
			Position position;
			double priority;
		};
		static std::vector<BlockEdge> block_edges;
		block_edges.clear();

		for (auto& sp : me.ships) {
			Ship* s = sp.second;
			if (s->assigned) continue;
			if (s->halite > 50) continue; // only use ships with low halite

			Position closest_enemy_dropoff;
			int distance = INF;
			for (auto& kv : game->players) {
				if (kv.first != game->my_id) {
					for (const Position& p : kv.second.dropoffs) {
						int offs[9][2] = {
							{ -1, -1 },
							{  0, -1 },
							{  1, -1 },
							{ -1,  0 },
							{  0,  0 },
							{  1,  0 },
							{ -1,  1 },
							{  0,  1 },
							{  1,  1 },
						};

						int dist = s->pos.ToroidalDistanceTo(p);
						if (dist > 10 && dist > game->remaining_turns * 1.1) {
							// we are too far!
							continue;
						}

						for (int i = 0; i < 9; i++) {
							Position pp = { p.x + offs[i][0], p.y + offs[i][1] };
							if (me.IsDropoff(pp)) continue;
							int d = std::abs(pp.x - p.x) + std::abs(pp.y - p.y);
							double h = game->map->GetCell(pp).enemy_reach_halite_max;
							h = std::max(0.0, h);

							block_edges.push_back({
								s,
								pp,
								p.ToroidalDistanceTo(s->pos) + (d == 2 ? 2 : 1) + (h / (double)constants::MAX_HALITE)
							});
						}
					}
				}
			}
		}

		std::sort(block_edges.begin(), block_edges.end(), [](const BlockEdge& a, const BlockEdge& b) {
			return a.priority > b.priority;
		});

		static bool blocked_assigned[MAX_MAP_SIZE][MAX_MAP_SIZE];
		for (int x = 0; x < constants::MAP_WIDTH; x++)
			for (int y = 0; y < constants::MAP_HEIGHT; y++)
				blocked_assigned[x][y] = false;

		for (BlockEdge& e : block_edges) {
			if (e.s->assigned) continue;
				
			if (!blocked_assigned[e.position.x][e.position.y]) {
				e.s->assigned = true;

				e.s->task.position = e.position;
				e.s->task.type = TaskType::BLOCK_DROPOFF;
				e.s->task.priority = e.priority;

				shipsToNavigate.push_back(e.s);

				blocked_assigned[e.position.x][e.position.y] = true;
			}
		}
	}

	/* DROPS */
	allow_dropoff_collision = false;

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

		int DROP_THRESHOLD = 970;
		if (game->map->GetCell(s->pos).near_info[5].avgHalite < 100) {
			DROP_THRESHOLD = 750;
		}

		if (s->halite >= DROP_THRESHOLD || s->dropping) {
			s->dropping = true;

			Position closest_dropoff = me.ClosestDropoff(s->pos);

			s->assigned = true;

			s->task.position = closest_dropoff;
			s->task.type = TaskType::DROP;
			s->task.priority = s->halite;

			shipsToNavigate.push_back(s);
		}
	}

	/* ATTACKS */
	if(!game->strategy->allow_dropoff_collision){
		for (auto& pp : game->players) {
			if (pp.first == me.id) continue;

			for (auto& ss : pp.second.ships) {
				// For each enemy ship
				Cell& c = game->map->GetCell(ss.second->pos);
				double friendliness = CalcFriendliness(nullptr, ss.second->pos);
				if (friendliness > 0.51937) {
					Ship* near_ship = me.ClosestShipAt(c.pos);
					if (!near_ship || near_ship->assigned) continue;

					near_ship->assigned = true;

					near_ship->task.position = c.pos;
					near_ship->task.type = TaskType::ATTACK;
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
		double profit;
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
					if (true) {
						int dist_to_cell = s->pos.ToroidalDistanceTo(p);
						int dist_to_dropoff = closestDropoffDist[p.x][p.y];

						double priority = 0;
						double profit = 0, time_cost = 0;

						/// --------------------
						int halite = c.halite;
						if (c.inspiration && constants::INSPIRATION_ENABLED) {
							halite *= 1 + constants::INSPIRED_BONUS_MULTIPLIER;
						}

						profit = halite * (1 + (c.near_info[6].avgHalite / game->map->map_avg_halite) * features::a);

						time_cost = dist_to_cell * features::b + dist_to_dropoff * features::c;

						//const int max_diff = 3;
						//int diff = (c.near_info[4].num_enemy_ships - c.near_info[4].num_ally_ships);
						//diff = std::max(-max_diff, std::min(max_diff, diff));

						profit += c.near_info[4].num_ally_ships  * features::d;
						profit += c.near_info[4].num_enemy_ships * features::e;

						/// --------------------

						priority = profit / time_cost;
						//out::Log(std::to_string(profit) + " / " + std::to_string(time_cost) + " = " + std::to_string(priority));

						if (priority > 0) {
							Edge edge;
							edge.s = s;
							edge.position = p;
							edge.profit = profit;
							edge.time_travel = dist_to_cell;
							edge.priority = profit / time_cost;
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
		if (std::fabs(a.priority - b.priority) < features::f)
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
		out::Log(std::to_string(e.priority));

		if (mining_assigned[e.position.x][e.position.y] <= max_ships) {
			e.s->assigned = true;

			e.s->task.position = e.position;
			e.s->task.type = TaskType::MINE;
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
