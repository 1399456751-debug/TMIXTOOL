#include "TestHarness.h"

#include <cstdio>

namespace tmixtest {

namespace {
int gFailures = 0;
int gChecks   = 0;
} // namespace

std::vector<TestCase>& registry()
{
    // Function-local static: safe regardless of static init order across TUs.
    static std::vector<TestCase> cases;
    return cases;
}

void beginCase()
{
    gFailures = 0;
    gChecks   = 0;
}

int failureCount() { return gFailures; }
int checkCount()   { return gChecks; }

void recordFailure (const char* file, int line, const std::string& message)
{
    ++gFailures;

    // Trim the path down to the file name so the output stays readable.
    std::string path (file);
    const auto slash = path.find_last_of ("/\\");
    if (slash != std::string::npos)
        path = path.substr (slash + 1);

    std::printf ("    FAIL %s:%d  %s\n", path.c_str(), line, message.c_str());
    std::fflush (stdout);
}

void recordCheck() { ++gChecks; }

std::string toString (double value)
{
    char buffer[64];
    std::snprintf (buffer, sizeof (buffer), "%.6g", value);
    return buffer;
}

Registrar::Registrar (const char* name, std::function<void()> fn)
{
    registry().push_back ({ name, std::move (fn) });
}

} // namespace tmixtest
