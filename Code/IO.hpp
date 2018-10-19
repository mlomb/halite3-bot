#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

#include "json.hpp"
using json = nlohmann::json;

#include "Types.hpp"

namespace in {
	std::string GetString();
	std::stringstream GetSStream();
}
namespace out {
	void Open(int bot_id);
	void Log(const std::string& message);
	void LogFluorineDebug(const json& meta, const json& data);
	void LogShip(EntityID ship_id, const json& data);

	class Stopwatch {
	public:
		Stopwatch(const std::string& identifier);
		~Stopwatch();

		static void FlushMessages();

		std::string identifier;
		std::chrono::time_point<std::chrono::high_resolution_clock> start;

		static std::vector<std::string> messages;
	};

}