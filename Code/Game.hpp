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

namespace features {
	extern double time_cost_dist_target;
	extern double time_cost_dist_dropoff;
	extern double time_cost_mining;
	extern double mine_avg_profit;
	extern double mine_halite_ship_profit;
	extern double mine_ally_ships_profit;
	extern double mine_enemy_ships_profit;
	extern double dropoff_ships_needed;
	extern double dropoff_map_distance;
	extern double dropoff_avg_threshold;
}

class Game {
public:
	Game();

	static Game* Get();

	void Initialize(const std::string& bot_name);
	void LoadFeatures(json& features);
	void Play();
	void Update();
	void Turn();

	Player& GetPlayer(PlayerID id);
	Player& GetMyPlayer();
	bool IsDropoff(const Position pos); // any player
	Ship* GetShipAt(const Position pos); // any player

	bool CanSpawnShip();
	bool TransformIntoDropoff(Ship* s, std::vector<Command>& commands);

	int remaining_turns;
	int turn;
	int num_players;
	int total_halite;
	PlayerID my_id;
	std::unordered_map<PlayerID, Player> players;

	Map* map;
	Strategy* strategy;
private:
	static Game* s_Instance;
};