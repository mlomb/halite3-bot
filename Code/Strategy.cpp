#include "Strategy.hpp"

#include "Game.hpp"
#include "Navigation.hpp"

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

void Strategy::CreateTasks()
{
	out::Stopwatch s("Create Tasks");

	Map* map = game->map;

	int task_id = 0;

	auto me = game->GetMyPlayer();

	for (Task* t : tasks)
		delete t;
	tasks.clear();

	for (Position dropoff : me.dropoffs) {
		Task* dropTask = new Task();
		dropTask->id = task_id++;
		dropTask->type = DROP;
		dropTask->pos = dropoff;
		dropTask->max_ships = -1;
		tasks.push_back(dropTask);
	}

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
					t->id = task_id++;
					t->type = MINE;
					t->pos = { x, y };
					t->max_ships = 1;
					tasks.push_back(t);
				}

				//if(me.ships.size() >= 12) { // TODO remove?
					Position drp = me.ClosestDropoff({ x, y });
					int dist = drp.ToroidalDistanceTo({ x, y });
					if (dist > 10) { // only create a transform dropoff task if we are more than 10 units away
						continue;
						Task* t = new Task();
						t->id = task_id++;
						t->type = TRANSFORM_INTO_DROPOFF;
						t->pos = { x, y };
						t->max_ships = 1;
						t->areaInfo = map->GetAreaInfo(t->pos, 5);
						tasks.push_back(t);
					}
				//}
			}
		}
	}
}

void Strategy::AssignTasks()
{
	out::Stopwatch s("Assign Tasks");

	Player& me = game->GetMyPlayer();

	std::queue<Ship*> shipsToAssign;
	
	for(Ship* s : shipsAvailable) {
		shipsToAssign.push(s);
		s->task_id = -1;
		s->task_priority = -1;
	}

	while (!shipsToAssign.empty()) {
		Ship* s = shipsToAssign.front();
		shipsToAssign.pop();

#ifdef DEBUG
		double priorityMap[64][64];
#endif

		/* PRIORITY CALCULATION */
		Task* priorizedTask = 0;
		double maxPriority = -INF;
		Ship* otherShipPtrOverriding = 0;
		
		for (Task* t : tasks) {
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

			//
#ifdef DEBUG
			priorityMap[t->pos.x][t->pos.y] = 0;
			if (t->type != TRANSFORM_INTO_DROPOFF) {
				priorityMap[t->pos.x][t->pos.y] = priority;
			}
#endif

			Ship* otherShipPtrWithLessPriority = 0;
			if (t->IsFull()) {
				double otherShipPtrMinPriority = INF;
				for (Ship* otherShipPtr : t->ships) {
					double otherShipPtrPriority = otherShipPtr->task_priority;

					if (priority > otherShipPtrPriority) {
						if (otherShipPtrPriority < otherShipPtrMinPriority) {
							otherShipPtrWithLessPriority = otherShipPtr;
							otherShipPtrMinPriority = otherShipPtrPriority;
						}
					}
				}
				if (!otherShipPtrWithLessPriority)
					continue;
			}

			if (priority > maxPriority) {
				maxPriority = priority;
				priorizedTask = t;
				otherShipPtrOverriding = otherShipPtrWithLessPriority;
			}
		}

#ifdef DEBUG
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

		if (priorizedTask == 0) {
			out::Log("Ship " + std::to_string(s->ship_id) + " couldn't find a suitable task");
			continue;
		}

		out::Log("Ship " + std::to_string(s->ship_id) + " assigned to task " + std::to_string(priorizedTask->id) + " (" + std::to_string(priorizedTask->type) + ") with priority " + std::to_string(maxPriority));
		out::Log(" (where seems to be " + std::to_string(game->map->GetCell(priorizedTask->pos)->halite) + " halite) ");
		
		if (otherShipPtrOverriding) {
			out::Log("... while overriding ship " + std::to_string(otherShipPtrOverriding->ship_id) + " in " + std::to_string(otherShipPtrOverriding->task_id));
			priorizedTask->ships.erase(otherShipPtrOverriding);
			shipsToAssign.push(otherShipPtrOverriding);
			otherShipPtrOverriding->task_id = -1;
			otherShipPtrOverriding->task_priority = 0;
		}

		s->task_id = priorizedTask->id;
		s->task_priority = maxPriority;
		priorizedTask->ships.insert(s);
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

		if (me.IsDropoff(s->pos)) {
			s->dropping = false;
		}

		if (s->halite < floor(game->map->GetCell(s->pos)->halite * 0.1)) {
			navigation->hits[s->pos.x][s->pos.y] = true;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			continue;
		}

		shipsAvailable.push_back(s);
	}

	
	CreateTasks();
	AssignTasks();
	/*
	for (Ship* s : shipsAvailable) {
		s->task_id = -1;
		s->priority = 0;
		if (game->turn == 1 ||  s->pos == s->target) {
			s->target = Position((int)(32.0 * (rand() / (double)RAND_MAX)), (int)(32.0 * (rand() / (double)RAND_MAX)));
		}
	}
	*/

	{
		out::Stopwatch s("Navigate");
		FixTasks();
		PrepareNavigate(commands);
	}

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

void Strategy::FixTasks()
{
	Player& me = game->GetMyPlayer();

	// Fix tasks
	// Targets & Priorities
	for (Ship* s : shipsAvailable) {
		s->target = s->pos;
		s->priority = 0;
		
		if (s->task_id != -1) {
			Task* t = tasks[s->task_id];
			s->target = t->pos;
			s->priority = s->task_priority;
		}
#ifdef DEBUG
		TaskType tt = (TaskType)-1;
		if (s->task_id != -1) {
			Task* t = tasks[s->task_id];
			tt = t->type;
		}
		out::LogShip(s->ship_id, {
			{ "task_type", tt },
			{ "task_x", s->target.x },
			{ "task_y", s->target.y },
			{ "task_priority", std::to_string(s->priority) },
		});
#endif
	}
}

void Strategy::PrepareNavigate(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();
	
	// Adjust priorities
	/*
	for (Ship* s : shipsAvailable) {
		if (me.IsDropoff(s->pos)) {
			// if there is a ship on a dropoff
			// the ship that have the most priority at the sides
			// should execute first
			// then the chain will continue
			std::vector<Ship*> ships_side_dropoff;
			std::vector<Direction> dirs = DIRECTIONS;
			for (const Direction d : dirs) {
				Position pp = s->pos.DirectionalOffset(d);
				Ship* s = me.ShipAt(pp);
				if (s)
					ships_side_dropoff.push_back(s);
			}
			Ship* ship_side = GetShipWithHighestPriority(ships_side_dropoff);
			if (ship_side) {
				s->priority += 10000000;
			}
		}
	}
	*/

	navigation->Navigate(shipsAvailable, commands);
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