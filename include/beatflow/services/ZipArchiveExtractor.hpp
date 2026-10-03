#pragma once

#include "beatflow/core/Error.hpp"

#include <cstdint>
#include <filesystem>
#include <span>

namespace beatflow {

class ZipArchiveExtractor {
  public:
    [[nodiscard]] static Outcome<bool> extract(std::span<const std::uint8_t> archive,
                                               const std::filesystem::path& destination);
};

} // namespace beatflow
