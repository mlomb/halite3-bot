#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

namespace in {
	std::string GetString();
	std::stringstream GetSStream();
}
namespace out {
	void Open(int bot_id);
	void Log(const std::string& message);
}