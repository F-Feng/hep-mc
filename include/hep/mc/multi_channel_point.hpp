#ifndef HEP_MC_MULTI_CHANNEL_POINT_HPP
#define HEP_MC_MULTI_CHANNEL_POINT_HPP

/*
 * hep-mc - A Template Library for Monte Carlo Integration
 * Copyright (C) 2015  Christopher Schwan
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

#include "hep/mc/mc_point.hpp"

#include <cstddef>
#include <vector>

namespace hep
{

/// \addtogroup integrands
/// @{

/// Point in the unit-hypercube for multi-channel Monte Carlo integration.
template <typename T>
struct multi_channel_point : public mc_point<T>
{
	/// Constructor.
	multi_channel_point(
		std::size_t calls,
		std::vector<T> const& point,
		std::vector<T> const& coordinates,
		std::size_t channel,
		T total_density
	)
		: mc_point<T>(calls, point)
		, channel(channel)
		, coordinates(coordinates)
	{
		this->weight /= total_density;
	}

	/// The selected channel for this point.
	std::size_t channel;

	/// The point in the hypercube transformed by the current \ref channel.
	std::vector<T> const& coordinates;
};

/// @}

}

#endif
