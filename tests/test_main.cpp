#include <cstdio>
#include <cstring>
#include <exception>

#include "harness.hpp"

// Usage: ob_tests [substring]   runs tests whose name contains substring.
int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  int failed = 0;
  for (const auto& c : test::registry()) {
    if (filter && !std::strstr(c.name, filter)) continue;
    ++run;
    try {
      c.fn();
      std::printf("[ ok ] %s\n", c.name);
    } catch (const std::exception& e) {
      ++failed;
      std::printf("[FAIL] %s\n       %s\n", c.name, e.what());
    }
  }
  std::printf("\n%d run, %d failed\n", run, failed);
  return failed == 0 ? 0 : 1;
}
