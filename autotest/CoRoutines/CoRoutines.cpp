#include <Core/Core.h>
#include <CoRoutines/CoRoutines.h>

using namespace Upp;

// Helpers

CoRoutine<int> SimpleRoutine()
{
    co_return 42;
}

CoRoutine<int> StepRoutine()
{
    co_await CoSuspend{};
    co_await CoSuspend{};
    co_return 7;
}

CoRoutine<void> VoidRoutine(bool& flag)
{
    co_await CoSuspend{};
    flag = true;
    co_return;
}

CoRoutine<int> ThrowRoutine()
{
    co_await CoSuspend{};
    throw Exc("boom");
}

CoGenerator<int> SimpleGenerator()
{
    co_yield 1;
    co_yield 2;
    co_yield 3;
}

CoGenerator<int> RangeGenerator(int n)
{
    for(int i = 0; i < n; i++)
        co_yield i;
}

CoGenerator<int> ThrowGenerator()
{
    co_yield 1;
    throw Exc("gen boom");
}

// CoRoutine Tests

void TestRoutine()
{
    // Immediate completion
    {
        auto r = SimpleRoutine();

        ASSERT(!r.Do());          // finishes immediately
        ASSERT(r.Get() == 42);
        ASSERT(!r.Do());          // remains finished
    }

    // Multi-step routine (2 suspends)
    {
        auto r = StepRoutine();

        ASSERT(r.Do());           // suspend #1
        ASSERT(r.Do());           // suspend #2
        ASSERT(!r.Do());          // finishes here
        ASSERT(r.Get() == 7);
        ASSERT(!r.Do());          // stable after completion
    }

    // Void routine
    {
        bool flag = false;
        auto r = VoidRoutine(flag);

        ASSERT(r.Do());           // suspend
        ASSERT(!flag);

        ASSERT(!r.Do());          // finishes
        ASSERT(flag);

        ASSERT(!r.Do());          // stable
    }

    // Exception propagation
    {
        auto r = ThrowRoutine();

        ASSERT(r.Do());           // suspend

        bool caught = false;
        try {
            r.Do();               // resumes and throws
        }
        catch(const Exc&) {
            caught = true;
        }

        ASSERT(caught);
        ASSERT(!r.Do());          // coroutine must now be done
    }

    // Move semantics
    {
        auto r1 = StepRoutine();
        auto r2 = pick(r1);

        ASSERT(r2.Do());
        ASSERT(r2.Do());
        ASSERT(!r2.Do());
        ASSERT(r2.Get() == 7);
    }

    LOG("CoRoutine: All tests passed.");
}

// CoGenerator Tests

void TestGenerator()
{
    // Simple sequence
    {
        auto g = SimpleGenerator();

        ASSERT(g.Next() == 1);
        ASSERT(g.Next() == 2);
        ASSERT(g.Next() == 3);

        bool thrown = false;
        try {
            g.Next();
        }
        catch(const Exc&) {
            thrown = true;
        }

        ASSERT(thrown);
    }

    // Range generator
    {
        auto g = RangeGenerator(5);

        for(int i = 0; i < 5; i++)
            ASSERT(g.Next() == i);

        bool thrown = false;
        try {
            g.Next();
        }
        catch(const Exc&) {
            thrown = true;
        }

        ASSERT(thrown);
    }

    // Iterator support
    {
        auto g = RangeGenerator(4);

        int expected = 0;
        for(int v : g) {
            ASSERT(v == expected);
            expected++;
        }

        ASSERT(expected == 4);
    }

    // Exception propagation
    {
        auto g = ThrowGenerator();

        ASSERT(g.Next() == 1);

        bool caught = false;
        try {
            g.Next();
        }
        catch(const Exc&) {
            caught = true;
        }

        ASSERT(caught);
    }

    // Move semantics
    {
        auto g1 = RangeGenerator(3);
        auto g2 = pick(g1);

        ASSERT(g2.Next() == 0);
        ASSERT(g2.Next() == 1);
        ASSERT(g2.Next() == 2);
    }

    LOG("CoGenerator: All tests passed.");
}

// Edge / Stability Tests

void TestEdgeCases()
{
    // Repeated Do() after completion
    {
        auto r = SimpleRoutine();
        ASSERT(!r.Do());
        ASSERT(!r.Do());
        ASSERT(!r.Do());
    }

    // Generator exhaustion stability
    {
        auto g = RangeGenerator(1);

        ASSERT(g.Next() == 0);

        bool thrown1 = false;
        bool thrown2 = false;

        try { g.Next(); } catch(const Exc&) { thrown1 = true; }
        try { g.Next(); } catch(const Exc&) { thrown2 = true; }

        ASSERT(thrown1);
        ASSERT(thrown2);
    }

    LOG("CoRoutine Edge cases: All tests passed.");
}

CONSOLE_APP_MAIN
{
    StdLogSetup(LOG_COUT | LOG_FILE);

    TestRoutine();
    TestGenerator();
    TestEdgeCases();

    LOG("All CoRoutine tests passed.");
}
