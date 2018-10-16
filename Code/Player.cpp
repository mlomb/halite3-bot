#include "Player.hpp"

#include "Game.hpp"

Player::Player(PlayerID id, int shipyard_x, int shipyard_y)
{
	this->id = id;
	this->shipyard_x = shipyard_x;
	this->shipyard_y = shipyard_y;
}

void Player::Update(int num_ships, int num_dropoffs, int halite, Game* game)
{
	this->halite = halite;

	ships.clear();
	for (int i = 0; i < num_ships; i++) {
		EntityID ship_id;
		int x;
		int y;
		int halite;
		in::GetSStream() >> ship_id >> x >> y >> halite;
		ships[ship_id] = Ship { ship_id, x, y, halite };
	}

	for (int i = 0; i < num_dropoffs; i++) {
		int dropoff_id;
		int x;
		int y;
		in::GetSStream() >> dropoff_id >> x >> y;
	}
}
