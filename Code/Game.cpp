#include "Game.hpp"

#include "Command.hpp"
#include "Strategy.hpp"

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

	LoadConstants(json::parse(in::GetString()));
	in::GetSStream() >> num_players >> my_id;

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
	out::Log("Seed: " + std::to_string(constants::GAME_SEED));
	out::Log("----------------------------");
}

void Game::LoadConstants(json& constants)
{
	out::Log("Constants from the engine:");

#define GET_CONSTANT(name, var_name) if(constants.find(#name) != constants.end()) { constants::var_name = constants[#name]; out::Log(std::string("  ") + #var_name + ": " + std::to_string(constants::var_name)); }

	GET_CONSTANT(NEW_ENTITY_ENERGY_COST, SHIP_COST);
	GET_CONSTANT(DROPOFF_COST, DROPOFF_COST);
	GET_CONSTANT(MAX_ENERGY, MAX_HALITE);
	GET_CONSTANT(MAX_TURNS, MAX_TURNS);
	GET_CONSTANT(EXTRACT_RATIO, EXTRACT_RATIO);
	GET_CONSTANT(MOVE_COST_RATIO, MOVE_COST_RATIO);
	GET_CONSTANT(INSPIRATION_ENABLED, INSPIRATION_ENABLED);
	GET_CONSTANT(INSPIRATION_RADIUS, INSPIRATION_RADIUS);
	GET_CONSTANT(INSPIRATION_SHIP_COUNT, INSPIRATION_SHIP_COUNT);
	GET_CONSTANT(INSPIRED_EXTRACT_RATIO, INSPIRED_EXTRACT_RATIO);
	GET_CONSTANT(INSPIRED_BONUS_MULTIPLIER, INSPIRED_BONUS_MULTIPLIER);
	GET_CONSTANT(INSPIRED_MOVE_COST_RATIO, INSPIRED_MOVE_COST_RATIO);
	GET_CONSTANT(game_seed, GAME_SEED);
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
	remaining_turns = constants::MAX_TURNS - turn;
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
	out::Log("----------------------------");
}

void Game::Turn()
{
	std::vector<Command> commands;

	{
		out::Stopwatch s("Strategy Execute");
		try {
			strategy->Execute(commands);
		}
		catch (std::exception& e)
		{
			out::Log("Exception catched during Strategy Execution: " + std::string(e.what()));
		}
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
	return map->GetCell(pos).dropoff_owned != -1;
}

Ship* Game::GetShipAt(const Position pos)
{
	return map->GetCell(pos).ship_on_cell;
}

bool Game::CanSpawnShip(int reserved)
{
	return GetMyPlayer().halite >= reserved + constants::SHIP_COST;
}

bool Game::TransformIntoDropoff(Ship* s, std::vector<Command>& commands)
{
	auto& me = GetMyPlayer();

	double budget = s->halite + map->GetCell(s->pos).halite + me.halite;
	if (budget >= constants::DROPOFF_COST) {
		me.halite = budget - constants::DROPOFF_COST;
		commands.push_back(TransformShipIntoDropoffCommand(s->ship_id));
		me.dropoffs.push_back(s->pos);
		map->GetCell(s->pos).dropoff_owned = me.id;
		map->GetCell(s->pos).halite = 0;
		return true;
	}
	return false;
}
