#pragma once

#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace beatflow::test {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

inline void require(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::ostringstream message;
        message << file << ':' << line << ": requirement failed: " << expression;
        throw std::runtime_error(message.str());
    }
}

inline void requireNear(double actual, double expected, double tolerance, const char* file, int line) {
    if (std::abs(actual - expected) > tolerance) {
        std::ostringstream message;
        message << file << ':' << line << ": expected " << expected << " ± " << tolerance << ", received "
                << actual;
        throw std::runtime_error(message.str());
    }
}

} // namespace beatflow::test

#define BF_JOIN_INNER(left, right) left##right
#define BF_JOIN(left, right) BF_JOIN_INNER(left, right)
#define BF_TEST(name)                                                                                        \
    static void BF_JOIN(beatflow_test_, __LINE__)();                                                         \
    static ::beatflow::test::Registrar BF_JOIN(beatflow_registrar_,                                          \
                                               __LINE__)(name, BF_JOIN(beatflow_test_, __LINE__));           \
    static void BF_JOIN(beatflow_test_, __LINE__)()
#define BF_REQUIRE(expression)                                                                               \
    ::beatflow::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define BF_REQUIRE_NEAR(actual, expected, tolerance)                                                         \
    ::beatflow::test::requireNear((actual), (expected), (tolerance), __FILE__, __LINE__)
