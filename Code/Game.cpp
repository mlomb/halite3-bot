#include "Game.hpp"

#include "Command.hpp"
#include "Strategy.hpp"

Game* Game::s_Instance = nullptr;

Game::Game()
{
	s_Instance = this;
	map = new Map(this);
	strategy = new Strategy(this);
}

void Game::Initialize(const std::string& bot_name)
{
	std::ios_base::sync_with_stdio(false);

	json constants_json = json::parse(in::GetString());
	LoadConstants(constants_json);
	constants::RANDOM_SEED = rand();
	mt().seed(constants::GAME_SEED/* + constants::RANDOM_SEED*/);
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
	strategy->Initialize(); 
	
	std::cout << bot_name << std::endl;

	// INFO
	out::Log("----------------------------");
	out::Log("Bot: " + bot_name);
	out::Log("Num players: " + std::to_string(num_players));
	out::Log("Map: " + std::to_string(map->width) + "x" + std::to_string(map->height));
	out::Log("Total Halite: " + std::to_string(total_halite));
	out::Log("Seed: " + std::to_string(constants::GAME_SEED));
	out::Log("Random seed: " + std::to_string(constants::RANDOM_SEED));
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
		if (it.value().is_number()) {
			double value = it.value().get<double>();
			out::Log("  " + key + " = " + std::to_string(value));
		}
		else if (it.value().is_boolean()) {
			bool value = it.value().get<bool>();
			out::Log("  " + key + " = " + std::to_string(value));
		}
	}

#define GET_FEATURE(name) if(features.find(#name) != features.end()) { features::name = features[#name]; }

	GET_FEATURE(dropoff_map_distance);
	GET_FEATURE(dropoff_avg_threshold);

	GET_FEATURE(friendliness_drop_preservation);
	GET_FEATURE(friendliness_dodge);
	GET_FEATURE(friendliness_can_attack);
	GET_FEATURE(friendliness_should_attack);
	GET_FEATURE(friendliness_mine_cell);

	GET_FEATURE(mine_dist_cost);
	GET_FEATURE(mine_dist_dropoff_cost);
	GET_FEATURE(mine_avg_mult);
	GET_FEATURE(mine_ally_mult);
	GET_FEATURE(mine_enemy_mult);

	GET_FEATURE(a);
	GET_FEATURE(b);
	GET_FEATURE(c);
	GET_FEATURE(d);
	GET_FEATURE(e);
	GET_FEATURE(f);
	GET_FEATURE(g);
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
	{
		out::Stopwatch s("Engine response wait");
		in::GetSStream() >> turn;
	}

	remaining_turns = constants::MAX_TURNS - turn;
	out::Log("=============== TURN " + std::to_string(turn) + " ================");
	turn_started = std::chrono::system_clock::now();

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

long long Game::MsTillTimeout()
{
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end - turn_started;
	long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	return std::max((2000 - 5 /* safe margin */) - ms, (long long)0);
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
