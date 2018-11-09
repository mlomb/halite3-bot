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

	extern double dropoff_ships_needed;
	extern double dropoff_map_distance;
	extern double dropoff_avg_threshold;

	extern double attack_mult;
	extern double enemy_halite_worth;
	extern double min_ally_ships_near;
	extern double ally_enemy_ratio;
	extern double ally_halite_less;
	extern double halite_ratio_less;
}

class Game {
public:
	Game();
	
	inline static Game* Get() { return s_Instance; }

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