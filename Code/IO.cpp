#include "IO.hpp"

namespace in {
	std::string GetString()
	{
		std::string result;
		std::getline(std::cin, result);
		if (!std::cin.good()) {
			out::Log("Input connection from server closed. Exiting...");
			exit(0);
		}
		return result;
	}

	std::stringstream GetSStream()
	{
		return std::stringstream(GetString());
	}
}

namespace out {
	static std::ofstream log_file;
	static std::vector<std::string> log_buffer;
	static bool has_opened = false;
	static bool has_atexit = false;

	void dump_buffer_at_exit() {
		if (has_opened) {
			return;
		}

		auto now_in_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
		std::string filename = "bot-unknown-" + std::to_string(now_in_nanos) + ".log";
		std::ofstream file(filename, std::ios::trunc | std::ios::out);
		for (const std::string& message : log_buffer)
			file << message << std::endl;
	}

	void Open(int bot_id)
	{
		if (has_opened) {
			out::Log("Error: log: tried to open(" + std::to_string(bot_id) + ") but we have already opened before.");
			exit(1);
		}

		has_opened = true;
		std::string filename = "bot-" + std::to_string(bot_id) + ".log";
		log_file.open(filename, std::ios::trunc | std::ios::out);

		for (const std::string& message : log_buffer)
			log_file << message << std::endl;
		log_buffer.clear();
	}

	void Log(const std::string& message)
	{
		if (has_opened) {
			log_file << message << std::endl;
		} else {
			if (!has_atexit) {
				has_atexit = true;
				atexit(dump_buffer_at_exit);
			}
			log_buffer.push_back(message);
		}
	}
}