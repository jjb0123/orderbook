#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {
struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct CheckFailed : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline std::string where(const char* file, int line, const char* expr) {
  std::ostringstream os;
  os << file << ':' << line << ": CHECK(" << expr << ")";
  return os.str();
}

}  // namespace test

#define OB_CAT2(a, b) a##b
#define OB_CAT(a, b) OB_CAT2(a, b)

#define TEST(name)                                           \
  static void name();                                        \
  static ::test::Registrar OB_CAT(reg_, name){#name, &name}; \
  static void name()

#define CHECK(expr)                                                                     \
  do {                                                                                  \
    if (!(expr)) throw ::test::CheckFailed(::test::where(__FILE__, __LINE__, #expr)); \
  } while (0)
