#include "beatnext/services/ZipArchiveExtractor.hpp"

#include "beatnext/services/ZipArchiveValidator.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <string_view>
#include <zlib.h>

namespace beatnext {
namespace {

constexpr std::uint32_t kLocalFileSignature = 0x04034b50U;
constexpr std::uint32_t kCentralFileSignature = 0x02014b50U;
constexpr std::uint32_t kEndSignature = 0x06054b50U;

std::uint16_t read16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t read32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) | static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

ServiceError invalid(std::string message) {
    return {ErrorCode::InvalidResponse, std::move(message), false, std::nullopt};
}

ServiceError storage(std::string message) {
    return {ErrorCode::Storage, std::move(message), false, std::nullopt};
}

Outcome<bool> extractFile(std::span<const std::uint8_t> compressed, std::uint16_t method,
                          std::uint32_t expectedSize, std::uint32_t expectedCrc,
                          const std::filesystem::path& destination) {
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) {
        return Outcome<bool>::failure(storage("Could not create a staged map file."));
    }

    uLong crc = crc32(0L, Z_NULL, 0);
    std::uint64_t written = 0;
    if (method == 0) {
        if (compressed.size() != expectedSize) {
            return Outcome<bool>::failure(invalid("A stored ZIP entry has an invalid size."));
        }
        output.write(reinterpret_cast<const char*>(compressed.data()),
                     static_cast<std::streamsize>(compressed.size()));
        crc = crc32(crc, compressed.data(), static_cast<uInt>(compressed.size()));
        written = compressed.size();
    } else if (method == 8) {
        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(compressed.data());
        stream.avail_in = static_cast<uInt>(compressed.size());
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            return Outcome<bool>::failure(invalid("Could not initialize ZIP decompression."));
        }

        std::array<std::uint8_t, 64U * 1024U> buffer{};
        int result = Z_OK;
        while (result == Z_OK) {
            stream.next_out = buffer.data();
            stream.avail_out = static_cast<uInt>(buffer.size());
            result = inflate(&stream, Z_NO_FLUSH);
            const auto produced = buffer.size() - stream.avail_out;
            if (produced > 0) {
                output.write(reinterpret_cast<const char*>(buffer.data()),
                             static_cast<std::streamsize>(produced));
                crc = crc32(crc, buffer.data(), static_cast<uInt>(produced));
                written += produced;
            }
            if (written > expectedSize) {
                inflateEnd(&stream);
                return Outcome<bool>::failure(invalid("A ZIP entry expanded beyond its declared size."));
            }
        }
        inflateEnd(&stream);
        if (result != Z_STREAM_END) {
            return Outcome<bool>::failure(invalid("The map archive contains damaged compressed data."));
        }
    } else {
        return Outcome<bool>::failure(invalid("The map archive uses an unsupported compression method."));
    }

    if (!output || written != expectedSize || static_cast<std::uint32_t>(crc) != expectedCrc) {
        return Outcome<bool>::failure(invalid("A map archive entry failed its integrity check."));
    }
    return Outcome<bool>::success(true);
}

} // namespace

Outcome<bool> ZipArchiveExtractor::extract(std::span<const std::uint8_t> archive,
                                           const std::filesystem::path& destination) {
    auto valid = ZipArchiveValidator::validate(archive);
    if (!valid) {
        return valid;
    }

    const auto searchStart = archive.size() > 65557 ? archive.size() - 65557 : 0;
    std::size_t endOffset = archive.size() - 22;
    while (endOffset > searchStart && read32(archive, endOffset) != kEndSignature) {
        --endOffset;
    }
    if (read32(archive, endOffset) != kEndSignature) {
        return Outcome<bool>::failure(invalid("The downloaded map is not a complete ZIP archive."));
    }

    const auto entryCount = read16(archive, endOffset + 10);
    std::size_t cursor = read32(archive, endOffset + 16);
    std::error_code error;
    std::filesystem::create_directories(destination, error);
    if (error) {
        return Outcome<bool>::failure(
            storage("Could not create the map extraction directory: " + error.message()));
    }

    for (std::size_t index = 0; index < entryCount; ++index) {
        if (cursor + 46 > archive.size() || read32(archive, cursor) != kCentralFileSignature) {
            return Outcome<bool>::failure(invalid("The map archive directory changed during extraction."));
        }
        const auto flags = read16(archive, cursor + 8);
        const auto method = read16(archive, cursor + 10);
        const auto expectedCrc = read32(archive, cursor + 16);
        const auto compressedSize = read32(archive, cursor + 20);
        const auto uncompressedSize = read32(archive, cursor + 24);
        const auto nameLength = read16(archive, cursor + 28);
        const auto extraLength = read16(archive, cursor + 30);
        const auto commentLength = read16(archive, cursor + 32);
        const auto localOffset = read32(archive, cursor + 42);
        const auto name =
            std::string_view(reinterpret_cast<const char*>(archive.data() + cursor + 46), nameLength);
        cursor += 46ULL + nameLength + extraLength + commentLength;

        if ((flags & 0x1U) != 0) {
            return Outcome<bool>::failure(invalid("Encrypted map archives are not supported."));
        }
        const auto outputPath = destination / std::filesystem::path(name);
        if (name.ends_with('/')) {
            std::filesystem::create_directories(outputPath, error);
            if (error) {
                return Outcome<bool>::failure(storage("Could not create a staged map directory."));
            }
            continue;
        }
        if (static_cast<std::uint64_t>(localOffset) + 30 > archive.size() ||
            read32(archive, localOffset) != kLocalFileSignature) {
            return Outcome<bool>::failure(invalid("The map archive contains an invalid file header."));
        }
        const auto localNameLength = read16(archive, localOffset + 26);
        const auto localExtraLength = read16(archive, localOffset + 28);
        const std::uint64_t dataOffset =
            static_cast<std::uint64_t>(localOffset) + 30 + localNameLength + localExtraLength;
        if (dataOffset + compressedSize > archive.size() ||
            compressedSize > std::numeric_limits<uInt>::max()) {
            return Outcome<bool>::failure(invalid("The map archive contains a truncated file."));
        }

        std::filesystem::create_directories(outputPath.parent_path(), error);
        if (error) {
            return Outcome<bool>::failure(storage("Could not create a staged map directory."));
        }
        auto extracted = extractFile(archive.subspan(static_cast<std::size_t>(dataOffset), compressedSize),
                                     method, uncompressedSize, expectedCrc, outputPath);
        if (!extracted) {
            return extracted;
        }
    }
    return Outcome<bool>::success(true);
}

} // namespace beatnext
