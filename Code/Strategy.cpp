#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

#include <fdeep/fdeep.hpp>

const int DROP_THRESHOLD = 970;

// Models
const fdeep::model& GetSpawnShipModel() {
	try {
		int num_players = Game::Get()->num_players;
		static const auto m = fdeep::load_model("model-ship-spawn-" + std::to_string(num_players) + "p.json", true, out::Log);
		return m;
	}
	catch (const std::exception& e) {
		out::Log(std::string(e.what()));
		throw e;
	}
}

Strategy::Strategy(Game* game) {
	this->game = game;
	this->navigation = new Navigation(this);
}

void Strategy::Initialize()
{
	GetSpawnShipModel();
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

	/*
	/// ML
	const auto result = GetSpawnShipModel().predict({
		fdeep::tensor5(fdeep::shape5(1, 1, 1, 1, 7),{
			(float)constants::MAX_TURNS - game->turn,
			(float)game->map->width * game->map->height,
			(float)game->map->halite_remaining,
			(float)my_halite,
			(float)my_ships,
			(float)enemy_halite,
			(float)enemy_ships
		})
	});

	// TODO Check this
	bool spawn_ship = (*result[0].as_vector())[0] > 0.9;

	return spawn_ship;
	*/


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
		case 32: max = 60; break;
		case 40: max = 62; break;
		case 48: max = 65; break;
		case 56: max = 68; break;
		case 64: max = 70; break;
		}
		break;
	case 4:
		switch (game->map->width) {
		case 32: max = 36; break;
		case 40: max = 40; break;
		case 48: max = 45; break;
		case 56: max = 52; break;
		case 64: max = 55; break;
		}
		break;
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
							if (info.num_ally_ships > 0 && info.num_ally_ships >= info.num_enemy_ships) {
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

		reserved_halite += constants::DROPOFF_COST - dropoff_ship->halite - game->map->GetCell(dropoff_spot).halite;

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
	int total_enemy_ships = 0;
	for (auto& pp : game->players) {
		if (pp.first != me.id) {
			total_enemy_ships += pp.second.ships.size();
		}
	}
	if (!game->strategy->allow_dropoff_collision) {
			for (auto& pp : game->players) {
				if (pp.first == me.id) continue;

				for (auto& ss : pp.second.ships) {
					// For each enemy ship
					Cell& c = game->map->GetCell(ss.second->pos);

					if (game->map->map_avg_halite <= 15 ||
						game->map->halite_remaining / (double)game->total_halite < 0.15 ||
						(me.ships.size() > 20 && me.ships.size() >= 2 * total_enemy_ships) ||
						(c.near_info[4].num_ally_ships > 3 && c.near_info[5].num_ally_ships > 3 * c.near_info[5].num_enemy_ships) ||
						(c.near_info[5].num_ally_ships > c.near_info[5].num_enemy_ships && c.halite > 650)) {
					if (c.near_info[4].num_ally_ships_not_dropping + 1 > c.near_info[4].num_enemy_ships && // is safe
						ss.second->halite > 200) { // is worth

						// near ships
						int num_assigned = 0;
						for (auto& ns : c.near_info[5].ally_ships_not_dropping_dist) {
							Ship* near_ship = ns.second;
							if (ns.second->assigned) continue;
							if (near_ship->halite > 750) {
								continue;
							}

							near_ship->assigned = true;

							near_ship->task.position = c.pos;
							near_ship->task.type = TaskType::ATTACK;
							near_ship->task.policy = EnemyPolicy::ENGAGE;
							near_ship->task.priority = ss.second->halite;

							shipsToNavigate.push_back(near_ship);
							num_assigned++;
							if (num_assigned >= 2) break;
						}
					}
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

#ifdef HALITE_DEBUG
		double miningPriorityMap[MAX_MAP_SIZE][MAX_MAP_SIZE];
#endif

		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			for (int y = 0; y < constants::MAP_HEIGHT; y++) {
				Position p = { x, y };
				if (!game->IsDropoff(p)) {
					Cell& c = game->map->GetCell(p);

					if (c.halite > threshold) {
						int dist_to_cell = s->pos.ToroidalDistanceTo(p);
						int dist_to_dropoff = closestDropoffDist[p.x][p.y];

						double profit, time_cost;

						profit = c.halite + (c.near_info[4].avgHalite / game->map->map_avg_halite) * 100.0;
						if (c.inspiration && constants::INSPIRATION_ENABLED) {
							profit *= 1 + constants::INSPIRED_BONUS_MULTIPLIER;
						}
						time_cost = dist_to_cell * 4.0 + dist_to_dropoff * 0.8;
						if (game->num_players == 2) {
							profit += std::min(c.near_info[4].num_ally_ships_not_dropping, 6) * 15;
							profit -= std::min(c.near_info[4].num_enemy_ships, 6) * 25;
						}
						else {
							profit -= std::min(c.near_info[4].num_ally_ships_not_dropping, 6) * 20;
						}

						Edge edge;
						edge.s = s;
						edge.position = p;
						edge.priority = profit / time_cost;
						edge.time_travel = dist_to_cell;

						if (edge.priority > 0) {
#ifdef HALITE_DEBUG
							miningPriorityMap[p.x][p.y] = edge.priority;
#endif
							edges.push_back(edge);
						}
					}
				}
			}
		}

#ifdef HALITE_DEBUG
		/*
		json data_map;
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			json data_row;
			for (int x = 0; x < constants::MAP_WIDTH; x++) {
				data_row.push_back(miningPriorityMap[x][y]);
			}
			data_map.push_back(data_row);
		}
		out::LogFluorineDebug({
			{ "type", "priority" },
			{ "position_x", s->pos.x },
			{ "position_y", s->pos.y }
			}, data_map);
		*/
#endif
	}

	out::Log("Edges: " + std::to_string(edges.size()));

	// Sort edges
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		if (std::fabs(a.priority - b.priority) < 0.5)
			return a.time_travel < b.time_travel;
		else
			return a.priority > b.priority;
	});

	// Assign mining
	static bool mining_assigned[MAX_MAP_SIZE][MAX_MAP_SIZE];
	for (int x = 0; x < constants::MAP_WIDTH; x++)
		for (int y = 0; y < constants::MAP_HEIGHT; y++)
			mining_assigned[x][y] = false;

	for (Edge& e : edges) {
		if (e.s->assigned) continue;

		if (!mining_assigned[e.position.x][e.position.y]) {
			e.s->assigned = true;

			e.s->task.position = e.position;
			e.s->task.type = TaskType::MINE;
			e.s->task.policy = EnemyPolicy::NONE;
			e.s->task.priority = e.priority;

			shipsToNavigate.push_back(e.s);

			mining_assigned[e.position.x][e.position.y] = true;
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

	/* CHECK FOR DANGER */
	for (Ship* s : shipsToNavigate) {
		// dodge if necessary
		if (s->task.policy == EnemyPolicy::NONE) {
			Cell& c = game->map->GetCell(s->pos);
			if (c.near_info[3].num_enemy_ships > c.near_info[3].num_ally_ships - 1 || s->halite > 600) {
				s->task.policy = EnemyPolicy::DODGE;
			}
		}
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
	if (!allow_dropoff_collision &&
		game->CanSpawnShip(reserved_halite) &&
		navigation->IsHitFree(me.shipyard_position)) {
		bool spawn_ship = ShouldSpawnShip();
		if(spawn_ship)
			commands.push_back(SpawnCommand());
		out::Log("We can spawn a ship. Spawned? " + std::to_string(spawn_ship));
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
