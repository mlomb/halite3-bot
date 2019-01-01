#include "Combat.hpp"

#include "Game.hpp"
#include "Strategy.hpp"

Combat::Combat(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

double Combat::Friendliness(Player& player, Position position, Ship* skip)
{
	Map* game_map = game->map;
	Cell& cell = game_map->GetCell(position);

	const int DISTANCES = 5; // 0, 1, 2, 3, 4
	double friendliness = 0;

	for (int d = 0; d < DISTANCES; d++) {
		for (auto& kv : cell.near_info[5].all_ships) {
			if (kv.first == d) {
				if (kv.second == skip) continue;
				if (Game::Get()->GetMyPlayer().id == kv.second->player_id) {
					if (kv.second->dropping || kv.second->halite > 950) continue;
				}

				bool ally_ship = kv.second->player_id == player.id;

				double contribution = DISTANCES - d;
				double i = 1.0;
				if (ally_ship) {
					//i -= ((double)kv.second->halite / (double)constants::MAX_HALITE);
				}
				friendliness += i * contribution * (ally_ship ? 1 : -1);
			}
		}
		// Ally Dropoffs are like ships with 0 halite
		for (auto& kv : cell.near_info[DISTANCES - 1].dropoffs_dist) {
			if (kv.first == d && kv.second == player.id && cell.ship_on_cell == nullptr) {
				double contribution = DISTANCES - d;
				double i = 1.0;
				friendliness += i * contribution;
			}
		}
	}

	return friendliness;
}

int Combat::EnemyReachHalite(Player& player, Position position)
{
	int reach_halite = -1;

	for (Direction d : DIRECTIONS_WITH_STILL) {
		Position p = position.DirectionalOffset(d);
		Ship* ship_there = game->GetShipAt(p);
		if (ship_there && ship_there->player_id != player.id) {
			if (reach_halite == -1 || ship_there->halite < reach_halite) {
				reach_halite = ship_there->halite;
			}
		}
	}

	return reach_halite;
}

bool Combat::WillReceiveImminentAttack(Player& player, Position position)
{
	bool check_default = false;
	for (Direction d : DIRECTIONS) {
		Position p = position.DirectionalOffset(d);
		Ship* ship_there = game->GetShipAt(p);
		if (ship_there && ship_there->player_id != player.id) {
			// this ship may attack
			double f = Friendliness(game->players[ship_there->player_id], position, ship_there);
			if (players[ship_there->player_id].engage_friendliness_points.size() < 5) {
				if (f > features::friendliness_should_attack)
					return false;
			}
			else {
				if (f > players[ship_there->player_id].friendliness_engage)
					return true;
			}
		}
	}
	return false;
}

bool Combat::FreeToMove(Player& player, Position position)
{
	bool check_default = false;
	for (Direction d : DIRECTIONS) {
		Position p = position.DirectionalOffset(d);
		Ship* ship_there = game->GetShipAt(p);
		if (ship_there && ship_there->player_id != player.id) {
			// this ship may move to this tile
			if (players[ship_there->player_id].no_dodge_friendliness_points.size() < 5) {
				check_default = true;
			}
			else {
				if (Friendliness(game->players[ship_there->player_id], position, nullptr) > players[ship_there->player_id].friendliness_no_dodge)
					return false;
			}
		}
	}
	if (check_default) {
		double f = Friendliness(player, position, nullptr);
		if (f > features::friendliness_dodge)
			return false;
	}
	return true;
}

json Combat::GetShipOnCellDescription(Player& player, Cell& cell)
{
	Ship* ship = cell.ship_on_cell;

	json ship_json;

	if (ship) {
		bool ally = ship->player_id == player.id;
		ship_json["halite"] = ship->halite;
		ship_json["ally"] = ally;
		if (ally) {
			ship_json["dropping"] = ship->dropping;
			ship_json["dist_to_target"] = ship->pos.ToroidalDistanceTo(ship->task.position);
			ship_json["is_this_target"] = ship->task.position == cell.pos;
			ship_json["target_type"] = static_cast<int>(ship->task.type);
		}
	}

	return ship_json;
}

json Combat::GetCellDescription(Player& player, Cell& cell, json& ships) {
	json cell_json;
	cell_json["halite"] = cell.halite;
	cell_json["inspiration"] = cell.inspiration;
	cell_json["ship_id"] = cell.ship_on_cell ? cell.ship_on_cell->ship_id : -1;
	cell_json["dropoff"] = cell.dropoff_owned != -1 ? (cell.dropoff_owned == player.id ? 1 : -1) : 0;
	cell_json["enemy_reach_halite"] = EnemyReachHalite(player, cell.pos);
	cell_json["imminent_attack"] = WillReceiveImminentAttack(player, cell.pos);
	cell_json["friendliness"] = Friendliness(player, cell.pos, cell.ship_on_cell);
	cell_json["friendliness_excluding"] = Friendliness(player, cell.pos, nullptr);

	json near_info = json::array();
	for (int k = 0; k <= MAX_CELL_NEAR_AREA_INFO; k++) {
		AreaInfo& info = cell.near_info[k];

		json area_info;
		area_info["k"] = k;
		area_info["halite"] = info.halite;
		area_info["cells"] = info.cells;
		area_info["avgHalite"] = info.avgHalite;
		area_info["num_ally_ships"] = info.num_ally_ships;
		area_info["num_enemy_ships"] = info.num_enemy_ships;
		area_info["num_ally_ships_not_dropping"] = info.num_ally_ships_not_dropping;
		
		json all_ships = json::array();
		for (auto& kv : info.all_ships) {
			json a_ship;
			a_ship["k"] = kv.first;
			a_ship["ship_id"] = kv.second->ship_id;
			all_ships.push_back(a_ship);
		}
		area_info["all_ships"] = all_ships;

		near_info.push_back(area_info);
	}
	cell_json["near_info"] = near_info;

	if (cell.ship_on_cell) {
		std::string key = std::to_string(cell.ship_on_cell->ship_id);
		if(ships.find(key) == ships.end())
			ships[key] = GetShipOnCellDescription(player, cell);
	}

	return cell_json;
}

const int MAP_DESCRIPTION_RADIUS = 4;

json Combat::GetMapDescription(Player& player, Position position, json& ships)
{
	json map = json::array();

	int nx = 0, ny = 0;

	for (int y = -MAP_DESCRIPTION_RADIUS; y <= MAP_DESCRIPTION_RADIUS; y++, ny++) {
		json row = json::array();
		for (int x = -MAP_DESCRIPTION_RADIUS; x <= MAP_DESCRIPTION_RADIUS; x++, nx++) {
			Position p = { position.x + x, position.y + y };
			Cell& c = strategy->game->map->GetCell(p);
			json desc = GetCellDescription(player, c, ships);
			desc["position"] = { nx, ny };
			row.push_back(desc);
		}
		map.push_back(row);
	}

	return map;
}

bool Combat::ShouldDodge(Ship* ship, Position position)
{
	bool should_dodge = false;
	Player& player = game->GetPlayer(ship->player_id);

	/// ------------------
	Cell& current_cell = game->map->GetCell(position);
	Cell& moving_cell = game->map->GetCell(position);

	int current_cell_halite_plus_ship = current_cell.halite + (current_cell.ship_on_cell ? current_cell.ship_on_cell->halite : 0);
	int moving_cell_halite_plus_ship = moving_cell.halite + (moving_cell.ship_on_cell ? moving_cell.ship_on_cell->halite : 0);

	double current_cell_friendliness = Friendliness(player, current_cell.pos, current_cell.ship_on_cell);
	double moving_cell_friendliness = Friendliness(player, moving_cell.pos, moving_cell.ship_on_cell);

	bool moving_cell_enemy_ship_there = moving_cell.ship_on_cell && moving_cell.ship_on_cell->player_id != ship->player_id;


	bool can_move;
	can_move |= moving_cell_friendliness > -1;
	//can_move |= moving_cell.friendliness > -11 && moving_cell.halite > 400;
	if (moving_cell_enemy_ship_there) {
		if (moving_cell_friendliness < 7 && current_cell_halite_plus_ship > moving_cell_halite_plus_ship) {
			can_move = false;
		}
	}
	should_dodge = !can_move;

	/// ------------------

	if (false) {
		json ships;
		json j;
		j["map_radius"] = MAP_DESCRIPTION_RADIUS;
		j["map"] = GetMapDescription(player, position, ships);
		j["ships"] = ships;

		Position corner = {
			position.x - MAP_DESCRIPTION_RADIUS,
			position.y - MAP_DESCRIPTION_RADIUS
		};
		Position current_cell = {
			ship->pos.x - corner.x,
			ship->pos.y - corner.y,
		};
		Position moving_cell = {
			position.x - corner.x,
			position.y - corner.y,
		};
		j["current_cell"] = {
			current_cell.x,
			current_cell.y
		};
		j["moving_cell"] = {
			moving_cell.x,
			moving_cell.y
		};

		Cell& c = game->map->GetCell(position);
		double friendliness = strategy->combat->Friendliness(player, position, ship);
		int enemy_reach = strategy->combat->EnemyReachHalite(player, position);
		Ship* ship_there = game->GetShipAt(position);
		bool enemy_there = ship_there && ship_there->player_id != player.id;

		j["naive_result"] = should_dodge;

		ship->database[position] = j;
	}

	return should_dodge;
}

void Combat::Update()
{
	out::Log(".....................");

	// Check last turn events
	for (Event& e : next_turn_events) {
		auto player_ships = game->players[e.player_id].ships;
		auto it = player_ships.find(e.ship_id);
		if (it != player_ships.end()) {
			if ((*it).second->pos == e.pos) {
				/*
				players[e.player_id].data.push_back(e.data);

				double r = e.data[e.data.size() - 1];
				players[e.player_id].safe_friendliness_points.push_back(r);

				PlayerCombat::sample_type s;
				s.set_size(e.data.size() - 1);
				for(int i = 0; i < e.data.size() - 2; i++)
					s(i) = e.data[i];

				double p = players[e.player_id].krls.get_decision_function()(s);

				out::Log("PLAYER #" + std::to_string(e.player_id) + "   REAL: " + std::to_string(r) + "  BEFORE TRAIN PREDICT: " + std::to_string(p) + "     DIFF: " + std::to_string(std::abs(r-p)));

				players[e.player_id].krls.train(s, r);
				*/
				switch (e.type) {
				case EventType::ENGAGE:
					players[e.player_id].engage_friendliness_points.push_back(e.friendliness);
					break;
				case EventType::DODGE:
					players[e.player_id].no_dodge_friendliness_points.push_back(e.friendliness);
					break;
				}
			}
		}
	}

	// Process data
	for (auto& kv : players) {
		// ENGAGE
		{
			auto& pts = kv.second.engage_friendliness_points;
			std::sort(pts.begin(), pts.end());
			int amount = (int)std::ceil((double)pts.size() * 0.15);

			kv.second.friendliness_engage = 0;
			for (int i = 0; i < amount; i++) {
				kv.second.friendliness_engage += pts[i];
			}
			kv.second.friendliness_engage /= amount;
		}
		// DODGE
		{
			auto& pts = kv.second.no_dodge_friendliness_points;
			std::sort(pts.begin(), pts.end());
			int amount = (int)std::ceil((double)pts.size() * 0.15);

			kv.second.friendliness_no_dodge = 0;
			for (int i = 0; i < amount; i++) {
				kv.second.friendliness_no_dodge += pts[i];
			}
			kv.second.friendliness_no_dodge /= amount;
		}
	}

	// Create next turn events
	next_turn_events.clear();
	for (auto& ip : game->players) {
		for (auto& is : ip.second.ships) {
			std::vector<Event> events;
			bool has_a_good_option = false;

			for (Direction d : DIRECTIONS_WITH_STILL) {
				Position p = is.second->pos.DirectionalOffset(d);
				Cell& c = game->map->GetCell(p);
				Ship* ship_there = game->GetShipAt(p);
				double friendliness = Friendliness(ip.second, p, nullptr);

				// ENGAGE (ram into ships)
				if (ship_there && ship_there->player_id != ip.second.id) {
					out::Log("Possible event player " + std::to_string(ip.second.id) + " ship " + std::to_string(is.second->ship_id) + " to " + p.str() + " because ship " + std::to_string(ship_there->ship_id) + " is there.");
					next_turn_events.push_back({
						ip.first,
						is.first,
						p,
						EventType::ENGAGE,
						friendliness,
						{ (double)is.second->halite, (double)ship_there->halite, (double)game->map->GetCell(p).halite, friendliness }
					});
				}
				
				// DODGEING
				int reach_halite = EnemyReachHalite(ip.second, p);
				if (reach_halite < 0) {
					has_a_good_option = true;
					continue;
				}

				if(ship_there == 0 || ship_there->player_id == ip.second.id) {
					events.push_back({
						ip.first,
						is.first,
						p,
						EventType::DODGE,
						friendliness,
						{ (double)is.second->halite,  (double)reach_halite, (double)game->map->GetCell(p).halite, friendliness }
					});
				}
			}

			if (has_a_good_option) {
				for (Event& e : events)
					next_turn_events.push_back(e);
			}
		}
	}

	Report();
}

void Combat::Report()
{
	out::Log(".....................");
	out::Log("    COMBAT REPORT    ");

	for (auto& kv : players) {
		out::Log("Player #" + std::to_string(game->players[kv.first].id) + ":");
		out::Log("friendliness_engage: " + std::to_string(kv.second.friendliness_engage));
		out::Log("friendliness_no_dodge: " + std::to_string(kv.second.friendliness_no_dodge));

		
		out::Log(" -- dodge " + std::to_string(kv.second.no_dodge_friendliness_points.size()) + " data points");
		//for (auto& t : kv.second.friendliness_engage) {
			auto& t = kv.second.no_dodge_friendliness_points;
			std::string line = "";
			for (int i = 0; i < t.size(); i++) {
				if (i)
					line += ",";
				line += std::to_string(t[i]);
			}
			out::Log(line);
		//}
		

		/*
		out::Log("");
		out::Log("TEST KRLS");
		for (auto& t : kv.second.data) {
			PlayerCombat::sample_type s;
			s(0) = t[0] - 0.1;
			s(1) = t[1] + 0.1;
			s(2) = t[2] - 0.1;

			out::Log("REAL: " + std::to_string(t[3]) + "  PREDICTED: " + std::to_string(kv.second.krls.get_decision_function()(s)));
		}*/

		out::Log("");
	}
}
