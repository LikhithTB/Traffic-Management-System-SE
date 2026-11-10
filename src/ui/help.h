#pragma once
// On-screen help (story #21, NFR-04, TC-17). Every menu prompt reads "Choice (lo-hi, h = help)";
// typing h, help or ? there prints the help text of that screen and asks again.
// Menus are at most 3 levels deep: Main -> Operations -> Intersections / Parameters / Users.
#include <string>

namespace help {

enum class Screen { Main, Operations, Commuter, Intersections, Parameters, Users };

const char* text(Screen screen);                  // fixed help text of one screen
bool isRequest(const std::string& input);         // "h", "help" or "?" (any case, spaces ignored)

}  // namespace help
