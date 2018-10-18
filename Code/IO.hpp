#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

#include "json.hpp"
using json = nlohmann::json;

namespace in {
	std::string GetString();
	std::stringstream GetSStream();
}
namespace out {
	void Open(int bot_id);
	void Log(const std::string& message);
	void LogShip(int ship_id, const json& j);
}