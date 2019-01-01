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
	//return me.ships.size() <= 4;

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

	/// ---------------------------------------------------------------------
	/// ---------------------------  PLAYGROUND -----------------------------
	/// ---------------------------------------------------------------------

	const int SETS_SIZE = 8;
	std::set<std::set<Ship*>> set_of_sets;

	// for each ship not assigned
	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;
		Cell& c = game->map->GetCell(s->pos);

		set_of_sets.insert(me.GetShipsByDistance(s->pos, SETS_SIZE));
	}

	out::Log("SET OF SETS (" + std::to_string(set_of_sets.size()) +"):");
	/*
	for (auto& s : set_of_sets) {
		std::string line = "";
		for (Ship* ss : s) {
			line += std::to_string(ss->ship_id) + ", ";
		}
		out::Log(line);
	}
	*/

	std::set<std::set<Ship*>> set_of_sets_combined;

	for (auto& s : set_of_sets) {
		std::bitset<SETS_SIZE> bs;
		std::vector<Ship*> vec(s.size());
		std::copy(s.begin(), s.end(), vec.begin());

		do {
			std::set<Ship*> comb_set;
			for (int i = 0; i < vec.size(); i++) {
				if (bs.test(i)) {
					comb_set.insert(vec[i]);
				}
			}
			if (comb_set.size() == 0) continue;
			set_of_sets_combined.insert(comb_set);
		} while (increase(bs));
	}

	out::Log("SET OF SETS COMBINED (" + std::to_string(set_of_sets_combined.size()) + "):");
	/*
	for (auto& s : set_of_sets_combined) {
		std::string line = "";
		for (Ship* ss : s) {
			line += std::to_string(ss->ship_id) + ", ";
		}
		out::Log(line);
	}
	*/

	struct SetRank {
		double rank;
		Position p;
		std::vector<Ship*> ships;
	};
	std::vector<SetRank> ranks;

	// rank sets
	double threshold = std::min(35.0, game->map->map_avg_halite * 0.8);

	for (auto& s : set_of_sets_combined) {
		std::vector<Ship*> vec(s.size());
		std::copy(s.begin(), s.end(), vec.begin());

		for (int x = 0; x < constants::MAP_WIDTH; x++) {
			for (int y = 0; y < constants::MAP_HEIGHT; y++) {
				Position p = { x, y };
				Cell& c = game->map->GetCell(p);

				double rank = 0;
				/// -----------------------
				std::vector<Ship*> ships_by_distance = Player::SortShipsByDistance(vec, p);

				int dist_to_dropoff = me.DistanceToClosestDropoff(p);

				if (ships_by_distance.size() != 1)
					continue;
				if (game->IsDropoff(p))
					continue;

				// BEST 35k
				int penalty = 0;
				int hal = c.halite;
				if (c.inspiration && constants::INSPIRATION_ENABLED)
					hal *= 1 + constants::INSPIRED_BONUS_MULTIPLIER;
				if (hal < 1.1 * game->map->map_avg_halite)
					penalty = 100;
				rank = (hal / (double)c.near_info[3].halite) / (double)(1 + ships_by_distance[0]->pos.ToroidalDistanceTo(p) + 0 * dist_to_dropoff + penalty);

				/// -----------------------

				ranks.push_back({
					rank,
					p,
					vec
				});
			}
		}
	}

	out::Log("SET RANKS (" + std::to_string(ranks.size()) + "):");

	std::sort(ranks.begin(), ranks.end(), [](const SetRank& a, const SetRank& b) {
		return a.rank == b.rank ? a.ships.size() < b.ships.size() : a.rank > b.rank;
	});

	std::map<Ship*, bool> assigned;
	bool pos_assigned[MAX_MAP_SIZE][MAX_MAP_SIZE];

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			pos_assigned[x][y] = false;
		}
	}

	for (SetRank& sr : ranks) {
		bool possible = !pos_assigned[sr.p.x][sr.p.y];
		for (Ship* s : sr.ships) {
			possible &= !assigned[s];
		}

		if (possible) {
			std::string setstr = "";
			for (Ship* s : sr.ships) {
				assigned[s] = true;
				setstr += std::to_string(s->ship_id) + ", ";

				// REAL ASSIGNMENT
				s->assigned = true;

				s->task.position = sr.p;
				s->task.type = TaskType::MINE;
				s->task.priority = sr.rank;

				shipsToNavigate.push_back(s);
			}
			pos_assigned[sr.p.x][sr.p.y] = true;
			out::Log("Assigned set " + setstr + " with rank " + std::to_string(sr.rank) + " on " + sr.p.str());
		}
	}

	return;

	/// ---------------------------------------------------------------------
	/// ---------------------------------------------------------------------
	/// ---------------------------------------------------------------------

	/* MINING */
	//double threshold = std::min(35.0, game->map->map_avg_halite * 0.8);
	Optimizer<Ship*, Position> optimizer;

	
	double priorityMap[MAX_MAP_SIZE][MAX_MAP_SIZE];

	// 1st pass, base halite
	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			Position p = { x, y };
			priorityMap[x][y] = 0;

			if (game->IsDropoff(p))
				continue;

			Cell& c = game->map->GetCell(p);
			double friendliness = combat->Friendliness(me, p, game->GetShipAt(p));
			int dist_to_dropoff = closestDropoffDist[p.x][p.y];

			/// --------------------
			long long hal = c.halite;

			Ship* ss = game->GetShipAt(p);

			if (friendliness > features::friendliness_dodge) {
				if (c.near_info[3].num_ally_ships >= c.near_info[3].num_enemy_ships) {
					if (ss && ss->player_id != me.id) {
						hal += ss->halite;
					}
				}
			}
			if (ss && ss->player_id == me.id) {
				if (ss->dropping && c.halite > 300) {
					hal += 1000;
				}
			}

			if (c.inspiration) {
				double mult = 1 + constants::INSPIRED_BONUS_MULTIPLIER;
				hal *= mult;
			}
			hal += std::min(3, c.near_info[4].num_enemy_ships) * 5;

			if (hal < threshold)
				continue;

			priorityMap[x][y] = hal;
		}
	}
	// 2nd pass, near halite
	//for (int x = 0; x < constants::MAP_WIDTH; x++) {
	//	for (int y = 0; y < constants::MAP_HEIGHT; y++) {
	//		Position p = { x, y };
	//
	//		long long near_hal = 0;
	//		int cells = 0;
	//
	//		const int NEAR_RADIO = 4;
	//
	//		for (int xx = -NEAR_RADIO; xx <= NEAR_RADIO; xx++) {
	//			for (int yy = -NEAR_RADIO; yy <= NEAR_RADIO; yy++) {
	//				Position pos = { x + xx, y + yy };
	//				int d = pos.ToroidalDistanceTo(p);
	//
	//				if (d <= NEAR_RADIO) {
	//					cells++;
	//					near_hal += priorityMap[x][y];
	//				}
	//			}
	//		}
	//
	//		near_hal /= cells;
	//		near_hal *= 0.4;
	//
	//		if (priorityMap[x][y] < threshold) {
	//			priorityMap[x][y] = 0;
	//		}
	//		else {
	//			priorityMap[x][y] += near_hal;
	//		}
	//	}
	//}
	

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			Position p = { x, y };

			//if (priorityMap[x][y] < 0)
			//	continue;

			Cell& c = game->map->GetCell(p);
			double friendliness = combat->Friendliness(me, p, game->GetShipAt(p));
			int dist_to_dropoff = closestDropoffDist[p.x][p.y];

			for (auto& sp : me.ships) {
				Ship* s = sp.second;
				if (s->assigned) continue;

				int dist_to_cell = s->pos.ToroidalDistanceTo(p);
				
				double priority = priorityMap[x][y] / (double)(dist_to_cell + 1);
				
				optimizer.InsertEdge(s, p, (long long)priority);
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

void Strategy::SimulateMining(int& cell_halite, bool inspired, int ship_halite, int& halite_mined, int& turns)
{
	int last_ship_halite = ship_halite;

	if (ship_halite > 970) // already full
		return;

	do {
		if (cell_halite == 0) {
			turns += 99999999;
			break;
		}
		int profit = std::ceil(cell_halite * (1.0 / (double)constants::EXTRACT_RATIO));
		//out::Log("cell_halite: " + std::to_string(cell_halite) + " profit: " + std::to_string(profit));
		//if (profit < 35) // not worth it --- usually ~8 iterations
		//	break;
		turns += 1;
		cell_halite -= profit;
		ship_halite += profit * (inspired ? 3 : 1);
		ship_halite = std::min(constants::MAX_HALITE, ship_halite);
		halite_mined += ship_halite - last_ship_halite;
		last_ship_halite = ship_halite;
		if (ship_halite > 970) // ship full
			break;
	} while (true);
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
