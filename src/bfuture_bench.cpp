#include "../include/orizzonte.hpp"
#include <atomic>
#include <boost/thread/thread_pool.hpp>
#include <boost/variant.hpp>
#include <chrono>
#include <experimental/type_traits>
#include <iostream>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread/future.hpp>

namespace ou = orizzonte::utility;

boost::executors::basic_thread_pool pool;

struct S
{
    template <typename F>
    void operator()(F&& f)
    {
        // std::thread{std::move(f)}.detach();
        pool.submit(std::move(f));
    }
};


using hr_clock = std::chrono::high_resolution_clock;

template <typename TF>
void bench(const std::string& title, TF&& f)
{
    constexpr int times = 1000;
    double acc = 0;

    for(int i(0); i < times; ++i)
    {
        const auto start = hr_clock::now();
        {
            f();
        }

        const auto dur = hr_clock::now() - start;
        const auto cnt =
            std::chrono::duration_cast<std::chrono::microseconds>(dur).count();

        acc += cnt;
    }

    std::cout << title << " | " << ((acc / times) / 1000.0) << " ms\n";
}

#define ENSURE(...)       \
    if(!(__VA_ARGS__))    \
    {                     \
        std::terminate(); \
    }

using namespace orizzonte::node;
using orizzonte::utility::sync_execute;

void sleepus(int x)
{
    usleep(x);
}

template <typename R, typename F>
auto make_bf(F&& f) -> boost::future<R>
{
    return boost::async(boost::launch::async, FWD(f));
}

template <typename F>
void with_ns(F&& f)
{
    f(0);
    for(int i = 0; i < 5; ++i)
    {
        f(std::pow(10, i));
    }

    std::cout << '\n';
}

/*
    (A)
*/
void b0_single_node(int d)
{
    bench(std::to_string(d) + "\tus - sngl - boostfutu", [&] {
        auto f = make_bf<int>([&] {
            sleepus(d);
            return 42;
        });
        ENSURE(f.get() == 42);
    });

    bench(std::to_string(d) + "\tus - sngl - orizzonte", [&] {
        auto f = leaf{[&] {
            sleepus(d);
            return 42;
        }};
        sync_execute(S{}, f, [](int x) { ENSURE(x == 42); });
    });
}

/*
    (A) -> (B) -> (C)
*/
void b1_then(int d)
{
    bench(std::to_string(d) + "\tus - then - boostfutu", [&] {
        auto g0 = make_bf<int>([&] {
            sleepus(d);
            return 42;
        });

        auto g1 =
            g0.then(boost::launch::async, [&](auto x) { return x.get() + 1; });

        auto f =
            g1.then(boost::launch::async, [&](auto x) { return x.get() + 1; });

        ENSURE(f.get() == 44);
    });

    bench(std::to_string(d) + "\tus - then - orizzonte", [&] {
        auto f = seq{seq{leaf{[&] {
                             sleepus(d);
                             return 42;
                         }},
                         leaf{[&](int x) { return x + 1; }}},
            leaf{[&](int x) { return x + 1; }}};

        sync_execute(S{}, f, [](int x) { ENSURE(x == 44); });
    });
}

/*
           -> (B0) \
         /          \
       -> (B1) -----> (C)
         \          /
          -> (B2)  /
*/
void b2_whenall(int d)
{
    bench(std::to_string(d) + "\tus - wall - boostfutu", [&] {
        auto b0 = make_bf<int>([&] {
            sleepus(d);
            return 0;
        });
        auto b1 = make_bf<int>([&] {
            sleepus(d);
            return 1;
        });
        auto b2 = make_bf<int>([&] {
            sleepus(d);
            return 2;
        });

        auto b = boost::when_all(std::move(b0), std::move(b1), std::move(b2));
        auto k = b.then([](auto x) {
            auto r = x.get();
            ENSURE(std::get<0>(r).get() == 0);
            ENSURE(std::get<1>(r).get() == 1);
            ENSURE(std::get<2>(r).get() == 2);

            return 42;
        });

        auto f =
            k.then(boost::launch::async, [&](auto x) { return x.get() + 2; });

        ENSURE(f.get() == 44);
    });

    bench(std::to_string(d) + "\tus - wall - orizzonte", [&] {
        auto f = seq{seq{all{leaf{[&] {
                                 sleepus(d);
                                 return 0;
                             }},
                             leaf{[&] {
                                 sleepus(d);
                                 return 1;
                             }},
                             leaf{[&] {
                                 sleepus(d);
                                 return 2;
                             }}},
                         leaf{[&](ou::cache_aligned_tuple<int, int, int> r) {
                             ENSURE(ou::get<0>(r) == 0);
                             ENSURE(ou::get<1>(r) == 1);
                             ENSURE(ou::get<2>(r) == 2);

                             return 42;
                         }}},
            leaf{[&](int x) { return x + 2; }}};

        sync_execute(S{}, f, [](int x) { ENSURE(x == 44); });
    });
}

/*
           -> (B0)
         /
       -> (B1) -----> (C)
         \
          -> (B2)
*/
void b3_whenany(int d)
{
    bench(std::to_string(d) + "\tus - wany - boostfutu", [&] {
        auto b0 = make_bf<int>([&] {
            sleepus(d);
            return 0;
        });
        auto b1 = make_bf<int>([&] {
            sleepus(d);
            return 1;
        });
        auto b2 = make_bf<int>([&] {
            sleepus(d);
            return 2;
        });

        auto b = boost::when_any(std::move(b0), std::move(b1), std::move(b2));
        auto k = b.then([](auto x) {
            auto r = x.get();
            if(std::get<0>(r).is_ready())
            {
                return 42 + std::get<0>(r).get();
            }
            if(std::get<1>(r).is_ready())
            {
                return 42 + std::get<1>(r).get();
            }
            return 42 + std::get<2>(r).get();
        });

        auto f =
            k.then(boost::launch::async, [&](auto x) { return x.get() + 2; });

        ENSURE(f.get() > 41);
    });

    bench(std::to_string(d) + "\tus - wany - orizzonte", [&] {
        auto f = seq{seq{any{leaf{[&] {
                                 sleepus(d);
                                 return 0;
                             }},
                             leaf{[&] {
                                 sleepus(d);
                                 return 1;
                             }},
                             leaf{[&] {
                                 sleepus(d);
                                 return 2;
                             }}},
                         leaf{[&](orizzonte::variant<int, int, int> r) {
                             return 42 + boost::apply_visitor(
                                             [](int x) { return x; }, r);
                         }}},
            leaf{[&](int x) { return x + 2; }}};

        sync_execute(S{}, f, [](int x) { ENSURE(x > 41); });
    });
}

int main()
{
    with_ns(b0_single_node);
    with_ns(b1_then);
    with_ns(b2_whenall);
    with_ns(b3_whenany);
}