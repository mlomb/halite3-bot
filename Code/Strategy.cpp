#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"
#include "Optimizer.hpp"

Strategy::Strategy(Game* game) {
	this->game = game;
	this->navigation = new Navigation(this);
	this->combat = new Combat(this);
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
					continue;
				}
			}
		}
	}

	// 1. Dropoffs
	// 2. Block dropoffs
	// 3. Drops
	// 4. Attacks
	// 5. Mining

	struct PositionJob {
		Position position;
		int order;

		bool operator==(const PositionJob& other) const {
			return position == other.position && order == other.order;
		}
	};

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
		Optimizer<Ship*, Position> block_dropoffs_optimizer;

		for (auto& sp : me.ships) {
			Ship* s = sp.second;
			if (s->assigned) continue;
			if (s->halite > 50) continue; // only use ships with low halite

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

							double priority = (2 - d) * 100000 + (100 - (combat->Friendliness(me, pp, nullptr)));
							priority /= (double)(s->pos.ToroidalDistanceTo(pp) + 1);

							block_dropoffs_optimizer.InsertEdge(s, pp, priority * 1000.0);
						}
					}
				}
			}
		}

		auto result = block_dropoffs_optimizer.Optimize(OptimizerMode::MAXIMIZE);

		for (auto& kv : result.assignments) {
			Ship* s = kv.first;
			if (s->assigned) continue;

			s->assigned = true;

			s->task.position = kv.second;
			s->task.type = TaskType::BLOCK_DROPOFF;
			s->task.priority = s->halite;

			shipsToNavigate.push_back(s);
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
		if (game->map->GetCell(s->pos).near_info[5].avgHalite < 45) {
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

	/* MINING */
	double threshold = std::min(35.0, game->map->map_avg_halite * 0.8);
	Optimizer<Ship*, Position> optimizer;

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			Position p = { x, y };
			Cell& c = game->map->GetCell(p);
			double friendliness = combat->Friendliness(me, p, nullptr);
			int dist_to_dropoff = closestDropoffDist[p.x][p.y];

			if (!game->IsDropoff(p) && c.halite > threshold && friendliness > features::friendliness_mine_cell) {
				for (auto& sp : me.ships) {
					Ship* s = sp.second;
					if (s->assigned) continue;

					// MINE
					int dist_to_cell = s->pos.ToroidalDistanceTo(p);

					double priority = 0;
					double profit = 0, time_cost = 0;

					/// --------------------
					int hal = c.halite;
					int near_hal = c.near_info[4].halite;

					if (friendliness >= 0) {
						for (auto& kv : c.near_info[4].all_ships) {
							if (kv.second->player_id != me.id) {
								near_hal += kv.second->halite;
							}
						}
						Ship* ss = game->GetShipAt(p);
						if (ss && ss->player_id != me.id) {
							hal += ss->halite;
						}
					}

					double near_hal_avg = (double)near_hal / (double)c.near_info[4].cells / game->map->map_avg_halite;

					profit = hal + near_hal_avg * 100.0;

					time_cost = dist_to_cell * 4.0 + dist_to_dropoff * 0.8;
					if (c.inspiration && constants::INSPIRATION_ENABLED) {
						profit *= 1 + constants::INSPIRED_BONUS_MULTIPLIER;
					}
					if (game->num_players == 2) {
						profit += c.near_info[4].num_ally_ships * 15;
						profit -= c.near_info[4].num_enemy_ships * 25;
					}
					else {
						profit -= c.near_info[4].num_ally_ships * 20;
					}
					/// --------------------

					priority = profit / time_cost;

					if (priority > 0) {
						optimizer.InsertEdge(s, p, (long long)priority);
					}
				}
			}
		}
	}

	
	auto result = optimizer.Optimize(OptimizerMode::MAXIMIZE);
	out::Log("Strategy priority: " + std::to_string(result.total_value));

	for (auto& kv : result.assignments) {
		Ship* s = kv.first;
		if (s->assigned) continue;

		s->assigned = true;
		out::Log("Priority chosen Ship #" + std::to_string(s->ship_id) + " in " + kv.second.str());


		s->task.position = kv.second;
		s->task.type = TaskType::MINE;
		s->task.priority = s->halite;

		shipsToNavigate.push_back(s);
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
	combat->Update();
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
