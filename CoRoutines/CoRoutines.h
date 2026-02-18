#ifndef _Upp_CoRoutines_h_
#define _Upp_CoRoutines_h_

#include <Core/Core.h>

#include <concepts>
#include <coroutine>
#include <type_traits>

namespace Upp {

using CoSuspend     = std::suspend_always;
using CoDontSuspend = std::suspend_never;

enum class CoRoutineType { Routine, Generator };

template<CoRoutineType R, typename T>
class CoRoutineT {
	static constexpr const char *errmsg = "Generator exhausted";
public:

    // Routine (non-void)
    template<typename U>
    struct ReturnValue {
        using Handle = std::coroutine_handle<ReturnValue>;

        CoRoutineT get_return_object()
        {
            return CoRoutineT(Handle::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }

        template<std::convertible_to<U> L>
        void return_value(L&& val)
        {
            value = std::forward<L>(val);
        }

        void unhandled_exception()
        {
            exc = std::current_exception();
        }

        std::exception_ptr exc;
        U value {};
    };

    // Routine (void)
    struct VoidValue {
        using Handle = std::coroutine_handle<VoidValue>;

        CoRoutineT get_return_object()
        {
            return CoRoutineT(Handle::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }

        void return_void() noexcept {}

        void unhandled_exception()
        {
            exc = std::current_exception();
        }

        std::exception_ptr exc;
    };

    // Generator
    template<typename U>
    struct CurrentValue {
        using Handle = std::coroutine_handle<CurrentValue>;

        CoRoutineT get_return_object()
        {
            return CoRoutineT(Handle::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }

        template<std::convertible_to<U> L>
        std::suspend_always yield_value(L&& val)
        {
            value = std::forward<L>(val);
            return {};
        }

        void return_void() noexcept {}

		template<typename X>
		std::suspend_never await_transform(X&&) = delete;


        void unhandled_exception()
        {
            exc = std::current_exception();
        }

        std::exception_ptr exc;
        U value {};
    };

public:

    using promise_type =
        typename std::conditional<
            std::is_void_v<T>,
                VoidValue,
                typename std::conditional<
                    R == CoRoutineType::Routine,
                        ReturnValue<T>,
                        CurrentValue<T>
                >::type
        >::type;

    using Handle = std::coroutine_handle<promise_type>;

    // Routine API
    bool Do()
        requires (R == CoRoutineType::Routine)
    {
        ASSERT(co);

        if(co.done())
            return false;

        co.resume();
        Rethrow();

        return !co.done();
    }

    T Get() const
        requires (R == CoRoutineType::Routine && !std::is_void_v<T>)
    {
        ASSERT(co);
        ASSERT(co.done());
        return co.promise().value;
    }

    T operator~() const
        requires (R == CoRoutineType::Routine && !std::is_void_v<T>)
    {
        return Get();
    }

    T Pick()
        requires (R == CoRoutineType::Routine && !std::is_void_v<T>)
    {
        ASSERT(co);
        ASSERT(co.done());
        return pick(co.promise().value);
    }

    // Generator API
    T Next()
        requires (R == CoRoutineType::Generator)
    {
        ASSERT(co);

        if(co.done())
            throw Exc(errmsg);

        co.resume();
        Rethrow();

        if(co.done())
            throw Exc(errmsg);

        return co.promise().value;
    }

    T operator~()
        requires (R == CoRoutineType::Generator)
    {
        return Next();
    }

    T PickNext()
        requires (R == CoRoutineType::Generator)
    {
        ASSERT(co);

        if(co.done())
            throw Exc(errmsg);

        co.resume();
        Rethrow();

        if(co.done())
            throw Exc(errmsg);

        return pick(co.promise().value);
    }

    explicit CoRoutineT(Handle h)
        : co(h)
    {}

    CoRoutineT(CoRoutineT&& r) noexcept
        : co(r.co)
    {
        r.co = {};
    }

    CoRoutineT& operator=(CoRoutineT&& r) noexcept
    {
        if(this != &r) {
            if(co)
                co.destroy();
            co = r.co;
            r.co = {};
        }
        return *this;
    }

    CoRoutineT(const CoRoutineT&) = delete;
    CoRoutineT& operator=(const CoRoutineT&) = delete;

    ~CoRoutineT() noexcept
    {
        if(co)
            co.destroy();
    }

    // Generator-only
    class Iterator {
    public:
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        explicit Iterator(CoRoutineT* g)
            : gen(g)
        {
            if(gen && !gen->co.done()) {
                gen->co.resume();
                gen->Rethrow();
                if(gen->co.done())
                    gen = nullptr;
            }
        }

        value_type operator*() const
        {
			ASSERT(gen);
			return gen->co.promise().value;
        }

        Iterator& operator++()
        {
            gen->co.resume();
            gen->Rethrow();
            if(gen->co.done())
                gen = nullptr;
            return *this;
        }

        bool operator==(std::default_sentinel_t) const
        {
            return gen == nullptr;
        }

        bool operator!=(std::default_sentinel_t s) const
        {
            return !(*this == s);
        }

    private:
        CoRoutineT* gen = nullptr;
    };

    Iterator begin()
        requires (R == CoRoutineType::Generator)
    {
        return Iterator(this);
    }

    std::default_sentinel_t end()
        requires (R == CoRoutineType::Generator)
    {
        return {};
    }

private:

    void Rethrow() const
    {
        if(co.promise().exc)
            std::rethrow_exception(co.promise().exc);
    }

private:
    Handle co {};
};

// Aliases

template<typename T>
using CoRoutine = CoRoutineT<CoRoutineType::Routine, T>;

template<typename T>
requires (!std::is_void_v<T>)
using CoGenerator = CoRoutineT<CoRoutineType::Generator, T>;

}

#endif
