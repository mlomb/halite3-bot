#include "Game.hpp"

#include "Command.hpp"
#include "Strategy.hpp"

namespace features {
	double time_cost_dist_target;
	double time_cost_dist_dropoff;
	double time_cost_mining;

	double dropoff_ships_needed;
	double dropoff_map_distance;
	double dropoff_avg_threshold;

	double attack_mult;
	double enemy_halite_worth;
	double min_ally_ships_near;
	double ally_enemy_ratio;
	double ally_halite_less;
	double halite_ratio_less;
}

Game* Game::s_Instance = nullptr;

Game::Game()
{
	s_Instance = this;
	strategy = new Strategy(this);
	map = new Map(this);
}

void Game::Initialize(const std::string& bot_name)
{
	std::ios_base::sync_with_stdio(false);

	hlt::constants::populate_constants(in::GetString());
	std::stringstream input(in::GetString());
	input >> num_players >> my_id;

	out::Open(my_id);

	for (int i = 0; i < num_players; i++) {
		PlayerID player_id;
		Position shipyard_position;
		in::GetSStream() >> player_id >> shipyard_position.x >> shipyard_position.y;
		players[player_id] = Player(player_id, shipyard_position);
	}

	map->Initialize();

	total_halite = map->halite_remaining;
	
	std::cout << bot_name << std::endl;

	// INFO
	out::Log("----------------------------");
	out::Log("Bot: " + bot_name);
	out::Log("Num players: " + std::to_string(num_players));
	out::Log("Map: " + std::to_string(map->width) + "x" + std::to_string(map->height));
	out::Log("Total Halite: " + std::to_string(total_halite));
	out::Log("----------------------------");
}

void Game::LoadFeatures(json& features)
{
	out::Log("Features:");
	for (json::iterator it = features.begin(); it != features.end(); ++it) {
		std::string key = it.key();
		double value = it.value().get<double>();

		out::Log("  " + key + " = " + std::to_string(value));
	}

#define GET_FEATURE(name) if(features.find(#name) != features.end()) { features::name = features[#name]; }

	GET_FEATURE(time_cost_dist_target);
	GET_FEATURE(time_cost_dist_dropoff);
	GET_FEATURE(time_cost_mining);

	GET_FEATURE(dropoff_ships_needed);
	GET_FEATURE(dropoff_map_distance);
	GET_FEATURE(dropoff_avg_threshold);

	GET_FEATURE(attack_mult);
	GET_FEATURE(enemy_halite_worth);
	GET_FEATURE(min_ally_ships_near);
	GET_FEATURE(ally_enemy_ratio);
	GET_FEATURE(ally_halite_less);
	GET_FEATURE(halite_ratio_less);

	out::Log("----------------------------");
}

void Game::Play()
{
	while (1) {
		{
			out::Stopwatch s("Update");
			Update();
		}
		{
			out::Stopwatch s("Turn");
			Turn();
		}
		out::Stopwatch::FlushMessages();
	}
}

void Game::Update()
{
	in::GetSStream() >> turn;
	remaining_turns = hlt::constants::MAX_TURNS - turn;
	out::Log("=============== TURN " + std::to_string(turn) + " ================");

	for (int i = 0; i < num_players; i++) {
		PlayerID player_id;
		int num_ships;
		int num_dropoffs;
		int halite;
		in::GetSStream() >> player_id >> num_ships >> num_dropoffs >> halite;
		players[player_id].Update(num_ships, num_dropoffs, halite, this);
	}

	map->Update();

	Player& me = players[my_id];

	out::Log("Halite: " + std::to_string(me.halite));
	out::Log("Ships: " + std::to_string(me.ships.size()));
	out::Log("AvgHalite: " + std::to_string(map->halite_remaining / (double)(map->width * map->height)));
	out::Log("-----");

}

void Game::Turn()
{
	std::vector<Command> commands;

	{
		out::Stopwatch s("Strategy Execute");
		strategy->Execute(commands);
	}

	// End Turn
	for (const Command& c : commands)
		std::cout << c << ' ';
	std::cout << std::endl;
}

Player& Game::GetPlayer(PlayerID id)
{
	return players[id];
}

Player& Game::GetMyPlayer()
{
	return GetPlayer(my_id);
}

bool Game::IsDropoff(const Position pos)
{
	for (auto& pp : players)
		if (pp.second.IsDropoff(pos))
			return true;
	return false;
}

Ship* Game::GetShipAt(const Position pos)
{
	for (auto& pp : players)
		for (auto& ss : pp.second.ships)
			if (ss.second->pos == pos)
				return ss.second;
	return nullptr;
}

bool Game::CanSpawnShip()
{
	return GetMyPlayer().halite >= hlt::constants::SHIP_COST;
}

bool Game::TransformIntoDropoff(Ship* s, std::vector<Command>& commands)
{
	auto& me = GetMyPlayer();

	double budget = s->halite + map->GetCell(s->pos)->halite + me.halite;
	if (budget >= hlt::constants::DROPOFF_COST) {
		me.halite = budget - hlt::constants::DROPOFF_COST;
		commands.push_back(TransformShipIntoDropoffCommand(s->ship_id));
		me.dropoffs.push_back(s->pos);
		map->GetCell(s->pos)->dropoff_owned = me.id;
		return true;
	}
	return false;
}
