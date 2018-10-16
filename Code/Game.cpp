#include "Game.hpp"

#include "Command.hpp"

Game* Game::s_Instance = nullptr;

Game::Game()
{
	s_Instance = this;
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
		int shipyard_x;
		int shipyard_y;
		in::GetSStream() >> player_id >> shipyard_x >> shipyard_y;
		players[player_id] = Player(player_id, shipyard_x, shipyard_y);
	}

	map.Initialize();

	std::cout << bot_name << std::endl;
}

void Game::Play()
{
	while (1) {
		Update();
		Turn();
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

	map.Update();
}

void Game::Turn()
{
	std::vector<Command> commands;

	if (CanSpawnShip()) {
		commands.push_back(SpawnCommand());
	}
	Player& me = GetMyPlayer();
	for (auto& p : me.ships) {
		Ship& s = p.second;


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
