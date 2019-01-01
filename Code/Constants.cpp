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
	double spawn_min_halite_per_ship;

	int dropoff_per_ships;
	double dropoff_map_distance;
	double dropoff_avg_threshold;

	double mine_halite_threshold;
	double mine_near_avg;
	double mine_dist_cost;
	double mine_dist_dropoff_cost;
	double mine_ally_ships_mult;
	double mine_enemy_ships_mult;
	double priority_epsilon;

	double friendliness_mine_cell;
	double friendliness_dodge;
	double friendliness_should_attack;
	double friendliness_can_attack;

	// For testing
	double a, b, c, d, e, f, g;
}