#pragma once

#include <unordered_map>
#include <string>
#include <algorithm>

namespace constants {
	/** The maximum amount of halite a ship can carry. */
	extern int MAX_HALITE;
	/** The cost to build a single ship. */
	extern int SHIP_COST;
	/** The cost to build a dropoff. */
	extern int DROPOFF_COST;
	/** The maximum number of turns a game can last. */
	extern int MAX_TURNS;
	/** 1/EXTRACT_RATIO halite (rounded) is collected from a square per turn. */
	extern int EXTRACT_RATIO;
	/** 1/MOVE_COST_RATIO halite (rounded) is needed to move off a cell. */
	extern int MOVE_COST_RATIO;
	/** Whether inspiration is enabled. */
	extern bool INSPIRATION_ENABLED;
	/** A ship is inspired if at least INSPIRATION_SHIP_COUNT opponent ships are within this Manhattan distance. */
	extern int INSPIRATION_RADIUS;
	/** A ship is inspired if at least this many opponent ships are within INSPIRATION_RADIUS distance. */
	extern int INSPIRATION_SHIP_COUNT;
	/** An inspired ship mines 1/X halite from a cell per turn instead. */
	extern int INSPIRED_EXTRACT_RATIO;
	/** An inspired ship that removes Y halite from a cell collects X*Y additional halite. */
	extern double INSPIRED_BONUS_MULTIPLIER;
	/** An inspired ship instead spends 1/X% halite to move. */
	extern int INSPIRED_MOVE_COST_RATIO;
	/** The game seed */
	extern int GAME_SEED;
	/** Map width */
	extern int MAP_WIDTH;
	/** Map height */
	extern int MAP_HEIGHT;
	/** Random number for this instance */
	extern unsigned int RANDOM_SEED;
}

namespace features {
	// Spawning
	extern double spawn_min_halite_per_ship;

	// Dropoffs
	extern int dropoff_per_ships;
	extern double dropoff_to_map_avg;
	extern int dropoff_min_avg;

	// Mining
	extern double mine_halite_threshold;
	extern double mine_near_avg;
	extern double mine_dist_cost;
	extern double mine_dist_dropoff_cost;
	extern double mine_ally_ships_mult;
	extern double mine_enemy_ships_mult;
	extern double priority_epsilon;

	// Combat
	extern double friendliness_mine_cell;
	extern double friendliness_dodge;
	extern double friendliness_should_attack;
	extern double friendliness_can_attack;

	// Tests
	extern double a, b, c, d, e, f, g;
}