#ifndef HEP_MC_HELPER_HPP
#define HEP_MC_HELPER_HPP

/*
 * hep-mc - A Template Library for Monte Carlo Integration
 * Copyright (C) 2013  Christopher Schwan
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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace
{

template <typename T, typename R>
inline std::size_t random_number_usage()
{
	// the number of random bits
	std::size_t const b = std::numeric_limits<T>::digits;

	// the number of different numbers the generator can generate
	long double const r = static_cast <long double> (R::max())
		- static_cast <long double> (R::min()) + 1.0L;

	// the number of bits needed to hold the value of 'r'
	std::size_t const log2r = std::log2(r);

	std::size_t const k = std::max<std::size_t>(1, (b + log2r - 1UL) / log2r);

	return k;
}

template <typename T, typename R>
inline void mpi_advance_generator_before(
	std::size_t dimensions,
	std::size_t total_calls,
	std::size_t rank,
	std::size_t world,
	R& generator
) {
	std::size_t const usage = random_number_usage<T, R>() * dimensions;

	std::size_t const before = (total_calls / world) * rank +
		(((total_calls % world) < rank) ? total_calls % world : rank);

	generator.discard(before * usage);
}

template <typename T, typename R>
inline void mpi_advance_generator_after(
	std::size_t dimensions,
	std::size_t total_calls,
	std::size_t calls,
	std::size_t rank,
	std::size_t world,
	R& generator
) {
	std::size_t const usage = random_number_usage<T, R>() * dimensions;

	std::size_t const before = (total_calls / world) * rank +
		(((total_calls % world) < rank) ? total_calls % world : rank);

	std::size_t const after = ((before + calls) < total_calls) ?
		(total_calls - before - calls) : 0;

	generator.discard(after * usage);
}

}

namespace hep
{

/// \addtogroup mpi_helper
/// @{

/**
 * This function returns true if the MPI functions should use a single random
 * number generator. If this is the case the generator of each process discards
 * an approriate amount of random numbers to ascertain independent numbers are
 * used. This make sure the result is independent of the number of processes.
 * However, since each process consumes as many random numbers as calls are made
 * in total, this may slow down the integration if the integrand evaluates too
 * fast. In this case a faster random number generator can be used.
 */
inline bool& mpi_single_generator()
{
	static bool use_single_generator = false;

	return use_single_generator;
}

/**
 * If `enabled` is true, the MPI function's results are independent of the
 * number of processes used to integrate.
 *
 * \see mpi_single_generator
 */
inline void mpi_single_generator(bool enabled)
{
	mpi_single_generator() = enabled;
}

/// @}

}

#endif
