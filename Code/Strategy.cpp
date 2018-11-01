#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

Strategy::Strategy(Game* game)
{
	this->game = game;
	this->navigation = new Navigation(this);
	this->stage = Stage::MINING;
}

double Strategy::ShipTaskPriority(Ship* s, Task* t)
{
	Player& me = game->GetMyPlayer();

	const Cell* c = game->map->GetCell(t->pos);
	OptimalPathCell toDestCost = navigation->PathMinCost(s->pos, t->pos);

	toDestCost.turns *= 3;

	double priority;
	switch (t->type) {
	case MINE:
	{
		// navigation cost
		OptimalPathCell toDropoffCost = navigation->PathMinCost(me.ClosestDropoff(t->pos), t->pos);

		// mining
		int halite_available = c->halite;
		int halite_acum = s->halite;
		double max_priority = 0;

		for (int mining_turns = 0; mining_turns < 25; mining_turns++) {
			if (mining_turns > 0) {
				/// -----------------------
				double profit = 0;
				double penalty = 0;

				OptimalPathCell combined;
				combined.haliteCost = toDestCost.haliteCost + toDropoffCost.haliteCost;
				combined.turns = toDestCost.turns + toDropoffCost.turns + mining_turns;

				profit = halite_acum + (c->near_info.avgHalite / 100.0) * 50;
				penalty = c->near_info.num_ally_ships * 16;

				double possible_priority = (profit - penalty) / (double)(combined.turns * combined.turns);

				/// -----------------------

				//out::Log(std::to_string(profit) + " -- " + std::to_string(toDestCost.turns) + " -- " + std::to_string(toDropoffCost.turns) + " -- " + std::to_string(mining_turns));

				if (possible_priority > max_priority) {
					max_priority = possible_priority;
				}
			}

			// Mine
			int mined = ceil((1.0 / hlt::constants::EXTRACT_RATIO) * halite_available);
			if (c->inspiration) {
				mined *= hlt::constants::INSPIRED_BONUS_MULTIPLIER + 1;
			}
			halite_acum += mined;
			halite_acum = std::min(halite_acum, hlt::constants::MAX_HALITE);
			halite_available -= mined;
		}

		priority = max_priority;
		break;
	}
	case TRANSFORM_INTO_DROPOFF:
	{
		// priority by distance
		priority = 100000 + std::max(0, 1000 - toDestCost.turns);
		break;
	}
	default:
	{
		priority = -42;
		break;
	}
	}

	return priority;
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

	return game->turn <= 0.65 * hlt::constants::MAX_TURNS;


	/* FULL ML* /
	typedef matrix<double> sample_type;
	typedef radial_basis_kernel<sample_type> kernel_type;
	typedef decision_function<kernel_type> dec_funct_type;
	typedef normalized_function<dec_funct_type> funct_type;

	funct_type learned_function;
	deserialize("saved_function.dat") >> learned_function;

	sample_type sample;
	sample.set_size(7, 1);

	sample(0) = my_halite;
	sample(1) = my_ships;
	sample(2) = enemy_halite;
	sample(3) = enemy_ships;
	sample(4) = game->map->halite_remaining;
	sample(5) = game->total_halite;
	sample(6) = game->turn;

	return learned_function(sample) >= 0;
	*/

	// HARD MAX HALITE COLLECTED
	//if (halite_collected_perc >= 0.5)
	//	return true;

	double margin = 0.05; // 5%

	bool wining = my_halite > enemy_halite;
	bool wining_by_margin  = (my_halite / (double)enemy_halite) >= 1 + margin;
	bool loosing_by_margin = (enemy_halite / (double)my_halite) >= 1 + margin;


	if (wining) {
		// spawn a ship if we are not wining by a considerable margin
		//return !wining_by_margin;
		return enemy_ships + 4 >= my_ships;
	}
	else { // loosing
		// spawn a ship if we are loosing by a considerable margin
		// TODO Think in 4p
		return enemy_ships + 2 >= my_ships;// loosing_by_margin;// || enemy_ships - my_ships >= 1;
	}
}

void Strategy::CreateTasks()
{
	out::Stopwatch s("Create Tasks");

	Map* map = game->map;

	int task_id = 0;

	auto& me = game->GetMyPlayer();

	for (Task* t : tasks)
		delete t;
	tasks.clear();

	for (int x = 0; x < map->width; x++) {
		for (int y = 0; y < map->height; y++) {
			Cell* c = map->GetCell({ x, y });

			bool is_dropoff = false;
			for (auto& p : game->players) {
				is_dropoff |= p.second.IsDropoff(c->pos);
			}

			if (!is_dropoff) {
				if(c->halite > 20) {
					Task* t = new Task();
					t->type = MINE;
					t->pos = { x, y };
					tasks.push_back(t);
				}
			}
		}
	}
	
	if (stage == MINING) {
		if (me.ships.size() >= 22 * me.dropoffs.size() && game->turn <= 0.75 * hlt::constants::MAX_TURNS) {
			// find a good spot for a dropoff
			Position dropoff_pos;
			double bestRatio = -1;

			double mapAvgHalite = game->map->halite_remaining / (double)(game->map->width * game->map->height);
			out::Log(std::to_string(mapAvgHalite));
			for (int x = 0; x < game->map->width; x++) {
				for (int y = 0; y < game->map->height; y++) {
					Position pos = { x, y };
					int distance_to_closest_dropoff = me.DistanceToClosestDropoff(pos);
					if (distance_to_closest_dropoff > map->width * 0.25) {
						AreaInfo info = game->map->GetAreaInfo(pos, 5);
						if (info.num_ally_ships > 0 && info.num_ally_ships >= info.num_enemy_ships) {
							if (info.avgHalite / mapAvgHalite >= 1.25) {
								double ratio = info.avgHalite / (distance_to_closest_dropoff * distance_to_closest_dropoff);
								if (ratio > bestRatio) {
									bestRatio = ratio;
									dropoff_pos = pos;
								}
							}
						}
					}
				}
			}

			// t->areaInfo = map->GetAreaInfo(t->pos, 5);

			if (bestRatio > 0) {
				if (!me.IsDropoff(dropoff_pos)) {
					Task* t = new Task();
					t->type = TRANSFORM_INTO_DROPOFF;
					t->pos = dropoff_pos;
					tasks.push_back(t);
				}
			}
		}
	}

	// Filter tasks to prevent timeout
	const int MAX_TASKS = 9999999999;

	if (tasks.size() > MAX_TASKS) {
		for (Task* t : tasks) {
			t->dist_to_dropoff = me.DistanceToClosestDropoff(t->pos);
		}

		std::sort(tasks.begin(), tasks.end(), [](const Task* a, const Task* b) {
			return a->dist_to_dropoff < b->dist_to_dropoff;
		});

		out::Log("Must remove " + std::to_string(tasks.size() - MAX_TASKS) + " (out of " + std::to_string(tasks.size()) + ") tasks to prevent timeout");

		tasks.resize(MAX_TASKS);
	}
}

void Strategy::AssignTasks()
{
	out::Stopwatch s("Assign Tasks");

	Player& me = game->GetMyPlayer();

	struct Edge {
		double priority;
		Ship* s;
		Task* t;
	};

	std::vector<Edge> edges;

	{
		out::Stopwatch s("Edge creation");
		for (Ship* s : shipsAvailable) {
			if (s->priority > 0) // already assigned
				continue;

#ifdef HALITE_LOCAL
			double priorityMap[64][64];
#endif

			for (Task* t : tasks) {
				double priority = ShipTaskPriority(s, t);
				edges.push_back({
					priority,
					s,
					t
				});
#ifdef HALITE_LOCAL
				priorityMap[t->pos.x][t->pos.y] = priority;
#endif
			}


			/*
#ifdef HALITE_LOCAL
			json data_map;
			for (int y = 0; y < game->map->height; y++) {
				json data_row;
				for (int x = 0; x < game->map->width; x++) {
					data_row.push_back(priorityMap[x][y]);
				}
				data_map.push_back(data_row);
			}
			out::LogFluorineDebug({
				{ "type", "priority" },
				{ "position_x", s->pos.x },
				{ "position_y", s->pos.y }
				}, data_map);
#endif
			*/
		}
	}

	{
		out::Stopwatch s("Edge sorting (" + std::to_string(edges.size()) + ")");

		std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
			return a.priority > b.priority;
		});
	}

	{
		out::Stopwatch s("Edge assignment");

		for (Edge& e : edges) {
			if (e.t->assigned || e.s->task != 0) continue;

			e.s->task = e.t;
			e.s->target = e.t->pos;
			e.s->priority = e.priority;
			e.t->assigned = true;
		}
	}
}

void Strategy::Execute(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();

	//--------- Trigger Suicide stage
	int min_turns = 0;
	for (auto& p : me.ships) {
		min_turns = std::max(min_turns, me.DistanceToClosestDropoff(p.second->pos));
	}
	if (game->turn >= hlt::constants::MAX_TURNS - (int)(min_turns * 1.1)) {
		this->stage = Stage::SUICIDE;
	}

	//-------------------------------
	navigation->Clear();

	// fill shipsAvailable
	shipsAvailable.clear();
	for (auto& p : me.ships) {
		Ship* s = p.second;

		if (s->task) {
			if (s->task->type == TaskType::TRANSFORM_INTO_DROPOFF) {
				if (s->target == s->pos) {
					double prof = s->halite + game->map->GetCell(s->pos)->halite + me.halite;
					if (prof >= hlt::constants::DROPOFF_COST) {
						me.halite = prof - hlt::constants::DROPOFF_COST;
						commands.push_back(TransformShipIntoDropoffCommand(s->ship_id));
						me.dropoffs.push_back(s->pos);
						continue;
					}
				}
			}
		}

		s->task = nullptr;
		s->priority = 0;
		s->policy = EnemyPolicy::DODGE;

		if (me.IsDropoff(s->pos)) {
			s->dropping = false;
		}

		if (me.DistanceToClosestDropoff(s->pos) <= 3) {
			s->policy = EnemyPolicy::IGNORE;
		}

		if (this->stage == Stage::SUICIDE || s->halite >= hlt::constants::MAX_HALITE * 0.95) {
			s->dropping = true;
		}

		if (s->halite < floor(game->map->GetCell(s->pos)->halite * 0.1) ||
			this->stage == Stage::SUICIDE && me.IsDropoff(s->pos)) {
			navigation->hits[s->pos.x][s->pos.y] = BlockedCell::STATIC;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			continue;
		}

		if (s->dropping) {
			s->priority = 10000000 + s->halite;
			s->target = me.ClosestDropoff(s->pos);
		}

		shipsAvailable.push_back(s);
	}

	if (this->stage != Stage::SUICIDE) {
		CreateTasks();
		AssignTasks();
	}

	navigation->Navigate(shipsAvailable, commands);

	//-------------------------------

	// SHIP SPAWNING
	if (this->stage != Stage::SUICIDE && game->CanSpawnShip() && navigation->hits[me.shipyard_position.x][me.shipyard_position.y] == BlockedCell::EMPTY) {
		int dropoff_tasks = 0;
		for (Task* t : tasks) {
			if (t->type == TRANSFORM_INTO_DROPOFF)
				dropoff_tasks++;
		}
		if (me.halite >= dropoff_tasks * hlt::constants::DROPOFF_COST + hlt::constants::SHIP_COST) {
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
	std::sort(ships.begin(), ships.end(), [](const Ship* a, const Ship* b) {
		return a->priority > b->priority;
	});

	auto it = ships.begin();
	if (it == ships.end())
		return nullptr;
	Ship* s = *it;
	return s;
}