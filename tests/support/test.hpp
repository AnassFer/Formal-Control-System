#pragma once
#include "symctrl/contracts/types.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
namespace test {
inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::ostringstream s; s << file << ':' << line << ": failed: " << expression;
        throw std::runtime_error(s.str());
    }
}
inline bool near(double a, double b, double tolerance = 1e-10) {
    return std::isfinite(a) && std::isfinite(b) && std::abs(a-b) <= tolerance;
}
template<class Exception, class F> void throws(F&& operation) {
    try { operation(); }
    catch (const Exception&) { return; }
    throw std::runtime_error("expected exception was not thrown");
}
inline int run(int argc, char** argv, const std::map<std::string,std::function<void()>>& cases,
               bool feature = false) {
    try {
        if (argc != 2 || cases.count(argv[1]) == 0) throw std::invalid_argument("unknown test case");
        cases.at(argv[1])();
        std::cout << "PASS " << argv[1] << '\n'; return 0;
    } catch (const symctrl::NotImplemented& e) {
        std::cerr << (feature ? "PENDING " : "FAIL ") << e.what() << '\n';
        return feature ? 77 : 1;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
}
#define CHECK(...) ::test::check((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
