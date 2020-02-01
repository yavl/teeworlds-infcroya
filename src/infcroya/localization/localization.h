#pragma once

#include <string>
#include "json_fwd.hpp"

class Localization {
public:
	static Localization& getInstance();
	void load(const std::string& filepath);
	std::string localize(const std::string& text, const std::string& language);
private:
	Localization() = default;
	Localization(const Localization&) = delete;
	void operator=(const Localization&) = delete;
	
	std::unique_ptr<nlohmann::json> json;
};

inline std::string localize(const std::string& text, const std::string& language) {
	return Localization::getInstance().localize(text, language);
}