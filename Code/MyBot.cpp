#include <iostream>

#include "Game.hpp"
#include "Strategy.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char **argv) {
#ifdef HALITE_DEBUG
	//Sleep(20 * 1000);
#endif

#ifdef _WIN32
	srand(time(NULL) + GetCurrentProcessId());
#else
	srand(time(NULL));
#endif
	Game game;

	game.Initialize("mlomb-bot-v86");
	{
		// Load features

		std::string game_code = std::to_string(game.num_players) + "p";
		std::string game_code_specific = game_code + "_" + std::to_string(game.map->width);

		std::vector<std::string> files = {
			"best_default.json",
			"best_" + game_code + ".json",
			"best_" + game_code_specific + ".json",
		};

		// override with args
		std::string json_str = "";
		if (argc == 2) {
			json_str = std::string(argv[1]);
			if (json_str.substr(json_str.length() - 5) == ".json") {
				files.push_back(json_str);
				json_str = "";
			}
		}

		for (std::string filename : files) {
			try {
				out::Log("Reading JSON file: " + filename);
				std::ifstream t(filename);
				if (!t.is_open()) continue;
				std::stringstream buffer;
				buffer << t.rdbuf();
				std::string json_str = buffer.str();
				//out::Log("Input JSON: " + json_str);
				json json = json::parse(json_str)["features"];
				out::Log("JSON parsed!");
				game.LoadFeatures(json);
			}
			catch (...) {
				out::Log("Couldn't read JSON!");
			}
		}

		// override with args
		if (json_str.length() > 0) {
			out::Log("Input JSON: " + json_str);
			json json = json::parse(json_str);
			out::Log("JSON parsed!");
			game.LoadFeatures(json);
		}
	}
	game.Play();

	return 0;
}