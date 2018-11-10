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
	this->carrying_halite = 0;

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
		s->player_id = id;
		s->ship_id = ship_id;
		s->pos = pos;
		s->halite = halite;
		s->dead = false;

		game->map->GetCell(pos).ship_on_cell = s;

		this->carrying_halite += s->halite;
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

int Player::TotalHalite()
{
	return halite + carrying_halite;// +ships.size() * hlt::constants::SHIP_COST;
}

bool Player::IsDropoff(const Position pos)
{
	return Game::Get()->map->GetCell(pos).dropoff_owned == id;
}

Position Player::ClosestDropoff(const Position pos)
{
	Position closest;
	int distance = INF;

	for (const Position& p : dropoffs) {
		int dist = p.ToroidalDistanceTo(pos);
		if (dist < distance) {
			distance = dist;
			closest = p;
		}
	}

	return closest;
}

int Player::DistanceToClosestDropoff(const Position pos)
{
	return ClosestDropoff(pos).ToroidalDistanceTo(pos);
}

Ship* Player::ShipAt(const Position& pos)
{
	return Game::Get()->map->GetCell(pos).ship_on_cell;
}

Ship* Player::ClosestShipAt(const Position& pos)
{
	Ship* closest_ship = nullptr;
	int distance = INF;

	for (auto& sp : ships) {
		int dist = sp.second->pos.ToroidalDistanceTo(pos);
		if (dist < distance) {
			closest_ship = sp.second;
			distance = dist;
		}
	}

	return closest_ship;
}

void Player::SortByTaskPriority(std::vector<Ship*>& ships)
{
	std::sort(ships.begin(), ships.end(), [](const Ship* a, const Ship* b) {
		if (a->task.override == b->task.override) {
			if (a->task.type == b->task.type)
				return a->task.priority > b->task.priority;
			else
				return static_cast<int>(a->task.type) > static_cast<int>(b->task.priority);
		}
		else
			return a->task.override;
	});
}

Ship* Player::GetShipWithHighestPriority(std::vector<Ship*>& ships)
{
	SortByTaskPriority(ships);

	auto it = ships.begin();
	if (it == ships.end())
		return nullptr;
	return *it;
}
