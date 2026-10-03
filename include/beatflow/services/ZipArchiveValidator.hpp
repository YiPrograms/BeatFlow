#pragma once

#include "beatflow/core/Error.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace beatflow {

struct ZipLimits {
    std::size_t maximumArchiveBytes{256U * 1024U * 1024U};
    std::size_t maximumEntries{4096};
    std::uint64_t maximumUncompressedBytes{1024ULL * 1024ULL * 1024ULL};
};

class ZipArchiveValidator {
  public:
    [[nodiscard]] static Outcome<bool> validate(std::span<const std::uint8_t> archive, ZipLimits limits = {});
};

} // namespace beatflow
