#pragma once

#include "beatflow/core/Cancellation.hpp"
#include "beatflow/core/Error.hpp"
#include "beatflow/core/Models.hpp"

#include <functional>
#include <string>
#include <vector>

namespace beatflow {

class MusicProvider {
  public:
    virtual ~MusicProvider() = default;
    virtual Outcome<std::vector<Track>> home(const CancellationToken& cancellation) = 0;
    virtual Outcome<std::vector<Track>> search(const std::string& query,
                                               const CancellationToken& cancellation) = 0;
    virtual Outcome<std::vector<Track>> radio(const std::string& trackId,
                                              const CancellationToken& cancellation) = 0;
};

class MapCatalog {
  public:
    virtual ~MapCatalog() = default;
    virtual Outcome<std::vector<MapCandidate>> search(const Track& track,
                                                      const CancellationToken& cancellation) = 0;
};

class SongLibrary {
  public:
    virtual ~SongLibrary() = default;
    [[nodiscard]] virtual bool isInstalled(const std::string& hash) const = 0;
    virtual Outcome<std::string> install(const MapCandidate& map, const CancellationToken& cancellation) = 0;
    virtual Outcome<bool> openSongDetails(const std::string& hash) = 0;
};

class CacheStore {
  public:
    virtual ~CacheStore() = default;
    virtual Outcome<std::string> read(const std::string& key) = 0;
    virtual Outcome<bool> write(const std::string& key, const std::string& value) = 0;
    virtual Outcome<bool> remove(const std::string& key) = 0;
    virtual Outcome<bool> clear() = 0;
};

struct HttpRequest {
    enum class Method { Get, Post };
    Method method{Method::Get};
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeoutSeconds{30};
};

struct HttpResponse {
    int status{0};
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

class HttpClient {
  public:
    virtual ~HttpClient() = default;
    virtual Outcome<HttpResponse> send(const HttpRequest& request, const CancellationToken& cancellation) = 0;
};

} // namespace beatflow
