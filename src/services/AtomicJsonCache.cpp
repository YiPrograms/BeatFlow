#include "beatnext/services/AtomicJsonCache.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace beatnext {
namespace {

ServiceError storageError(std::string message) {
    return {ErrorCode::Storage, std::move(message), false, std::nullopt};
}

} // namespace

AtomicJsonCache::AtomicJsonCache(std::filesystem::path root, std::size_t maximumEntryBytes,
                                 std::size_t maximumEntries)
    : root_(std::move(root)), maximumEntryBytes_(maximumEntryBytes),
      maximumEntries_(std::max<std::size_t>(1, maximumEntries)) {}

Outcome<std::string> AtomicJsonCache::read(const std::string& key) {
    std::scoped_lock lock(mutex_);
    auto resolved = pathForKey(key);
    if (!resolved) {
        return Outcome<std::string>::failure(resolved.error());
    }

    std::error_code error;
    const auto size = std::filesystem::file_size(resolved.value(), error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return Outcome<std::string>::failure(
                {ErrorCode::NotFound, "Cache entry does not exist.", false, std::nullopt});
        }
        return Outcome<std::string>::failure(
            storageError("Could not inspect cache entry: " + error.message()));
    }
    if (size > maximumEntryBytes_) {
        std::filesystem::remove(resolved.value(), error);
        return Outcome<std::string>::failure(storageError("Cache entry exceeded the configured size limit."));
    }

    std::ifstream input(resolved.value(), std::ios::binary);
    if (!input) {
        return Outcome<std::string>::failure(storageError("Could not open cache entry."));
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return Outcome<std::string>::success(contents.str());
}

Outcome<bool> AtomicJsonCache::write(const std::string& key, const std::string& value) {
    std::scoped_lock lock(mutex_);
    if (value.size() > maximumEntryBytes_) {
        return Outcome<bool>::failure(storageError("Cache entry exceeded the configured size limit."));
    }
    auto resolved = pathForKey(key);
    if (!resolved) {
        return Outcome<bool>::failure(resolved.error());
    }

    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not create cache directory: " + error.message()));
    }

    auto temporary = resolved.value();
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return Outcome<bool>::failure(storageError("Could not create temporary cache entry."));
        }
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporary, error);
            return Outcome<bool>::failure(storageError("Could not finish writing cache entry."));
        }
    }

    std::filesystem::rename(temporary, resolved.value(), error);
    if (error) {
        // Android/Linux rename replaces atomically, but this fallback supports filesystems that do not.
        std::filesystem::remove(resolved.value(), error);
        error.clear();
        std::filesystem::rename(temporary, resolved.value(), error);
    }
    if (error) {
        std::filesystem::remove(temporary, error);
        return Outcome<bool>::failure(storageError("Could not publish cache entry: " + error.message()));
    }
    return prune(resolved.value());
}

Outcome<bool> AtomicJsonCache::remove(const std::string& key) {
    std::scoped_lock lock(mutex_);
    auto resolved = pathForKey(key);
    if (!resolved) {
        return Outcome<bool>::failure(resolved.error());
    }
    std::error_code error;
    std::filesystem::remove(resolved.value(), error);
    if (error) {
        return Outcome<bool>::failure(storageError("Could not remove cache entry: " + error.message()));
    }
    return Outcome<bool>::success(true);
}

Outcome<bool> AtomicJsonCache::clear() {
    std::scoped_lock lock(mutex_);
    std::error_code error;
    if (!std::filesystem::exists(root_, error)) {
        return Outcome<bool>::success(true);
    }
    for (const auto& entry : std::filesystem::directory_iterator(root_, error)) {
        if (error) {
            return Outcome<bool>::failure(storageError("Could not enumerate cache: " + error.message()));
        }
        if (entry.is_regular_file() &&
            (entry.path().extension() == ".json" || entry.path().extension() == ".tmp")) {
            std::filesystem::remove(entry.path(), error);
            if (error) {
                return Outcome<bool>::failure(storageError("Could not clear cache: " + error.message()));
            }
        }
    }
    return Outcome<bool>::success(true);
}

Outcome<std::filesystem::path> AtomicJsonCache::pathForKey(const std::string& key) const {
    if (key.empty() || key.size() > 120) {
        return Outcome<std::filesystem::path>::failure(storageError("Cache key has an invalid length."));
    }
    for (const unsigned char character : key) {
        if (!(std::isalnum(character) != 0 || character == '-' || character == '_')) {
            return Outcome<std::filesystem::path>::failure(
                storageError("Cache key contains unsafe characters."));
        }
    }
    return Outcome<std::filesystem::path>::success(root_ / (key + ".json"));
}

Outcome<bool> AtomicJsonCache::prune(const std::filesystem::path& preserved) {
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type modified;
    };
    std::vector<Entry> entries;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(root_, error)) {
        if (error) {
            return Outcome<bool>::failure(storageError("Could not inspect cache bounds: " + error.message()));
        }
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            entries.push_back({entry.path(), entry.last_write_time(error)});
            if (error) {
                return Outcome<bool>::failure(
                    storageError("Could not inspect cache age: " + error.message()));
            }
        } else if (entry.is_regular_file() && entry.path().extension() == ".tmp") {
            std::filesystem::remove(entry.path(), error);
            error.clear();
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        if (left.modified != right.modified) {
            return left.modified < right.modified;
        }
        return left.path < right.path;
    });
    auto entriesToRemove = entries.size() > maximumEntries_ ? entries.size() - maximumEntries_ : 0;
    for (const auto& entry : entries) {
        if (entriesToRemove == 0) {
            break;
        }
        if (entry.path == preserved) {
            continue;
        }
        std::filesystem::remove(entry.path, error);
        if (error) {
            return Outcome<bool>::failure(storageError("Could not prune cache entry: " + error.message()));
        }
        --entriesToRemove;
    }
    return Outcome<bool>::success(true);
}

} // namespace beatnext
