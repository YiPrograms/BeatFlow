#pragma once

#include "beatflow/core/Interfaces.hpp"

namespace beatflow::quest {

class QuestHttpClient final : public HttpClient {
  public:
    Outcome<HttpResponse> send(const HttpRequest& request, const CancellationToken& cancellation) override;
};

} // namespace beatflow::quest
