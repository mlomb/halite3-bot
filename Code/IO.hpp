#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

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

	struct StopwatchSummaryEntry {
		long long ms_min = INF;
		long long ms_max = -INF;
		long long ms_sum = 0;
	};

	class Stopwatch {
	public:
		Stopwatch(const std::string& identifier);
		~Stopwatch();

		static void FlushMessages();
		static void ShowSummary();

		std::string identifier;
		std::chrono::time_point<std::chrono::system_clock> start;

		static std::vector<std::string> messages;
		static std::map<std::string, StopwatchSummaryEntry> summary;
	};

}