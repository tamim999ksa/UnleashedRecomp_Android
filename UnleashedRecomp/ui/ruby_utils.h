#pragma once

#include <string>
#include <map>
#include <string_view>
#include <utility>

std::pair<std::string, std::map<std::string, std::string>> RemoveRubyAnnotations(const char* input);
std::string ReAddRubyAnnotations(const std::string_view& wrappedText, const std::map<std::string, std::string>& rubyMap);
