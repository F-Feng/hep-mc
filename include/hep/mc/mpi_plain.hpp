#ifndef HEP_MC_MPI_PLAIN_HPP
#define HEP_MC_MPI_PLAIN_HPP

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

#include <hep/mc/mc_point.hpp>
#include <hep/mc/mc_result.hpp>
#include <hep/mc/mpi_helper.hpp>
#include <hep/mc/mpi_datatype.hpp>
#include <hep/mc/plain.hpp>

#include <cstddef>
#include <limits>
#include <random>
#include <vector>

#include <mpi.h>

namespace hep
{

/**
 * \addtogroup plain
 * @{
 */

/**
 * MPI-parallelized PLAIN Monte Carlo integrator. This function integrates
 * `function` over the unit-hypercube with the specified `dimensions` using
 * `calls` function evaluations at randomly chosen points determined by
 * `generator`. The generator is not seeded.
 *
 * \param communicator The MPI communicator that is used to communicate between
 *        the different MPI processes.
 * \param dimensions The number of parameters `function` accepts.
 * \param calls The number of function calls that are used to obtain the result.
 * \param function The function that will be integrated over the hypercube. See
 *        \ref integrands for further explanation.
 * \param generator The random number generator that will be used to generate
 *        random points from the hypercube. This generator is not seeded.
 */
template <typename T, typename F, typename R = std::mt19937>
inline mc_result<T> mpi_plain(
	MPI_Comm communicator,
	std::size_t dimensions,
	std::size_t calls,
	F&& function,
	R&& generator = std::mt19937()
) {
	int rank = 0;
	MPI_Comm_rank(communicator, &rank);
	int world = 0;
	MPI_Comm_size(communicator, &world);

	if (!mpi_single_generator())
	{
		// seed every random number generator differently
		std::size_t const r = rank * 10;
		std::seed_seq sequence{r + 0, r + 1, r + 2, r + 3, r + 4,
			r + 5, r + 6, r + 7, r + 8, r + 9};
		generator.seed(sequence);
	}

	// default-initialize sum and sum_of_squares
	T  buffer[2]      = { T(), T() };
	T& sum            = buffer[0];
	T& sum_of_squares = buffer[1];

	std::vector<T> random_numbers(dimensions);

	// the number of function calls for each MPI process
	std::size_t const sub_calls = (calls / world) +
		(static_cast <std::size_t> (rank) < (calls % world) ? 1 : 0);

	if (mpi_single_generator())
	{
		mpi_advance_generator_before<T>(
			dimensions, calls, rank, world, generator);
	}

	auto result = plain_iteration<T>(
		dimensions, calls, sub_calls, function, generator);

	sum = result.value * T(result.calls);
	sum_of_squares = T(result.calls) * (result.value * result.value + T(calls) *
		result.error * result.error);

	if (mpi_single_generator())
	{
		mpi_advance_generator_after<T>(
			dimensions, calls, sub_calls, rank, world, generator
		);
	}

	MPI_Allreduce(
		MPI_IN_PLACE,
		&buffer,
		2,
		mpi_datatype<T>(),
		MPI_SUM,
		communicator
	);

	return mc_result<T>(calls, sum, sum_of_squares);
}

/**
 * @}
 */

}

#endif