#include "Constants.hpp"

namespace constants {
	int MAX_HALITE;
	int SHIP_COST;
	int DROPOFF_COST;
	int MAX_TURNS;
	int EXTRACT_RATIO;
	int MOVE_COST_RATIO;
	bool INSPIRATION_ENABLED;
	int INSPIRATION_RADIUS;
	int INSPIRATION_SHIP_COUNT;
	int INSPIRED_EXTRACT_RATIO;
	double INSPIRED_BONUS_MULTIPLIER;
	int INSPIRED_MOVE_COST_RATIO;
	int GAME_SEED;
	int MAP_WIDTH;
	int MAP_HEIGHT;
	unsigned int RANDOM_SEED;
}

namespace features {
	Parameter dropoff_avg_threshold(0, 3.5);

	Parameter friendliness_drop_preservation(-1, 1);
	Parameter friendliness_dodge(-0.5, 1.0);
	Parameter friendliness_can_attack(-1, 4);
	Parameter friendliness_should_attack(1, 5);
	Parameter friendliness_mine_cell(-1.5, 0.1);

	Parameter mine_dist_cost(0, 10);
	Parameter mine_dist_dropoff_cost(0, 10);
	Parameter mine_avg_mult(0, 50);
	Parameter mine_ally_mult(-150, 150);
	Parameter mine_enemy_mult(-150, 150);

	// For testing
	double a, b, c, d, e, f, g;
}