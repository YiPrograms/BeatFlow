#pragma once

#include "beatnext/core/Interfaces.hpp"

namespace beatnext::quest {

class QuestHttpClient final : public HttpClient {
  public:
    Outcome<HttpResponse> send(const HttpRequest& request, const CancellationToken& cancellation) override;
};

} // namespace beatnext::quest
