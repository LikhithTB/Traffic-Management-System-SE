#pragma once
// Tiny test framework (no external dependency). Usage:
//   TEST(name_of_test) { CHECK(cond); CHECK_EQ(a, b); }
#include <functional>
#include <iostream>
#include <vector>

struct TestCase { const char* name; std::function<void()> fn; };

inline std::vector<TestCase>& testRegistry() { static std::vector<TestCase> r; return r; }
inline int& testFailures() { static int f = 0; return f; }

struct TestRegistrar {
    TestRegistrar(const char* n, std::function<void()> f) { testRegistry().push_back({n, std::move(f)}); }
};

#define TEST(name) \
    static void name(); \
    static TestRegistrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) { std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
                         ++testFailures(); } } while (0)
#define CHECK_EQ(a, b) CHECK((a) == (b))
