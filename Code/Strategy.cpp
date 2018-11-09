#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

const int DROP_THRESHOLD = 970;

Strategy::Strategy(Game* game)
{
	this->game = game;
	this->navigation = new Navigation(this);
}

bool Strategy::ShouldSpawnShip()
{
	auto& me = game->GetMyPlayer();

	int total_ships = 0;
	int enemy_halite = 0;
	int enemy_ships = 0;
	int my_halite = me.TotalHalite();
	int my_ships = me.ships.size();

	for (auto& pp : game->players) {
		total_ships += pp.second.ships.size();
		if (pp.first != me.id) {
			int hal = pp.second.TotalHalite();
			if (hal > enemy_halite) {
				enemy_halite = hal;
				enemy_ships = pp.second.ships.size();
			}
		}
	}

	double halite_collected_perc = game->map->halite_remaining / (double)game->total_halite;

	// HARD MAX TURNS
	if (game->turn >= 0.8 * hlt::constants::MAX_TURNS)
		return false;

	// HARD MIN TURNS
	if (game->turn < 0.2 * hlt::constants::MAX_TURNS)
		return true;

	// HARD MAX COLLECTED
	if (game->map->halite_remaining / (double)(game->map->width * game->map->height) < 60)
		return false;

	return game->turn <= 0.65 /* TODO: A feature? */ * hlt::constants::MAX_TURNS;
}

std::vector<Position> Strategy::BestDropoffSpots()
{
	auto& me = game->GetMyPlayer();

	Position best_dropoff;
	best_dropoff.x = -1;
	best_dropoff.y = -1;

	if (me.ships.size() >= features::dropoff_ships_needed * me.dropoffs.size() && game->turn <= 0.75 * hlt::constants::MAX_TURNS) {
		// find a good spot for a dropoff
		double bestRatio = -1;

		for (int x = 0; x < game->map->width; x++) {
			for (int y = 0; y < game->map->height; y++) {
				Position pos = { x, y };
				int distance_to_closest_dropoff = closestDropoffDist[x][y];
				if (distance_to_closest_dropoff > std::ceil(game->map->width * features::dropoff_map_distance)) {
					AreaInfo info = game->map->GetAreaInfo(pos, 5);
					if (info.num_ally_ships > 0 && info.num_ally_ships >= info.num_enemy_ships) {
						if (info.avgHalite / game->map->map_avg_halite >= features::dropoff_avg_threshold) {
							double ratio = info.avgHalite / (distance_to_closest_dropoff * distance_to_closest_dropoff);
							if (ratio > bestRatio) {
								bestRatio = ratio;
								best_dropoff = pos;
							}
						}
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
	out::Stopwatch s("Assign tasks");
	out::Log("Assigning tasks...");

	Player& me = game->GetMyPlayer();
	std::vector<Position> dropoffs = BestDropoffSpots();

	reserved_halite = 0;
	allow_dropoff_collision = false;

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
		// TODO Fix for multiple dropoff spots
		Ship* dropoff_ship = me.ClosestShipAt(dropoff_spot);

		dropoff_ship->assigned = true;

		dropoff_ship->task.position = dropoff_spot;
		dropoff_ship->task.type = TaskType::TRANSFORM_INTO_DROPOFF;
		dropoff_ship->task.policy = EnemyPolicy::DODGE;
		dropoff_ship->task.priority = dropoff_ship->halite;

		reserved_halite += hlt::constants::DROPOFF_COST - dropoff_ship->halite - game->map->GetCell(dropoff_spot)->halite;

		shipsToNavigate.push_back(dropoff_ship);
	}

	FillClosestDropoffDist();

	/* DROPS */
	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;

		int time_to_drop = closestDropoffDist[s->pos.x][s->pos.y];
		if (game->turn + time_to_drop * 1.2 >= hlt::constants::MAX_TURNS) {
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
	// Nothing yet ;)

	/* MINING */
	struct Edge {
		Ship* s;

		Position position;
		double priority;
		int time_travel;
		int time_mining;
	};

	static std::vector<Edge> edges;
	edges.clear();
	edges.reserve(me.ships.size() * game->map->width * game->map->height);

	// Create mining edges
	for (auto& sp : me.ships) {
		Ship* s = sp.second;
		if (s->assigned) continue;

#ifdef HALITE_LOCAL
		double miningPriorityMap[64][64];
#endif

		for (int x = 0; x < game->map->width; x++) {
			for (int y = 0; y < game->map->height; y++) {
				Position p = { x, y };
				if (!game->IsDropoff(p)) {
					Cell* c = game->map->GetCell(p);

					if (c->halite > 35) {
						Edge edge;
						edge.s = s;
						edge.position = p;

						int dist_to_cell = s->pos.ToroidalDistanceTo(p);
						int dist_to_dropoff = closestDropoffDist[p.x][p.y];
						int halite_available = c->halite;
						int halite_ship = s->halite;
						int halite_mined = 0;

						edge.priority = 0;
						edge.time_travel = 0;
						edge.time_mining = 0;

						for (int mining_turns = 1; mining_turns <= 20; mining_turns++) {
							int mined = ceil((1.0 / hlt::constants::EXTRACT_RATIO) * halite_available);
							int mined_profit = mined;
							if (c->inspiration) {
								mined_profit *= hlt::constants::INSPIRED_BONUS_MULTIPLIER + 1;
							}

							halite_ship += mined_profit;
							halite_ship = std::min(halite_ship, hlt::constants::MAX_HALITE);
							halite_available -= mined;
							halite_mined += mined_profit;

							double time_cost = 0;
							double profit = 0;

							//time_cost = dist_to_cell * features::time_cost_dist_target + dist_to_dropoff * features::time_cost_dist_dropoff + mining_turns * features::time_cost_mining;
							//time_cost = dist_to_cell * 3 + dist_to_dropoff + mining_turns;
							time_cost = dist_to_cell * 3 + dist_to_dropoff * 0.2 + mining_turns * 5;
							//time_cost = dist_to_cell * 6.66 + dist_to_dropoff * 2.22 + mining_turns * 8.88;
							time_cost = time_cost;
							profit = halite_ship;

							double ratio = profit / time_cost;

							if (ratio > edge.priority) {
								edge.priority = ratio;
								edge.time_travel = dist_to_cell;
								edge.time_mining = mining_turns;
							}

							if (halite_ship >= DROP_THRESHOLD) {
								break;
							}
						}

						if (edge.priority > 0) {
#ifdef HALITE_LOCAL
							miningPriorityMap[p.x][p.y] = edge.priority;
#endif
							edges.push_back(edge);
						}
					}
				}
			}
		}

#ifdef HALITE_LOCAL
		json data_map;
		for (int y = 0; y < game->map->height; y++) {
			json data_row;
			for (int x = 0; x < game->map->width; x++) {
				data_row.push_back(miningPriorityMap[x][y]);
			}
			data_map.push_back(data_row);
		}
		out::LogFluorineDebug({
			{ "type", "priority" },
			{ "position_x", s->pos.x },
			{ "position_y", s->pos.y }
			}, data_map);
#endif
	}

	out::Log("Edges: " + std::to_string(edges.size()));

	// Sort edges
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		if (std::fabs(a.priority - b.priority) < 0.00001)
			return a.time_travel < b.time_travel;
		else
			return a.priority > b.priority;
	});

	// Assign mining
	static bool mining_assigned[64][64]; // 0=not assigned, X=available in X turns
	for (int x = 0; x < game->map->width; x++)
		for (int y = 0; y < game->map->height; y++)
			mining_assigned[x][y] = false;

	for (Edge& e : edges) {
		if (e.s->assigned) continue;

		if (!mining_assigned[e.position.x][e.position.y]) {
			e.s->assigned = true;

			e.s->task.position = e.position;
			e.s->task.type = TaskType::MINE;
			e.s->task.policy = EnemyPolicy::ENGAGE;
			e.s->task.priority = e.priority;

			shipsToNavigate.push_back(e.s);

			mining_assigned[e.position.x][e.position.y] = true;
		}
	}
}
 
void Strategy::Execute(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();

	//-------------------------------
	navigation->Clear();
	shipsToNavigate.clear();

	AssignTasks(commands);

	navigation->Navigate(shipsToNavigate, commands);
	//------------------------------- SHIP SPAWNING

	if (game->CanSpawnShip() && navigation->hits[me.shipyard_position.x][me.shipyard_position.y] == BlockedCell::EMPTY) {
		if (me.halite >= reserved_halite + hlt::constants::SHIP_COST) {
			// We CAN spawn a ship
			// We should?
			if (ShouldSpawnShip()) {
				commands.push_back(SpawnCommand());
			}
		}
	}
}

Ship* Strategy::GetShipWithHighestPriority(std::vector<Ship*>& ships)
{
	Player::SortByTaskPriority(ships);

	auto it = ships.begin();
	if (it == ships.end())
		return nullptr;
	Ship* s = *it;
	return s;
}

void Strategy::FillClosestDropoffDist()
{
	Player& me = game->GetMyPlayer();

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			closestDropoffDist[x][y] = me.DistanceToClosestDropoff({ x, y });
		}
	}
}
