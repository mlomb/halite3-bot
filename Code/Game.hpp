#pragma once

#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

#include "Constants.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "IO.hpp"

class Strategy;

class Game {
public:
	Game();

	static Game* Get();

	void Initialize(const std::string& bot_name);
	void Play();
	void Update();
	void Turn();

	Player& GetPlayer(PlayerID id);
	Player& GetMyPlayer();

	bool CanSpawnShip();

	int remaining_turns;
	int turn;
	int num_players;
	PlayerID my_id;
	std::unordered_map<PlayerID, Player> players;

	Map* map;
	Strategy* strategy;

private:
	static Game* s_Instance;
};