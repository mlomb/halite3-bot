#include "Player.hpp"

#include "Game.hpp"

Player::Player(PlayerID id, Position shipyard_position)
{
	this->id = id;
	this->shipyard_position = shipyard_position;
}

void Player::Update(int num_ships, int num_dropoffs, int halite, Game* game)
{
	this->halite = halite;

	for (auto& p : ships) p.second->dead = true;

	for (int i = 0; i < num_ships; i++) {
		EntityID ship_id;
		Position pos;
		int halite;
		in::GetSStream() >> ship_id >> pos.x >> pos.y >> halite;
		
		auto it = ships.find(ship_id);
		Ship* s;
		if (it != ships.end()) {
			s = (*it).second;
		}
		else {
			s = new Ship();
			ships[ship_id] = s;
		}
		s->ship_id = ship_id;
		s->pos = pos;
		s->halite = halite;
		s->dead = false;
	}

	auto itr = ships.begin();
	while (itr != ships.end()) {
		if ((*itr).second->dead) {
			delete (*itr).second;
			itr = ships.erase(itr);
		}
		else {
			++itr;
		}
	}

	for (int i = 0; i < num_dropoffs; i++) {
		int dropoff_id;
		int x;
		int y;
		in::GetSStream() >> dropoff_id >> x >> y;
	}
}
