#include "TestHarness.h"

#include <cstdio>
#include <exception>
#include <string>

int main (int argc, char** argv)
{
    // Optional filter: TMIXTests <substring> runs only matching cases.
    std::string filter;
    if (argc > 1)
        filter = argv[1];

    auto& cases = tmixtest::registry();

    int failedCases = 0;
    int ranCases    = 0;
    int totalChecks = 0;

    for (auto& testCase : cases)
    {
        if (! filter.empty() && testCase.name.find (filter) == std::string::npos)
            continue;

        ++ranCases;
        std::printf ("[ RUN  ] %s\n", testCase.name.c_str());
        // Flush per case: if a case crashes the process, the output up to that
        // point must still be visible instead of dying in the stdio buffer.
        std::fflush (stdout);

        tmixtest::beginCase();

        try
        {
            testCase.fn();
        }
        catch (const tmixtest::TestAbort&)
        {
            // A TMIX_REQUIRE precondition failed and already reported itself.
        }
        catch (const std::exception& e)
        {
            tmixtest::recordFailure (__FILE__, __LINE__,
                                     std::string ("unexpected exception: ") + e.what());
        }
        catch (...)
        {
            tmixtest::recordFailure (__FILE__, __LINE__,
                                     "unexpected non-standard exception");
        }

        const int failures = tmixtest::failureCount();
        totalChecks += tmixtest::checkCount();

        if (failures == 0)
        {
            std::printf ("[  OK  ] %s\n", testCase.name.c_str());
        }
        else
        {
            ++failedCases;
            std::printf ("[ FAIL ] %s  (%d failed assertion(s))\n",
                         testCase.name.c_str(), failures);
        }
    }

    std::printf ("\n%d case(s) run, %d failed, %d assertion(s) checked\n",
                 ranCases, failedCases, totalChecks);

    if (ranCases == 0)
    {
        std::printf ("No test cases matched");
        if (! filter.empty())
            std::printf (" filter '%s'", filter.c_str());
        std::printf ("\n");
        return 2;
    }

    return failedCases == 0 ? 0 : 1;
}
