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
}

namespace features {
	double time_cost_dist_target;
	double time_cost_dist_dropoff;
	double time_cost_mining;

	double dropoff_ships_needed;
	double dropoff_map_distance;
	double dropoff_avg_threshold;

	double attack_mult;
	double enemy_halite_worth;
	double min_ally_ships_near;
	double ally_enemy_ratio;
	double ally_halite_less;
	double halite_ratio_less;
}