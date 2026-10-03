#pragma once

namespace beatflow::quest::ui {

void initialize();
void showForYou();
void showRecommendedNext();
void close(bool immediately = false);
bool openingRecommendedNext();

} // namespace beatflow::quest::ui
