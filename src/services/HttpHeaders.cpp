#include "beatflow/services/HttpHeaders.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace beatflow::http {

std::vector<std::pair<std::string, std::string>> parseHeaders(std::string_view raw) {
    std::vector<std::pair<std::string, std::string>> result;
    std::istringstream lines{std::string(raw)};
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        auto name = line.substr(0, separator);
        auto value = line.substr(separator + 1);
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character) {
                        return std::isspace(character) == 0;
                    }));
        result.emplace_back(std::move(name), std::move(value));
    }
    return result;
}

int statusFromRawHeaders(std::string_view raw) {
    int status = 0;
    std::istringstream lines{std::string(raw)};
    for (std::string line; std::getline(lines, line);) {
        if (!line.starts_with("HTTP/")) {
            continue;
        }
        const auto separator = line.find(' ');
        if (separator == std::string::npos) {
            continue;
        }
        std::istringstream value(line.substr(separator + 1));
        int candidate = 0;
        if (value >> candidate && candidate >= 100 && candidate <= 599) {
            status = candidate;
        }
    }
    return status;
}

} // namespace beatflow::http
