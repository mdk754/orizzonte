#pragma once

#include <vrm/core/tuple_utils.hpp>
#include <vrm/core/utility_macros.hpp>

namespace orizzonte::impl
{
    using vrm::core::apply;

    template <typename TF, typename TTuple>
    decltype(auto) for_tuple(TF&& f, TTuple&& t)
    {
        return orizzonte::impl::apply(
            [&f](auto&&... xs) { (f(FWD(xs)), ...); }, FWD(t));
    }

    template <typename TF, typename... TTuples>
    decltype(auto) multi_apply(TF&& f, TTuples&&... xs)
    // TODO: noexcept
    {
        // TODO: replace with multi-index-generation
        return orizzonte::impl::apply(FWD(f), std::tuple_cat(FWD(xs)...));
    }
}