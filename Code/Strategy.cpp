#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

#include "munkres/munkres.h"
#include "munkres/matrix.h"

Strategy::Strategy(Game* game)
{
	this->game = game;
	this->navigation = new Navigation(this);
}

double Strategy::ShipTaskPriority(Ship* s, Task* t)
{
	Player& me = game->GetMyPlayer();

	double priority;
	switch (t->type) {
	case MINE:
	{
		const Cell* c = game->map->GetCell(t->pos);

		// navigation cost
		OptimalPathCell toDestCost = navigation->PathMinCost(s->pos, t->pos);
		OptimalPathCell toDropoffCost = navigation->PathMinCost(me.ClosestDropoff(t->pos), t->pos);

		// mining
		int halite_available = c->halite;
		int halite_acum = s->halite;
		double max_priority = 0;

		for (int mining_turns = 0; mining_turns < 25; mining_turns++) {
			if (mining_turns > 0) {
				/// -----------------------
				double profit = halite_acum;

				OptimalPathCell combined;
				combined.haliteCost = toDestCost.haliteCost + toDropoffCost.haliteCost;
				combined.turns = toDestCost.turns + toDropoffCost.turns + mining_turns;
				
				double possible_priority = profit / combined.ratio();

				possible_priority += game->map->GetAreaInfo(t->pos, 4).avgHalite / 10000.0;
				/// -----------------------

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
	default:
	{
		priority = -42;
		break;
	}
	}

	return priority;
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

				//if(me.ships.size() >= 12) { // TODO remove?
					Position drp = me.ClosestDropoff({ x, y });
					int dist = drp.ToroidalDistanceTo({ x, y });
					if (dist > 10) { // only create a transform dropoff task if we are more than 10 units away
						continue;
						Task* t = new Task();
						t->type = TRANSFORM_INTO_DROPOFF;
						t->pos = { x, y };
						t->areaInfo = map->GetAreaInfo(t->pos, 5);
						tasks.push_back(t);
					}
				//}
			}
		}
	}

	// Filter tasks to prevent timeout
	const int MAX_TASKS = 800;

	if (tasks.size() > MAX_TASKS) {
		for (Task* t : tasks) {
			t->dist_to_dropoff = me.ClosestDropoff(t->pos).ToroidalDistanceTo(t->pos);
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
	double BIG_NUM = 10000000000000;
	out::Stopwatch s("Assign Tasks");

	Player& me = game->GetMyPlayer();

	int ships_num = shipsAvailable.size(), tasks_num = tasks.size();

	if (ships_num == 0 || tasks_num == 0) return;

	Matrix<double> priorityMatrix(ships_num, tasks_num);

	for (int r = 0; r < ships_num; r++) {
		Ship* s = shipsAvailable[r];

		if (s->priority == -1)
			continue;

		for (int t = 0; t < tasks_num; t++) {
			priorityMatrix(r, t) = BIG_NUM - ShipTaskPriority(s, tasks[t]);
		}
	}

	Munkres<double> m;
	Matrix<double> result = priorityMatrix;
	{
		out::Stopwatch s("Munkres solve");
		m.solve(result);
	}

	for (int r = 0; r < ships_num; r++) {
		Ship* s = shipsAvailable[r];

		if (s->priority == -1)
			continue;

		int task_i = -1;
		for (int t = 0; t < tasks_num; t++) {
			if(result(r, t) == 0) {
				task_i = t;
				break;
			}
		}

		if (task_i != -1) {
			Task* t = tasks[task_i];

			s->task = t;
			s->target = t->pos;
			s->priority = -(priorityMatrix(r, task_i) - BIG_NUM);

#ifdef HALITE_LOCAL
			out::LogShip(s->ship_id, {
				{ "task_type", t->type },
				{ "task_x", s->target.x },
				{ "task_y", s->target.y },
				{ "task_priority", std::to_string(s->priority) },
				});
#endif
		}
		else {
			// no tasks, prob end game
			// go to base
			s->target = me.ClosestDropoff(s->pos);
		}
	}
}

void Strategy::Execute(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();
	int min_turns = 0;
	for (auto& p : me.ships) {
		if (p.second->halite > 50) {
			int dist = p.second->pos.ToroidalDistanceTo(me.ClosestDropoff(p.second->pos));
			min_turns = std::max(min_turns, dist);
		}
	}
	suicide_stage = game->turn >= std::min((int)(0.95 * hlt::constants::MAX_TURNS), hlt::constants::MAX_TURNS - (int)(min_turns * 1.1));

	//-------------------------------
	navigation->Clear();

	// fill shipsAvailable
	shipsAvailable.clear();
	for (auto& p : me.ships) {
		Ship* s = p.second;

		s->task = nullptr;
		s->priority = 0;

		if (me.IsDropoff(s->pos)) {
			s->dropping = false;
		}

		if (suicide_stage || s->halite >= hlt::constants::MAX_HALITE * 0.95) {
			s->dropping = true;
		}

		if (s->halite < floor(game->map->GetCell(s->pos)->halite * 0.1)) {
			navigation->hits[s->pos.x][s->pos.y] = true;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			continue;
		}

		if (s->dropping) {
			s->priority = -1; // -1 DO NOT ASSIGN ANY TASK
			s->target = me.ClosestDropoff(s->pos);
		}

		shipsAvailable.push_back(s);
	}

	if (!suicide_stage) {
		CreateTasks();
		AssignTasks();
	}

	navigation->Navigate(shipsAvailable, commands);

	//-------------------------------

	// SHIP SPAWNING
	if (!suicide_stage && game->CanSpawnShip() && !navigation->hits[me.shipyard_position.x][me.shipyard_position.y]) {
		if (me.halite >= hlt::constants::SHIP_COST) {
			if (game->turn < 0.65 * hlt::constants::MAX_TURNS) {
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