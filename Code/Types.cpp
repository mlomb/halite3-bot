#include "Types.hpp"

#include "Game.hpp"

int Position::ToroidalDistanceTo(const Position& other) const {
	Game* g = Game::Get();

	int dx = std::abs(x - other.x);
	int dy = std::abs(y - other.y);

	int toroidal_dx = std::min(dx, g->map->width - dx);
	int toroidal_dy = std::min(dy, g->map->height - dy);

	return toroidal_dx + toroidal_dy;
}
void Position::Wrap() {
	Game* g = Game::Get();

	const int wx = g->map->width;
	const int wy = g->map->height;

	x = (x + wx) % wx;
	y = (y + wy) % wy;
}

std::mt19937_64& mt()
{
	thread_local static std::random_device srd;
	thread_local static std::mt19937_64 smt(srd());
	return smt;
}
