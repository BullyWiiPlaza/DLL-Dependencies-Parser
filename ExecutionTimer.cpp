#include "ExecutionTimer.hpp"

execution_timer::execution_timer() : beginning_(clock::now())
{
}

double execution_timer::elapsed_seconds() const
{
	return std::chrono::duration_cast<second>(clock::now() - beginning_).count();
}

void execution_timer::reset()
{
	beginning_ = clock::now();
}

bool execution_timer::is_expired(const int milliseconds) const
{
	const auto elapsed_seconds_v = elapsed_seconds();
	return elapsed_seconds_v * 1000 > milliseconds;
}

inline auto round_up(const double value, const int decimal_places)
{
	const auto multiplier = std::pow(10.0, decimal_places);
	return std::ceil(value * multiplier) / multiplier;
}

std::string execution_timer::build_log_message(const std::string& action) const
{
	const auto elapsed = elapsed_seconds();
	std::stringstream message_builder;
	message_builder << action << " took " << round_up(elapsed, 2) << " s";
	return message_builder.str();
}