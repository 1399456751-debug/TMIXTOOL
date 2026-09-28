#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace tmixtest {

struct TestCase
{
    std::string name;
    std::function<void()> fn;
};

// Thrown by TMIX_REQUIRE to abandon the rest of a case once a precondition
// has failed, so dependent code cannot index into a container the failed
// check was supposed to guarantee. Caught by main().
struct TestAbort {};

std::vector<TestCase>& registry();

// Per-case bookkeeping. main() brackets every case with these.
void beginCase();
int  failureCount();
int  checkCount();

void recordFailure (const char* file, int line, const std::string& message);
void recordCheck();

std::string toString (double value);

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn);
};

} // namespace tmixtest

#define TMIX_TEST(name)                                                       \
    static void name();                                                       \
    static ::tmixtest::Registrar tmix_registrar_##name (#name, name);         \
    static void name()

#define TMIX_CHECK(cond)                                                      \
    do {                                                                      \
        ::tmixtest::recordCheck();                                            \
        if (! (cond))                                                         \
            ::tmixtest::recordFailure (__FILE__, __LINE__,                    \
                                       std::string ("expected: ") + #cond);   \
    } while (false)

// Fatal variant: reports the failure and abandons the current case. Use it for
// preconditions that later assertions depend on - for example a size check
// before indexing, or a decode-success check before reading decoded data.
#define TMIX_REQUIRE(cond)                                                    \
    do {                                                                      \
        ::tmixtest::recordCheck();                                            \
        if (! (cond))                                                         \
        {                                                                     \
            ::tmixtest::recordFailure (__FILE__, __LINE__,                    \
                                       std::string ("required: ") + #cond);   \
            throw ::tmixtest::TestAbort();                                    \
        }                                                                     \
    } while (false)

#define TMIX_CHECK_NEAR(actual, expected, tol)                                \
    do {                                                                      \
        ::tmixtest::recordCheck();                                            \
        const double tmixActual_  = static_cast<double> (actual);             \
        const double tmixExpect_  = static_cast<double> (expected);           \
        const double tmixTol_     = static_cast<double> (tol);                \
        if (! (std::fabs (tmixActual_ - tmixExpect_) <= tmixTol_))            \
            ::tmixtest::recordFailure (                                       \
                __FILE__, __LINE__,                                           \
                std::string (#actual) + " = " + ::tmixtest::toString (tmixActual_) \
                    + ", expected " + ::tmixtest::toString (tmixExpect_)      \
                    + " +/- " + ::tmixtest::toString (tmixTol_));             \
    } while (false)

#define TMIX_CHECK_EQ(actual, expected)                                       \
    do {                                                                      \
        ::tmixtest::recordCheck();                                            \
        const auto tmixActual_ = (actual);                                    \
        const auto tmixExpect_ = (expected);                                  \
        if (! (tmixActual_ == tmixExpect_))                                   \
            ::tmixtest::recordFailure (                                       \
                __FILE__, __LINE__,                                           \
                std::string (#actual) + " == " + #expected + " (got "         \
                    + ::tmixtest::toString (static_cast<double> (tmixActual_)) \
                    + ", wanted "                                             \
                    + ::tmixtest::toString (static_cast<double> (tmixExpect_)) \
                    + ")");                                                   \
    } while (false)
