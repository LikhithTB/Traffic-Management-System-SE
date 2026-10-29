#include "test_framework.h"

int main() {
    int run = 0;
    for (const auto& t : testRegistry()) {
        const int before = testFailures();
        t.fn();
        ++run;
        std::cout << (testFailures() == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << run << " tests, " << testFailures() << " failed checks\n";
    return testFailures() == 0 ? 0 : 1;
}
