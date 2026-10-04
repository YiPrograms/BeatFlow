#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace beatnext::http {

std::vector<std::pair<std::string, std::string>> parseHeaders(std::string_view raw);

// Returns the last valid HTTP status line. A response can contain more than
// one header block after a proxy handshake or redirect.
int statusFromRawHeaders(std::string_view raw);

} // namespace beatnext::http
