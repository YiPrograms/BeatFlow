#pragma once

#include "beatnext/core/Cancellation.hpp"
#include "beatnext/core/Error.hpp"
#include "beatnext/core/Models.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace beatnext {

class MusicProvider {
  public:
    virtual ~MusicProvider() = default;
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

class MapInstaller {
  public:
    virtual ~MapInstaller() = default;
    [[nodiscard]] virtual bool isInstalled(const std::string& hash) const = 0;
    virtual Outcome<std::string> install(const MapCandidate& map, const CancellationToken& cancellation) = 0;
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
    std::size_t maximumResponseBytes{16U * 1024U * 1024U};
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

} // namespace beatnext
