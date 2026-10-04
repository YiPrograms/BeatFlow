#pragma once

#include "beatnext/core/Error.hpp"

#include <cstdint>
#include <filesystem>
#include <span>

namespace beatnext {

class ZipArchiveExtractor {
  public:
    [[nodiscard]] static Outcome<bool> extract(std::span<const std::uint8_t> archive,
                                               const std::filesystem::path& destination);
};

} // namespace beatnext
