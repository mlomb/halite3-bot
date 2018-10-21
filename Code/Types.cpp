#include "Types.hpp"

#include "Game.hpp"

Position Position::DirectionalOffset(const Direction d) const {
	auto dx = 0;
	auto dy = 0;
	switch (d) {
	case Direction::NORTH:
		dy = -1;
		break;
	case Direction::SOUTH:
		dy = 1;
		break;
	case Direction::EAST:
		dx = 1;
		break;
	case Direction::WEST:
		dx = -1;
		break;
	case Direction::STILL:
		// No move
		break;
	}
	return Position(x + dx, y + dy);
}

int Position::DistanceTo(const Position& other) const {
	return std::abs(x - other.x) + std::abs(y - other.y);
}
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
