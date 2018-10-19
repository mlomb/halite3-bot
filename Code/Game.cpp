#include "Game.hpp"

#include "Command.hpp"
#include "Strategy.hpp"

Game* Game::s_Instance = nullptr;

Game::Game()
{
	s_Instance = this;
	strategy = new Strategy(this);
	map = new Map();
}

Game* Game::Get()
{
	return s_Instance;
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
	
	std::cout << bot_name << std::endl;

	// INFO
	out::Log("----------------------------");
	out::Log("Bot: " + bot_name);
	out::Log("Num players: " + num_players);
	out::Log("Map: " + std::to_string(map->width) + "x" + std::to_string(map->height));
	out::Log("----------------------------");
}

void Game::Play()
{
	while (1) {
		{
			out::Stopwatch("Update");
			Update();
		}
		{
			out::Stopwatch("Turn");
			Turn();
		}
		out::Stopwatch::FlushMessages();
	}
}

void Game::Update()
{
	in::GetSStream() >> turn;
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
}

void Game::Turn()
{
	std::vector<Command> commands;

	{
		out::Stopwatch("Strategy Execute");
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

bool Game::CanSpawnShip()
{
	return GetMyPlayer().halite >= hlt::constants::SHIP_COST;
}
