#include "Combat.hpp"

#include "Game.hpp"
#include "Strategy.hpp"

Combat::Combat(Strategy* strategy)
	: strategy(strategy), game(strategy->game)
{
}

bool Combat::IsSafe(Position p, bool may_attack)
{
	const int SAFE_DIST = 3;
	Cell& c = game->map->GetCell(p);

	if (c.near_info[SAFE_DIST].num_enemy_ships == 0)
		return true;

	for (int d = 1; d <= SAFE_DIST; d++) {
		int a = c.near_info[d].num_enemy_ships;
		int b = c.near_info[d].num_ally_ships_not_dropping;

		bool s = may_attack ? a >= b : a > b;

		if (s) {
			return false;
		}
	}
	return true;
}

int Combat::FriendlinessNew(Player& player, Position position, Ship* skip)
{
	Map* game_map = game->map;
	Cell& cell = game_map->GetCell(position);

	const int SAFE_DIST = 3;
	const double contributions[SAFE_DIST + 1] = { 8, 8, 3, 1 };

	int friendliness = 0;

	for (int d = 0; d <= SAFE_DIST; d++) {
		for (auto& kv : cell.near_info[5].all_ships) {
			if (kv.first == d) {
				if (kv.second->halite >= 900) continue;
				if (kv.second == skip) continue;

				bool ally_ship = kv.second->player_id == player.id;

				friendliness += contributions[d] * (ally_ship ? 1 : -1);
			}
		}
	}

	return friendliness;
}

double Combat::Friendliness(Player& player, Position position, bool ignore_position)
{
	Map* game_map = game->map;
	Cell& cell = game_map->GetCell(position);

	const int DISTANCES = 4; // 0, 1, 2, 3
	const double contributions[DISTANCES] = { 8, 4, 2, 1 };

	double friendliness = 0;

	for (int d = 0; d < DISTANCES; d++) {
		for (auto& kv : cell.near_info[5].all_ships) {
			if (kv.first == d) {
				if (kv.second->dropping) continue;
				if (ignore_position) {
					if (kv.second->pos == position && kv.second->player_id == player.id) {
						continue;
					}
				}

				bool ally_ship = kv.second->player_id == player.id;

				//double i = 1.0 - ((double)kv.second->halite / (double)constants::MAX_HALITE);
				//i = std::max(i, 0.35);
				friendliness += /* i */ 1.0 * contributions[d] * (ally_ship ? 1 : -1);
			}
		}
		/*
		// Ally Dropoffs are like ships with 0 halite
		for (auto& kv : cell.near_info[5].dropoffs_dist) {
			if (kv.first == d && kv.second == player.id) {
				double contribution = DISTANCES - d;
				double i = 1.0;
				friendliness += i * contributions[d];
			}
		}
		*/
	}

	return friendliness;
}

int Combat::EnemyReachHalite(Player& player, Position position)
{
	int reach_halite = -1;

	for (Direction d : DIRECTIONS) {
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
			double f = Friendliness(game->players[ship_there->player_id], position);
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
				if (Friendliness(game->players[ship_there->player_id], position) > players[ship_there->player_id].friendliness_no_dodge)
					return false;
			}
		}
	}
	if (check_default) {
		double f = Friendliness(player, position);
		if (f > features::friendliness_dodge)
			return false;
	}
	return true;
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
				double friendliness = Friendliness(ip.second, p);

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
