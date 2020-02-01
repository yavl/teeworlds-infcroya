#include "localization.h"
#include <fstream>
#include "json.hpp"

Localization& Localization::getInstance()
{
	static Localization instance;
	return instance;
}

void Localization::load(const std::string& filepath)
{
	std::ifstream fin(filepath);
	json = std::make_unique<nlohmann::json>();
	auto& j = *json;
	try {
		fin >> j;
	}
	catch (std::exception & e) {
		printf("%s\n", e.what());
	}
	printf("------ File loaded: %s\n", filepath.c_str()); // todo replace with default teeworlds logger
}

std::string Localization::localize(const std::string& text, const std::string& language)
{
	// todo cleanup, add error handling, change translations format (separate translation files)
	if (language == "english")
		return text;

	auto& j = *json;
	auto& arr = j["translated strings"];
	auto res = std::find_if(arr.begin(), arr.end(), [text, language](const nlohmann::json& x) {
		auto contextIt = x.find("context");
		auto orIt = x.find("or");
		auto trIt = x.find("tr");
		if (contextIt != x.end() && orIt != x.end() && trIt != x.end()) {
			return contextIt.value() == language && orIt.value() == text;
		}
		return false;
	});
	if (res != arr.end()) {
		return (*res)["tr"].get<std::string>();
	}
	return text;
}
