#pragma once

#include <dlib/svm.h>
using namespace dlib;

#include "Player.hpp"
#include "Map.hpp"

class Strategy;

struct PlayerCombat {
	//PlayerCombat() 
	//	: krls(kernel_type(0.000001), 0.0001) {};
	//typedef matrix<double, 0, 1> sample_type;
	//typedef radial_basis_kernel<sample_type> kernel_type;
	//krls<kernel_type> krls;
	//
	//std::vector<std::vector<double>> data;

	// yes
	std::vector<double> engage_friendliness_points;
	std::vector<double> no_dodge_friendliness_points;
	double friendliness_no_dodge;
	double friendliness_engage;
};

enum class EventType {
	DODGE,
	ENGAGE
};

struct Event {
	PlayerID player_id;
	EntityID ship_id;
	Position pos;

	EventType type;
	double friendliness;

	std::vector<double> data;
};

class Combat {
public:
	Combat(Strategy* strategy);

	bool IsSafe(Position p, bool may_attack);
	int FriendlinessNew(Player& player, Position position, Ship* skip);

	double Friendliness(Player& player, Position position, bool ignore_position = false);
	int EnemyReachHalite(Player& player, Position position);

	bool WillReceiveImminentAttack(Player& player, Position position);
	bool FreeToMove(Player& player, Position position);

	void Update();
	void Report();

	Strategy* strategy;
	Game* game;

	std::map<PlayerID, PlayerCombat> players;
	std::vector<Event> next_turn_events;
};