#ifndef HEP_MC_MULTI_CHANNEL_HPP
#define HEP_MC_MULTI_CHANNEL_HPP

/*
 * hep-mc - A Template Library for Monte Carlo Integration
 * Copyright (C) 2015-2019  Christopher Schwan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hep/mc/accumulator.hpp"
#include "hep/mc/discrete_distribution.hpp"
#include "hep/mc/integrand.hpp"
#include "hep/mc/multi_channel_callback.hpp"
#include "hep/mc/multi_channel_chkpt.hpp"
#include "hep/mc/multi_channel_map.hpp"
#include "hep/mc/multi_channel_point.hpp"
#include "hep/mc/multi_channel_refine_weights.hpp"
#include "hep/mc/multi_channel_result.hpp"

#include <cstddef>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace hep
{

/// \addtogroup multi_channel_group
/// @{

/// Performs exactly one iteration of the multi channel integration. The parameter `channel_weights`
/// lets the user specify the weights of each channel. Note that they must add up to one. See \ref
/// multi_channel_group for a description of the remaining parameters.
template <typename I, typename R>
inline multi_channel_result<numeric_type_of<I>> multi_channel_iteration(
    I&& integrand,
    std::size_t calls,
    std::vector<numeric_type_of<I>> const& channel_weights,
    R& generator
) {
    using T = numeric_type_of<I>;

    auto accumulator = make_accumulator(integrand);

    std::size_t const channels = channel_weights.size();

    std::vector<T> random_numbers(integrand.dimensions());
    std::vector<T> coordinates(integrand.map_dimensions());
    std::vector<T> densities(channels);
    std::vector<T> adjustment_data(channels);

    std::vector<std::size_t> enabled_channels;
    enabled_channels.reserve(channels);

    for (std::size_t i = 0; i != channels; ++i)
    {
        if (channel_weights.at(i) != T())
        {
            enabled_channels.push_back(i);
        }
    }

    // distribution that randomly selects a channel
    discrete_distribution<std::size_t, T> channel_selector(channel_weights.begin(),
        channel_weights.end());

    for (std::size_t i = 0; i != calls; ++i)
    {
        // generate as many random numbers as we need
        for (std::size_t j = 0; j != integrand.dimensions(); ++j)
        {
            random_numbers[j] = std::generate_canonical<T,
                std::numeric_limits<T>::digits>(generator);
        }

        // randomly select a channel
        std::size_t const channel = channel_selector(generator);

        using map_type = typename std::remove_reference<
            typename std::remove_reference<I>::type::map_type>::type;

        // calculate `coordinates` and possibly `densities`
        integrand.map()(
            channel,
            random_numbers,
            coordinates,
            enabled_channels,
            densities,
            multi_channel_map::calculate_coordinates
        );

        multi_channel_point2<T, map_type> const point(
            random_numbers,
            coordinates,
            channel,
            densities,
            channel_weights,
            enabled_channels,
            integrand.map()
        );

        T const value = accumulator.invoke(integrand, point);

        if (value == T())
        {
            continue;
        }

        T const square = value * value * point.weight();

        // these are the values W that are used to update the alphas
        for (std::size_t j = 0; j != channels; ++j)
        {
            adjustment_data[j] += densities[j] * square;
        }
    }

    return multi_channel_result<T>(accumulator.result(calls), adjustment_data, channel_weights);
}

///
template <typename I, typename Checkpoint = default_multi_channel_chkpt<numeric_type_of<I>>>
inline Checkpoint multi_channel(
    I&& integrand,
    std::vector<std::size_t> iteration_calls,
    Checkpoint chkpt = make_multi_channel_chkpt<numeric_type_of<I>>()
) {
    using T = numeric_type_of<I>;

    chkpt.channels(integrand.channels());

    auto generator = chkpt.generator();
    auto weights = chkpt.channel_weights();

    for (auto const calls : iteration_calls)
    {
        auto const result = multi_channel_iteration(integrand, calls, weights, generator);

        chkpt.add(result, generator);

        if (!multi_channel_callback<T>()(chkpt))
        {
            break;
        }

        weights = multi_channel_refine_weights(weights, result.adjustment_data(),
            chkpt.min_weight(), chkpt.beta());
    }

    return chkpt;
}

/// @}

}

#endif
