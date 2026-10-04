#pragma once

#include "beatnext/core/Error.hpp"

#include <functional>
#include <string>

namespace beatnext::quest {

class SongSelectionNavigator {
  public:
    using Callback = std::function<void(Outcome<bool>)>;
    void open(const std::string& hash, Callback callback = {});

  private:
    void selectWhenReady(const std::string& hash, int attemptsRemaining, Callback callback);
};

} // namespace beatnext::quest
