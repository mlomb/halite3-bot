#include <iostream>

#include "Game.hpp"

#ifdef HALITE_LOCAL
#include <Windows.h>
#endif


int main() {
#ifdef HALITE_LOCAL
	//Sleep(20 * 1000);
#endif

	Game game;

	game.Initialize("mlomb-bot-v38");
	game.Play();

	return 0;
}