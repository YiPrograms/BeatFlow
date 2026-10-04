#include "Test.hpp"

#include <exception>
#include <iostream>

int main() {
    std::size_t failures = 0;
    for (const auto& test : beatnext::test::registry()) {
        try {
            test.body();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }
    std::cout << beatnext::test::registry().size() - failures << "/" << beatnext::test::registry().size()
              << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
