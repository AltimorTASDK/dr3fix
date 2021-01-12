#include "config_file.h"
#include <fstream>
#include <regex>

const std::regex comment_regex("\\s*//.*");
const std::regex config_regex("\\s*(\\S+)\\s*=\\s*(\\S+)\\s*");

config_file::config_file(const std::string &path)
{
	std::ifstream stream(path);

	std::string line;
	while (std::getline(stream, line)) {
		if (std::regex_match(line, comment_regex))
			continue;

		std::smatch match;
		if (!std::regex_match(line, match, config_regex) || match.size() != 3)
			continue;

		values[match[1]] = match[2];
	}
}

float config_file::get_float(const std::string &key, float default_value)
{
	const auto it = values.find(key);
	if (it == values.end())
		return default_value;

	char *end;
	float value = strtof(it->second.c_str(), &end);
	if (*end == '\0')
		return value;
	else
		return default_value;
}

bool config_file::get_bool(const std::string &key, bool default_value)
{
	const auto it = values.find(key);
	if (it == values.end())
		return default_value;

	const std::string &value = it->second;
	if (value == "true" || value == "True" || value == "1")
		return true;
	else if (value == "false" || value == "False" || value == "0")
		return false;
	else
		return default_value;
}