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

	dropoffs.clear();
	dropoffs.push_back(shipyard_position);
	for (int i = 0; i < num_dropoffs; i++) {
		int dropoff_id;
		Position dropoff_position;
		in::GetSStream() >> dropoff_id >> dropoff_position.x >> dropoff_position.y;
		dropoffs.push_back(dropoff_position);
	}
}

bool Player::IsDropoff(const Position pos)
{
	for (const Position& p : dropoffs)
		if (p == pos)
			return true;
	return false;
}

Position Player::ClosestDropoff(const Position pos)
{
	Position closest;
	int distance = INF;

	for (const Position p : dropoffs) {
		int dist = p.ToroidalDistanceTo(pos);
		if (dist < distance) {
			distance = dist;
			closest = p;
		}
	}

	return closest;
}

Ship* Player::ShipAt(const Position pos)
{
	for (auto& p : ships) {
		if (p.second->pos == pos)
			return p.second;
	}
	return nullptr;
}

void Player::SortByTaskPriority(std::vector<Ship*>& ships)
{
	std::sort(ships.begin(), ships.end(), [](const Ship* a, const Ship* b) {
		return a->task_priority > b->task_priority;
	});
}
