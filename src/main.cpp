// Traffic Management System (Simulation) - entry point (Team E16).
#include <iostream>
#include <random>

#include "app/application.h"

int main() {
    AppOptions options;
    std::random_device rd;
    // Salts and session ids must not be predictable from the simulation seed.
    options.authSeed = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());
    Application app(std::cin, std::cout, options);
    return app.run();
}
