#include "Strategy.hpp"

#include "Game.hpp"


Strategy::Strategy(Game* game)
{
	this->game = game;
}

void Strategy::PathMinCostFromMap(Position start, OptimalPathMap& map)
{
	std::queue<Position> q;

	map.cells[start.x][start.y] = { 0, 1, false, true, false };
	q.push(start);

	while (!q.empty()) {
		Position p = q.front();
		q.pop();

		OptimalPathResult& r = map.cells[p.x][p.y];

		r.added = false;
		if (r.expanded) continue;
		r.expanded = true;

		const std::vector<Direction> dirs = {
			Direction::EAST,
			Direction::WEST,
			Direction::NORTH,
			Direction::SOUTH,
		};

		for (const Direction d : dirs) {
			Position new_pos = p.DirectionalOffset(d);

			if (map.cells[new_pos.x][new_pos.y].blocked)
				continue;

			OptimalPathResult new_state;

			new_state.haliteCost = r.haliteCost + floor(0.1 * game->map->GetCell(p)->halite);
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.blocked = false;

			if (new_state < map.cells[new_pos.x][new_pos.y]) {
				if (!map.cells[new_pos.x][new_pos.y].added)
					q.push(new_pos);
				map.cells[new_pos.x][new_pos.y] = new_state;
			}
		}
	}
}

// TODO discount the cost of moving from the just mined cell
OptimalPathResult Strategy::PathMinCost(Position start, Position end)
{
	auto it = minCostCache.find(start);
	if (it != minCostCache.end()) {
		return (*it).second.cells[end.x][end.y];
	}

	// not cached
	OptimalPathMap map = {};

	PathMinCostFromMap(start, map);
	minCostCache.emplace(start, map);

	return map.cells[end.x][end.y];
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

OptimalMiningResult Strategy::MineMaxProfit(int shipHalite, int base_haliteCost, int base_turns, int cellHalite)
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

	bool is_dropoff = destination == me.shipyard_position;

	OptimalPathResult destCost = PathMinCost(start, destination);
	
	//out::Log("Cost from: " + start.str() + " to " + destination.str() + ": " + std::to_string(destCost.haliteCost) + " in " + std::to_string(destCost.turns));

	if (is_dropoff) {
		return priorityEq(shipHalite, destCost.haliteCost, destCost.turns);
	}
	else {
		// we use dropoff to destination instead in favor of the cache
		OptimalPathResult toDropoffCost = PathMinCost(me.shipyard_position, destination);

		double cost = destCost.haliteCost + toDropoffCost.haliteCost;
		double turns_cost = destCost.turns + toDropoffCost.turns;


		OptimalMiningResult mineOptimal = MineMaxProfit(shipHalite, cost, turns_cost, game->map->GetCell(destination)->halite);
		return mineOptimal.profit_per_turn;
	}
}

void Strategy::CreateTasks()
{
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
			if (c->halite > 20 && !me.IsDropoff(c->pos)) {
				Task* t = new Task();
				t->id = task_id++;
				t->type = MINE;
				t->pos = { x, y };
				t->max_ships = 1;
				tasks.push_back(t);
			}
		}
	}
}

void Strategy::AssignTasks()
{
	Player& me = game->GetMyPlayer();

	std::queue<Ship*> shipsToAssign;

	for (auto& p : game->GetMyPlayer().ships) {
		shipsToAssign.push(p.second);
		p.second->task_id = -1;
		p.second->task_priority = -1;
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
			double priority = priority = CalculatePriority(s->pos, t->pos, s->halite);

			if (t->type == DROP) {
				OptimalPathResult r = PathMinCost(s->pos, me.shipyard_position);
				double th = suicide_stage ? 0.0 : 0.9;
				bool gotta_drop = s->halite + 2*r.haliteCost >= hlt::constants::MAX_HALITE * th;

				if (s->dropping || gotta_drop) {
					priority += 1000;
					s->dropping = true;
				}
				else {
					if(me.IsDropoff(s->pos)) // do not stay on a dropoff
						priority = -INF;
					else
						priority = 0;
				}
			}

			//
#ifdef DEBUG
			priorityMap[t->pos.x][t->pos.y] = priority;
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

void Strategy::Navigate(std::vector<Command>& commands)
{
	Player& me = game->GetMyPlayer();

	std::set<Ship*> my_ships;
	std::vector<std::vector<bool>> hits(game->map->width, std::vector<bool>(game->map->height, false));

	for (auto& p : me.ships) {
		Ship* s = p.second;

		s->navigation_processed = false;

		if (s->dropping) {
			if (s->pos == me.shipyard_position) {
				s->dropping = false;
			}
		}

		if(s->halite < floor(game->map->GetCell(s->pos)->halite * 0.1)) {
			hits[s->pos.x][s->pos.y] = true;
			commands.push_back(MoveCommand(s->ship_id, Direction::STILL));
			continue;
		}

		if (s->pos == me.shipyard_position) {
			s->task_priority += 1000000;
		}


		my_ships.insert(s);
	}

	OptimalPathMap map;
	
	while (!my_ships.empty()) {
		auto it = my_ships.begin();
		Ship* s = *it;
		my_ships.erase(it);

		// let ships crash on dropoffs
		if (suicide_stage) {
			for (Position dropoff : me.dropoffs) {
				hits[dropoff.x][dropoff.y] = false;
			}
		}

		// ---------------- Navigation
		map = {}; // reset

		Position target = s->pos;
		if (s->task_id != -1) {
			Task* t = tasks[s->task_id];
			target = t->pos;


			out::LogShip(s->ship_id, {
				{ "task_type", t->type },
				{ "task_x", t->pos.x },
				{ "task_y", t->pos.y },
				{ "task_priority", std::to_string(s->task_priority) },
			});
		}

		PathMinCostFromMap(target, map);

		struct Option {
			int toTargetCost;
			Direction direction;
		};

		std::vector<Option> options;

		std::vector<Direction> dirs = {
			Direction::EAST,
			Direction::WEST,
			Direction::NORTH,
			Direction::SOUTH,
			Direction::STILL
		};

		for (const Direction d : dirs) {
			Position pp = s->pos.DirectionalOffset(d);
			options.push_back(Option{
				map.cells[pp.x][pp.y].turns,
				d
			});
		}

		std::sort(options.begin(), options.end(), [](const Option& a, const Option& b) {
			return a.toTargetCost < b.toTargetCost;
		});


		Direction d = Direction::STILL;
		for (const Option& option : options) {
			auto mov_pos = s->pos.DirectionalOffset(option.direction);

			if (!hits[mov_pos.x][mov_pos.y]) {
				d = option.direction;
				break;
			}
		}

		auto mov_pos = s->pos.DirectionalOffset(d);

		hits[mov_pos.x][mov_pos.y] = true;
		s->navigation_processed = true;

		Ship* shipInMovedLocation = me.ShipAt(mov_pos);
		if (shipInMovedLocation) {
			if (!shipInMovedLocation->navigation_processed) {
				// this ship should be the next to process
				shipInMovedLocation->task_priority += 1000000;
			}
		}

		commands.push_back(MoveCommand(s->ship_id, d));
	}
	
	if (!suicide_stage && game->CanSpawnShip() && !hits[me.shipyard_position.x][me.shipyard_position.y]) {
		// Spawneo o no
		/*
		int avg_ships_per_enemies = 0;
		for (auto& p : game->players) {
			avg_ships_per_enemies += p.second.ships.size();
		}
		avg_ships_per_enemies /= game->num_players - 1;
		*/

		if (game->turn < 0.65 * hlt::constants::MAX_TURNS) {
			commands.push_back(SpawnCommand());
		}
	}
}


void Strategy::Execute(std::vector<Command>& commands)
{
	suicide_stage = game->turn > std::max(0.95 * hlt::constants::MAX_TURNS, hlt::constants::MAX_TURNS - game->map->width * 0.6);

	minCostCache.clear();

	{
		out::Stopwatch("Create Tasks");
		CreateTasks();
	}

	{
		out::Stopwatch("Assign Tasks");
		AssignTasks();
	}

	{
		out::Stopwatch("Navigate");
		Navigate(commands);
	}

#ifdef DEBUG
	for (auto& p : minCostCache) {
		json data_map;
		for (int y = 0; y < game->map->height; y++) {
			json data_row;
			for (int x = 0; x < game->map->width; x++) {
				data_row.push_back(p.second.cells[x][y].haliteCost);
			}
			data_map.push_back(data_row);
		}
		out::LogFluorineDebug({
			{ "type", "minCost" },
			{ "position_x", p.first.x },
			{ "position_y", p.first.y }
		}, data_map);
	}
#endif
}
