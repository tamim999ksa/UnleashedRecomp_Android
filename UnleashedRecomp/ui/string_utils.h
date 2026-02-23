#pragma once
#include <string>

std::string Truncate(const std::string& input, size_t maxLength, bool useEllipsis = true, bool usePrefixEllipsis = false);
