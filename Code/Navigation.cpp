#include "Navigation.hpp"

#include "Game.hpp"
#include "Strategy.hpp"
#include "Optimizer.hpp"

Navigation::Navigation(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

void Navigation::MinCostBFS(Position start, OptimalPathMap& map)
{
	Player& me = game->GetMyPlayer();
	Map* game_map = game->map;

	std::queue<Position> q;

	map.cells[start.x][start.y].haliteCost = 0;
	map.cells[start.x][start.y].turns = 0;
	map.cells[start.x][start.y].tor_dist = 0;
	map.cells[start.x][start.y].expanded = false;
	map.cells[start.x][start.y].added = false;
	q.push(start);

	while (!q.empty()) {
		Position p = q.front();
		q.pop();

		OptimalPathCell& r = map.cells[p.x][p.y];

		r.added = false;
		if (r.expanded) continue;
		r.expanded = true;

		for (const Direction d : DIRECTIONS) {
			Position new_pos = p.DirectionalOffset(d);

			OptimalPathCell new_state;
			new_state.haliteCost = r.haliteCost + game_map->GetCell(p).MoveCost();
			new_state.turns = r.turns + 1;
			new_state.expanded = false;
			new_state.added = true;
			new_state.tor_dist = start.ToroidalDistanceTo(new_pos);

			if (hits[new_pos.x][new_pos.y] == BlockedCell::TRANSIENT) {
				new_state.turns += 4;
			}
			/*
			if (hits[new_pos.x][new_pos.y] == BlockedCell::STATIC ||
				hits[new_pos.x][new_pos.y] == BlockedCell::GHOST) {
				new_state.turns += 4;
			}
			*/

			if (new_state < map.cells[new_pos.x][new_pos.y]) {
				if (!map.cells[new_pos.x][new_pos.y].added)
					q.push(new_pos);
				map.cells[new_pos.x][new_pos.y] = new_state;
			}
		}
	}
}

void Navigation::Clear()
{
	Player& me = game->GetMyPlayer();

	for (int x = 0; x < constants::MAP_WIDTH; x++) {
		for (int y = 0; y < constants::MAP_HEIGHT; y++) {
			hits[x][y] = BlockedCell::EMPTY;
		}
	}

	// fill hit map with enemies
	for (auto& ep : game->players) {
		Player& enemy_player = ep.second;
		if (enemy_player.id == game->my_id) continue;

		// avoid enemy shipyard
		hits[enemy_player.shipyard_position.x][enemy_player.shipyard_position.y] = BlockedCell::STATIC;

		// enemy dropoffs
		for (Position dp : enemy_player.dropoffs) {
			if (strategy->allow_dropoff_collision ||
				// prevent ships colliding into enemy dropoffs if enemies close
				game->map->GetCell(dp).near_info[1].num_enemy_ships > 0) {
				hits[dp.x][dp.y] = BlockedCell::STATIC;
			}
		}

		// enemy ships
		for (auto& es : enemy_player.ships) {
			if (me.IsDropoff(es.second->pos)) {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::EMPTY;
			}
			else {
				hits[es.second->pos.x][es.second->pos.y] = BlockedCell::STATIC;
			}
		}
	}

	// ally ships
	for (auto& sp : me.ships) {
		if (!sp.second->CanMove()) {
			hits[sp.second->pos.x][sp.second->pos.y] = BlockedCell::TRANSIENT;
		}
	}
}

bool Navigation::IsHitFree(const Position pos)
{
	return hits[pos.x][pos.y] == BlockedCell::EMPTY ||
		   hits[pos.x][pos.y] == BlockedCell::GHOST;
}

std::vector<NavigationOption> Navigation::NavigationOptionsForShip(Ship* ship)
{
	Player& me = game->GetMyPlayer();
	Map* game_map = game->map;
	Cell& current_cell = game_map->GetCell(ship->pos);
	Position target = ship->task.position;

	OptimalPathMap map = {};
	MinCostBFS(target, map);

	std::vector<NavigationOption> options;
	
	for (Direction direction : DIRECTIONS_WITH_STILL) {
		Position p = ship->pos.DirectionalOffset(direction);

		if (IsHitFree(p)) {
			NavigationOption option;
			option.direction = direction;
			option.pos = p;
			option.ship = ship;
			option.optionCost = map.cells[p.x][p.y].cost();

			options.push_back(option);
		}
	}

	return options;
}

void Navigation::Navigate(std::vector<Ship*> ships, std::vector<Command>& commands)
{
	out::Log("****************************");
	out::Log("Navigating " + std::to_string(ships.size()) + " ships...");
	out::Stopwatch s("Navigate");

	Player& me = game->GetMyPlayer();

	Optimizer<Ship*, Position> optimizer;
	std::vector<Direction> all_dirs = DIRECTIONS;

	Player::SortByTaskPriority(ships);

	for (Ship* s : ships) {
		if (!s->CanMove()) continue;

		if (strategy->allow_dropoff_collision) {
			bool go_crash = false;
			for (Direction d : DIRECTIONS_WITH_STILL) {
				Position p = s->pos.DirectionalOffset(d);
				if (me.IsDropoff(p)) {
					commands.push_back(MoveCommand(s->ship_id, d));
					go_crash = true;
					break;
				}
			}
			if (go_crash)
				continue;
		}

		auto options = NavigationOptionsForShip(s);

		for (NavigationOption& no : options) {
			optimizer.InsertEdge(s, no.pos, no.optionCost);
		}
	}

	auto result = optimizer.OptimizeOptimal(OptimizerMode::MINIMIZE);
	out::Log("Navigation cost: " + std::to_string(result.total_value));

	for (auto& so : result.assignments) {
		Ship* s = so.first;
		Position p = so.second;
		for (Direction d : DIRECTIONS_WITH_STILL) {
			if (s->pos.DirectionalOffset(d) == p) {
				commands.push_back(MoveCommand(s->ship_id, d));
				hits[p.x][p.y] = p == s->task.position && !me.IsDropoff(p) ? BlockedCell::STATIC : BlockedCell::TRANSIENT;

				//out::Log("### SHIP " + std::to_string(s->ship_id) + " CHOOSE OPTION " + std::to_string(static_cast<int>(d)));
				break;
			}
		}
	}
}
