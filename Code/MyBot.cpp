#include <iostream>

#include "Game.hpp"

#ifdef DEBUG
#include <Windows.h>
#endif

int main() {
#ifdef DEBUG
	//Sleep(20 * 1000);
#endif

	Game game;

	game.Initialize("mlomb-bot");
	game.Play();

	return 0;
}