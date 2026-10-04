#pragma once

#include "beatnext/core/Interfaces.hpp"
#include <string>

namespace beatnext {

class YouTubeMusicProvider final : public MusicProvider {
  public:
    explicit YouTubeMusicProvider(HttpClient& http, CacheStore* anonymousCache = nullptr,
                                  std::string language = "en", std::string location = "US");
    Outcome<std::vector<Track>> search(const std::string& query,
                                       const CancellationToken& cancellation) override;
    Outcome<std::vector<Track>> radio(const std::string& trackId,
                                      const CancellationToken& cancellation) override;

    // Public for fixture-based contract tests. Production calls use the methods above.
    Outcome<std::vector<Track>> parseTracks(const std::string& response, const std::string& source) const;

  private:
    struct RawResponse {
        std::string body;
        bool stale{false};
    };

    Outcome<RawResponse> rawRequest(const std::string& endpoint, const std::string& payload,
                                    const CancellationToken& cancellation);
    Outcome<std::vector<Track>> request(const std::string& endpoint, const std::string& payload,
                                        const std::string& source, const CancellationToken& cancellation);
    [[nodiscard]] std::string contextPayload(const std::string& clientVersion) const;

    HttpClient& http_;
    CacheStore* anonymousCache_;
    std::string language_;
    std::string location_;
};

} // namespace beatnext
