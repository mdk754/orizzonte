// Copyright (c) 2017 Vittorio Romeo
// MIT License |  https://opensource.org/licenses/MIT
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#pragma once

#include "../utility/noop.hpp"
#include "./helper.hpp"

namespace orizzonte::node
{
    template <typename A, typename B>
    class seq : A, B
    {
    public:
        using in_type = typename A::in_type;
        using out_type = typename B::out_type;

        constexpr seq(A&& a, B&& b) : A{std::move(a)}, B{std::move(b)}
        {
        }

        template <typename Scheduler, typename Input, typename Then>
        void execute(Scheduler& scheduler, Input&& input,
            Then&& then = utility::noop_v) &
        {
            // A `seq` doesn't schedule a computation on a separate
            // thread by default. `A` could however be executed asynchronously -
            // arguments to this function need to be captured inside the closure
            // passed to `A`.
            static_cast<A&>(*this).execute(
                scheduler, FWD(input), [this, &scheduler, then](auto&& out) {
                    static_cast<B&>(*this).execute(scheduler, FWD(out), then);
                });
        }
    };
}