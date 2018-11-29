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
	double dropoff_map_distance;
	double dropoff_avg_threshold;

	double friendliness_drop_preservation;
	double friendliness_dodge;
	double friendliness_can_attack;
	double friendliness_should_attack;
	double friendliness_mine_cell;

	double mine_dist_cost;
	double mine_dist_dropoff_cost;
	double mine_avg_mult;
	double mine_ally_mult;
	double mine_enemy_mult;

	// For testing
	double a, b, c, d, e, f, g;
}