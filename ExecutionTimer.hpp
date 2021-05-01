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
	execution_timer() : beginning_(clock::now())
	{
	}

	void reset() { beginning_ = clock::now(); }

	[[nodiscard]] double elapsed_seconds() const
	{
		return std::chrono::duration_cast<second>(clock::now() - beginning_).count();
	}

	[[nodiscard]] double round_up(const double value, const int decimal_places) const
	{
		const auto multiplier = std::pow(10.0, decimal_places);
		return std::ceil(value * multiplier) / multiplier;
	}

	[[nodiscard]] std::string build_log_message(const std::string& action) const
	{
		const auto elapsed = elapsed_seconds();
		std::stringstream message_builder;
		message_builder << action << " took " << round_up(elapsed, 2) << " s";
		return message_builder.str();
	}

	[[nodiscard]] auto is_expired(const int milliseconds) const
	{
		const auto elapsed_seconds_v = elapsed_seconds();
		return elapsed_seconds_v * 1000 > milliseconds;
	}
};