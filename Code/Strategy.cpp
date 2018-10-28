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

double priorityEq(int profit, int cost, int turns) {
	//if (profit < cost)
	//	return -1;
	// the CORE of the strategy
	// (profit - cost) = earned
	// earned / turns = profit per turn
	// profit per turn / turns = profit per turn x turns
	return (double)(profit - cost) / (double)(turns * turns);
}

OptimalMiningResult Strategy::MineMaxProfit(int shipHalite, int base_haliteCost, int base_turns, int cellHalite, bool cellInspired)
{
	OptimalMiningResult best = {
		0,
		0,
		0
	};

	int halite_available = cellHalite;
	int halite_acum = shipHalite;
	for (int mining_turns = 0; mining_turns < 30; mining_turns++) {
		double profit_per_turn = priorityEq(halite_acum, base_haliteCost, base_turns + mining_turns);

		OptimalMiningResult possible = {
			profit_per_turn,
			halite_acum,
			mining_turns,
		};
		if (mining_turns == 0) {}
		else if(mining_turns == 1) best = possible;
		else {
			if (possible.profit_per_turn > best.profit_per_turn) {
				best = possible;
			}
		}

		int mined = ceil((1.0 / hlt::constants::EXTRACT_RATIO) * halite_available);
		if (cellInspired) {
			mined *= hlt::constants::INSPIRED_BONUS_MULTIPLIER + 1;
		}
		halite_acum += mined;
		halite_acum = std::min(halite_acum, hlt::constants::MAX_HALITE);
		halite_available -= mined;
	}

	return best;
}

// return the profit per turn of going to the position using the optimal strategy
// if task type == MINE then its going, mine and go to the nearest dropoff
double Strategy::CalculatePriority(Position start, Position destination, int shipHalite)
{
	Player& me = game->GetMyPlayer();

	bool is_dropoff = me.IsDropoff(destination);

	OptimalPathCell destCost = navigation->PathMinCost(start, destination);
	destCost.turns *= 3;
	
	//out::Log("Cost from: " + start.str() + " to " + destination.str() + ": " + std::to_string(destCost.haliteCost) + " in " + std::to_string(destCost.turns));

	if (is_dropoff) {
		return priorityEq(shipHalite, destCost.haliteCost, destCost.turns);
	}
	else {
		// we use dropoff to destination instead in favor of the cache
		OptimalPathCell toDropoffCost = navigation->PathMinCost(me.ClosestDropoff(destination), destination);

		double cost = destCost.haliteCost + toDropoffCost.haliteCost;
		double turns_cost = destCost.turns + toDropoffCost.turns;
		
		const Cell* c = game->map->GetCell(destination);
		OptimalMiningResult mineOptimal = MineMaxProfit(shipHalite, cost, turns_cost, c->halite, c->inspiration);
		return mineOptimal.profit_per_turn;
	}
}

double Strategy::ShipTaskPriority(Ship* s, Task* t)
{
	Player& me = game->GetMyPlayer();

	double priority;
	switch (t->type) {
	case MINE:
	{
		priority = CalculatePriority(s->pos, t->pos, s->halite);
		break;
	}
	case DROP:
	{
		priority = CalculatePriority(s->pos, t->pos, s->halite);

		OptimalPathCell r = navigation->PathMinCost(s->pos, me.ClosestDropoff(s->pos));
		double th = suicide_stage ? 0.0 : 0.9;
		bool gotta_drop = s->halite + 2 * r.haliteCost >= hlt::constants::MAX_HALITE * th;

		if (s->dropping || gotta_drop) {
			priority += 1000;
			s->dropping = true;
		}
		else {
			if (me.IsDropoff(s->pos)) // do not stay on a dropoff
				priority = -INF;
			else
				priority = 0;
		}
		break;
	}
	case TRANSFORM_INTO_DROPOFF:
	{
		if (t->areaInfo.num_ally_ships >= t->areaInfo.num_enemy_ships) {
			OptimalPathCell r = navigation->PathMinCost(s->pos, t->pos);

			Position closestActualDropoff = me.ClosestDropoff(t->pos);
			int distToClosest = closestActualDropoff.ToroidalDistanceTo(t->pos);

			if (distToClosest >= 8) {
				int cost = (hlt::constants::DROPOFF_COST + r.haliteCost) - (s->halite + game->map->GetCell(t->pos)->halite);
				int max_possible_distance = game->map->width / 2 + game->map->height / 2;

				priority = priorityEq(t->areaInfo.avgHalite * 25 * (1.0 - ((double)distToClosest / (double)max_possible_distance)), cost, r.turns);
			}
		}
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