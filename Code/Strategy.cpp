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
	int dist = s->pos.ToroidalDistanceTo(t->position);

	const int DROP_THRESHOLD = 970;

	switch (t->type) {
	default:
	case TaskType::NONE:
	{
		return 0.00001;
	}
	case TaskType::TRANSFORM_INTO_DROPOFF:
	{
		// priority by distance
		return 1000000 + std::max(0, 10000 - dist);
	}
	case TaskType::DROP:
	{
		if (s->halite >= DROP_THRESHOLD || s->dropping) {
			// only drop on the closest dropoff
			if (me.ClosestDropoff(s->pos) == t->position) {
				s->dropping = true;
				return 100000 + s->halite;
			}
		}
		return 0;
	}
	case TaskType::ATTACK:
	{
		/*
		Ship* enemy_ship = c->ship_on_cell;

		if (navigation->ShouldAttack(s->halite, c->near_info_3.num_ally_ships, enemy_ship->halite, c->near_info_3.num_enemy_ships)) {
			int dif = c->near_info_3.num_ally_ships - c->near_info_3.num_enemy_ships;
			return ((enemy_ship->halite - s->halite) / (double)(dist * dist)) * (double)dif * 0.8;
		}
		*/
		return 0;
	}
	case TaskType::MINE:
	{
		if (c->halite <= 35)
			return 0;
		if (s->dropping)
			return 0;

		int halite_available = c->halite;
		int halite_ship = s->halite;

		double best_priority = 0;

		for (int mining_turns = 1; mining_turns <= 20; mining_turns++) {
			// Mine
			int mined = ceil((1.0 / hlt::constants::EXTRACT_RATIO) * halite_available);
			int mined_profit = mined;
			if (c->inspiration) {
				mined_profit *= hlt::constants::INSPIRED_BONUS_MULTIPLIER + 1;
			}

			halite_ship += mined_profit;
			halite_ship = std::min(halite_ship, hlt::constants::MAX_HALITE);
			halite_available -= mined;

			double time_cost = dist * features::time_cost_dist_target + me.DistanceToClosestDropoff(t->position) * features::time_cost_dist_dropoff + mining_turns * features::time_cost_mining;
			double profit = (c->near_info_4.avgHalite / game->map->map_avg_halite) * features::mine_avg_profit + halite_ship * features::mine_halite_ship_profit + c->near_info_4.num_ally_ships * features::mine_ally_ships_profit + c->near_info_4.num_enemy_ships * features::mine_enemy_ships_profit;

			double p = profit / time_cost;

			if (p > best_priority) {
				best_priority = p;
			}

			if (halite_ship >= DROP_THRESHOLD) {
				break;
			}
		}

		return best_priority;
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

	return game->turn <= 0.65 /* TODO: A feature? */ * hlt::constants::MAX_TURNS;
}

Position Strategy::BestDropoffSpot()
{
	auto& me = game->GetMyPlayer();

	Position best_dropoff;
	best_dropoff.x = -1;
	best_dropoff.y = -1;

	if (stage == Stage::MINING) {
		if (me.ships.size() >= features::dropoff_ships_needed * me.dropoffs.size() && game->turn <= 0.75 * hlt::constants::MAX_TURNS) {
			// find a good spot for a dropoff
			double bestRatio = -1;

			for (int x = 0; x < game->map->width; x++) {
				for (int y = 0; y < game->map->height; y++) {
					Position pos = { x, y };
					int distance_to_closest_dropoff = me.DistanceToClosestDropoff(pos);
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

	/*
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
	*/

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

	out::Log("Edges: " + std::to_string(edges.size()));

	{
		out::Stopwatch s("Edge sorting");

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

		/*
		if (none_tasks > 0 && none_tasks / double(shipsAvailable.size()) >= 0.75) {
			this->stage = Stage::SUICIDE;
		}
		*/
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