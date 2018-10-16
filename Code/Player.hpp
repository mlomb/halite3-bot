#pragma once

#include <unordered_map>

typedef int PlayerID;
typedef int EntityID;

class Game;

struct Ship {
	EntityID ship_id;
	int x, y;
	int halite;
};

class Player {
public:
	Player() {};
	Player(PlayerID id, int shipyard_x, int shipyard_y);

	void Update(int num_ships, int num_dropoffs, int halite, Game* game);

	PlayerID id;
	int halite;
	int shipyard_x, shipyard_y;
	std::unordered_map<EntityID, Ship> ships;
};