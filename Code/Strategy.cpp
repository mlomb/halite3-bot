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

	const Cell* c = game->map->GetCell(t->position);

	switch (t->type) {
	default:
	case TaskType::NONE:
	{
		return 0.00001;
	}
	case TaskType::TRANSFORM_INTO_DROPOFF:
	{
		// priority by distance
		OptimalPathCell toDestCost = navigation->PathMinCost(s->pos, t->position);
		return 1000000 + std::max(0, 10000 - toDestCost.turns);
	}
	case TaskType::DROP:
	{
		if (s->halite >= 950 || s->dropping) {
			// only drop on the closest dropoff
			if (me.ClosestDropoff(s->pos) == t->position) {
				s->dropping = true;
				return 10000 + s->halite;
			}
		}
		return 0;
	}
	case TaskType::ATTACK:
	{
		Ship* enemy_ship = c->ship_on_cell;

		if (navigation->ShouldAttack(s->halite, c->near_info_3.num_ally_ships, enemy_ship->halite, c->near_info_3.num_enemy_ships)) {
			OptimalPathCell toDestCost = navigation->PathMinCost(s->pos, t->position);
			toDestCost.turns *= 2;

			int dif = c->near_info_3.num_ally_ships - c->near_info_3.num_enemy_ships;
			return (enemy_ship->halite - s->halite) / (double)(toDestCost.turns * toDestCost.turns) * (double)dif * 0.8;
		}
		return 0;
	}
	case TaskType::MINE:
	{
		if (c->halite <= 35)
			return 0;
		if (s->dropping)
			return 0;

		// navigation cost
		OptimalPathCell toDestCost = navigation->PathMinCost(s->pos, t->position);
		toDestCost.turns *= 3;
		OptimalPathCell toDropoffCost = navigation->PathMinCost(me.ClosestDropoff(t->position), t->position);

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

				profit = halite_acum + (c->near_info_4.avgHalite / 100.0) * 65;
				penalty = c->near_info_4.num_ally_ships * 16;
				penalty = c->near_info_4.num_enemy_ships * 2;
				double revenue = ((profit - penalty) / game->map->map_avg_halite) * 100.0;

				double possible_priority = revenue / (double)(combined.turns * combined.turns);

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

		return max_priority;
	}
	}

	return 0;
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
}

Position Strategy::BestDropoffSpot()
{
	auto& me = game->GetMyPlayer();

	Position best_dropoff;
	best_dropoff.x = -1;
	best_dropoff.y = -1;

	if (stage == Stage::MINING) {
		if (me.ships.size() >= 21 * me.dropoffs.size() && game->turn <= 0.75 * hlt::constants::MAX_TURNS) {
			// find a good spot for a dropoff
			double bestRatio = -1;

			for (int x = 0; x < game->map->width; x++) {
				for (int y = 0; y < game->map->height; y++) {
					Position pos = { x, y };
					int distance_to_closest_dropoff = me.DistanceToClosestDropoff(pos);
					if (distance_to_closest_dropoff > std::ceil(game->map->width * 0.35)) {
						AreaInfo info = game->map->GetAreaInfo(pos, 5);
						if (info.num_ally_ships > 0 && info.num_ally_ships >= info.num_enemy_ships) {
							if (info.avgHalite / game->map->map_avg_halite >= 1.35) {
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
	}

	return best_dropoff;
}

void Strategy::GenerateTasks() {
	out::Log("Generating tasks...");
	out::Stopwatch s("Generate Tasks");

	auto& me = game->GetMyPlayer();

	for (Task* t : tasks)
		delete t;
	tasks.clear();

	Position next_dropoff = BestDropoffSpot();

	for (int x = 0; x < game->map->width; x++) {
		for (int y = 0; y < game->map->height; y++) {
			Task* task = new Task();
			task->type = TaskType::NONE;
			task->policy = EnemyPolicy::DODGE;
			task->position = { x, y };
			task->max_ships = 0;

			if (me.IsDropoff(task->position)) {
				task->type = TaskType::DROP;
				task->policy = EnemyPolicy::DODGE;
				task->max_ships = INF;
			}
			else if (next_dropoff == task->position) {
				task->type = TaskType::TRANSFORM_INTO_DROPOFF;
				task->policy = EnemyPolicy::DODGE;
				task->max_ships = 1;
			}
			else {
				task->type = TaskType::MINE;
				task->policy = EnemyPolicy::ENGAGE;
				task->max_ships = 1;
			}

			tasks.push_back(task);
		}
	}

	for(auto& pp : game->players) {
		if (pp.first == me.id) continue;
		for (auto& ss : pp.second.ships) {
			Task* attackTask = new Task();
			attackTask->type = TaskType::ATTACK;
			attackTask->policy = EnemyPolicy::ENGAGE;
			attackTask->position = ss.second->pos;
			attackTask->max_ships = 2;
			tasks.push_back(attackTask);
		}
	}

	Task* dumbTask = new Task();
	dumbTask->type = TaskType::NONE;
	dumbTask->policy = EnemyPolicy::DODGE;
	dumbTask->position = me.shipyard_position;
	dumbTask->max_ships = INF;
	tasks.push_back(dumbTask);
}

void Strategy::AssignTasks()
{
	out::Log("Assigning tasks...");
	out::Stopwatch s("Assign Tasks");

	Player& me = game->GetMyPlayer();

	struct Edge {
		double priority;
		Ship* s;
		Task* t;
	};

	std::vector<Edge> edges;
	edges.reserve(shipsAvailable.size() * tasks.size());

	{
		out::Stopwatch s("Edge creation");

		for (Ship* s : shipsAvailable) {
			for(Task* t : tasks) {
				double priority = ShipTaskPriority(s, t);
				if (priority < 0) continue;

				edges.push_back({
					priority,
					s,
					t
				});
			}
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
		int none_tasks = 0;

		for (Edge& e : edges) {
			if (e.t->assigned.size() >= e.t->max_ships || e.s->task != nullptr) continue;

			e.s->task = e.t;
			e.s->priority = e.priority;

			if (e.t->type == TaskType::NONE)
				none_tasks++;

			e.t->assigned.push_back(e.s);
		}

		if (none_tasks > 0 && none_tasks / double(shipsAvailable.size()) >= 0.75) {
			this->stage = Stage::SUICIDE;
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
	if (game->turn >= hlt::constants::MAX_TURNS - (min_turns * 1.1)) {
		this->stage = Stage::SUICIDE;
	}

	//-------------------------------
	navigation->Clear();
	
	// fill shipsAvailable
	shipsAvailable.clear();
	for (auto& p : me.ships) {
		Ship* s = p.second;

		if (s->task && s->task->type == TaskType::TRANSFORM_INTO_DROPOFF) {
			if (s->task->position == s->pos) {
				if (game->TransformIntoDropoff(s, commands)) {
					continue;
				}
			}
		}

		s->task = nullptr;
		s->priority = 0;

		if (me.IsDropoff(s->pos)) {
			s->dropping = false;
		}
		if (this->stage == Stage::SUICIDE) {
			s->dropping = true;
		}

		shipsAvailable.push_back(s);
	}

	GenerateTasks();
	AssignTasks();

	navigation->Navigate(shipsAvailable, commands);

	//------------------------------- SHIP SPAWNING

	if (this->stage != Stage::SUICIDE && game->CanSpawnShip() && navigation->hits[me.shipyard_position.x][me.shipyard_position.y] == BlockedCell::EMPTY) {
		int dropoffs_cost = 0;
		for (Task* t : tasks) {
			if (t->type == TaskType::TRANSFORM_INTO_DROPOFF && t->assigned.size() > 0) {
				dropoffs_cost += hlt::constants::DROPOFF_COST - (*t->assigned.begin())->halite - game->map->GetCell(t->position)->halite;
			}
		}
		if (me.halite >= dropoffs_cost + hlt::constants::SHIP_COST) {
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