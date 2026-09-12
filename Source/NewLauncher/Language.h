#pragma once
#include <string>
#include <unordered_map>

#define Dictionary std::unordered_map<std::string, std::string>

namespace lang
{
	extern Dictionary Languages;
	bool Load(const std::wstring& exeDir, const std::string& languageCode = "en-us") noexcept;
	std::string GetString(const std::string& key) noexcept;
}