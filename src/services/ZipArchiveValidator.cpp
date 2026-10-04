#include "beatnext/services/ZipArchiveValidator.hpp"

#include <algorithm>
#include <string_view>

namespace beatnext {
namespace {

constexpr std::uint32_t kCentralFileSignature = 0x02014b50U;
constexpr std::uint32_t kEndSignature = 0x06054b50U;

std::uint16_t read16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    const auto low = static_cast<std::uint16_t>(bytes[offset]);
    const auto high = static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
    return static_cast<std::uint16_t>(low | high);
}

std::uint32_t read32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) | static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

bool safePath(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos) {
        return false;
    }
    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }
    for (std::size_t start = 0; start <= path.size();) {
        const auto end = path.find('/', start);
        const auto part =
            path.substr(start, end == std::string_view::npos ? path.size() - start : end - start);
        if (part == "..") {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

ServiceError invalid(std::string message) {
    return {ErrorCode::InvalidResponse, std::move(message), false, std::nullopt};
}

} // namespace

Outcome<bool> ZipArchiveValidator::validate(std::span<const std::uint8_t> archive, ZipLimits limits) {
    if (archive.size() < 22 || archive.size() > limits.maximumArchiveBytes) {
        return Outcome<bool>::failure(invalid("The downloaded map archive has an invalid size."));
    }

    const auto searchStart = archive.size() > 65557 ? archive.size() - 65557 : 0;
    std::optional<std::size_t> endOffset;
    for (std::size_t offset = archive.size() - 22;; --offset) {
        if (read32(archive, offset) == kEndSignature) {
            endOffset = offset;
            break;
        }
        if (offset == searchStart) {
            break;
        }
    }
    if (!endOffset) {
        return Outcome<bool>::failure(invalid("The downloaded map is not a complete ZIP archive."));
    }

    const auto entryCount = read16(archive, *endOffset + 10);
    const auto centralSize = read32(archive, *endOffset + 12);
    const auto centralOffset = read32(archive, *endOffset + 16);
    if (entryCount == 0 || entryCount > limits.maximumEntries ||
        static_cast<std::uint64_t>(centralOffset) + centralSize > archive.size()) {
        return Outcome<bool>::failure(invalid("The downloaded map has an invalid ZIP directory."));
    }

    std::uint64_t totalUncompressed = 0;
    std::size_t cursor = centralOffset;
    bool hasInfo = false;
    for (std::size_t index = 0; index < entryCount; ++index) {
        if (cursor + 46 > archive.size() || read32(archive, cursor) != kCentralFileSignature) {
            return Outcome<bool>::failure(invalid("The downloaded map has a malformed ZIP entry."));
        }
        const auto uncompressedSize = read32(archive, cursor + 24);
        const auto nameLength = read16(archive, cursor + 28);
        const auto extraLength = read16(archive, cursor + 30);
        const auto commentLength = read16(archive, cursor + 32);
        const auto next = cursor + 46ULL + nameLength + extraLength + commentLength;
        if (nameLength == 0 || next > archive.size()) {
            return Outcome<bool>::failure(invalid("The downloaded map has an invalid ZIP entry name."));
        }
        const auto* nameData = reinterpret_cast<const char*>(archive.data() + cursor + 46);
        const std::string_view name(nameData, nameLength);
        if (!safePath(name)) {
            return Outcome<bool>::failure(invalid("The downloaded map contains an unsafe archive path."));
        }
        auto leaf = name;
        if (const auto slash = name.find_last_of('/'); slash != std::string_view::npos) {
            leaf = name.substr(slash + 1);
        }
        std::string lower(leaf);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        hasInfo = hasInfo || lower == "info.dat";

        totalUncompressed += uncompressedSize;
        if (totalUncompressed > limits.maximumUncompressedBytes) {
            return Outcome<bool>::failure(
                invalid("The downloaded map expands beyond the configured safety limit."));
        }
        cursor = static_cast<std::size_t>(next);
    }
    if (!hasInfo) {
        return Outcome<bool>::failure(invalid("The downloaded map does not contain Info.dat."));
    }
    return Outcome<bool>::success(true);
}

} // namespace beatnext
