#pragma once

#include <string>
#include <map>

class config_file {
	std::map<std::string, std::string> values;

public:
	config_file(const std::string &path);

	float get_float(const std::string &key, float default_value);
	bool get_bool(const std::string &key, bool default_value);
};
