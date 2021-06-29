#pragma once

#include <chrono>
#include <string>
#include <sstream>

class execution_timer
{
	typedef std::chrono::high_resolution_clock clock;
	typedef std::chrono::duration<double, std::ratio<1>> second;
	std::chrono::time_point<clock> beginning_;
	
public:
	execution_timer();

	void reset();

	[[nodiscard]] double elapsed_seconds() const;
	
	[[nodiscard]] std::string build_log_message(const std::string& action) const;

	[[nodiscard]] bool is_expired(int milliseconds) const;
};