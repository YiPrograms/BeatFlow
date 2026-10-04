#pragma once

#include "beatnext/core/Interfaces.hpp"

#include <cstddef>
#include <filesystem>
#include <mutex>

namespace beatnext {

class AtomicJsonCache final : public CacheStore {
  public:
    explicit AtomicJsonCache(std::filesystem::path root, std::size_t maximumEntryBytes = 4 * 1024 * 1024,
                             std::size_t maximumEntries = 96);

    Outcome<std::string> read(const std::string& key) override;
    Outcome<bool> write(const std::string& key, const std::string& value) override;
    Outcome<bool> remove(const std::string& key) override;
    Outcome<bool> clear() override;

  private:
    Outcome<std::filesystem::path> pathForKey(const std::string& key) const;
    Outcome<bool> prune(const std::filesystem::path& preserved);

    std::filesystem::path root_;
    std::size_t maximumEntryBytes_;
    std::size_t maximumEntries_;
    std::mutex mutex_;
};

} // namespace beatnext
