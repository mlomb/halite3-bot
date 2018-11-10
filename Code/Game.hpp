#pragma once

#include "Types.hpp"
#include "Player.hpp"
#include "Map.hpp"
#include "IO.hpp"

class Strategy;

class Game {
public:
	Game();
	
	inline static Game* Get() { return s_Instance; }

	void Initialize(const std::string& bot_name);
	void LoadConstants(json& constants);
	void LoadFeatures(json& features);
	void Play();
	void Update();
	void Turn();

	Player& GetPlayer(PlayerID id);
	Player& GetMyPlayer();
	bool IsDropoff(const Position pos); // any player
	Ship* GetShipAt(const Position pos); // any player

	bool CanSpawnShip(int reserved);
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