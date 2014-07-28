#ifndef HEP_MC_VEGAS_HPP
#define HEP_MC_VEGAS_HPP

/*
 * hep-mc - A Template Library for Monte Carlo Integration
 * Copyright (C) 2012-2014  Christopher Schwan
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

#include "hep/mc/mc_helper.hpp"
#include "hep/mc/mc_point.hpp"
#include "hep/mc/mc_result.hpp"
#include "hep/mc/vegas_callback.hpp"
#include "hep/mc/vegas_iteration_result.hpp"
#include "hep/mc/vegas_pdf.hpp"
#include "hep/mc/vegas_point.hpp"

#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace hep
{

/// \addtogroup vegas
/// @{

/**
 * Integrates `function` over the unit-hypercube using `calls` function evaluations with random
 * numbers generated by `generator` which is not seeded. The `grid` is used to implement importance
 * sampling; stratified sampling is not used. The dimension of the function is determined by
 * `grid.size()`.
 *
 * If `total_calls` is larger than `calls` this means the iteration is split up in multiple
 * `vegas_iteration` calls that are typically run in parallel. In this case `total_calls` is the sum
 * of all of function evaluations used.
 */
template <typename T, typename F, typename R>
inline vegas_iteration_result<T> vegas_iteration(
	std::size_t calls,
	std::size_t total_calls,
	vegas_pdf<T> const& grid,
	F&& function,
	R&& generator
) {
	T average = T();
	T averaged_squares = T();

	// for kahan summation
	T compensation = T();

	std::size_t const dimensions = grid.dimensions();
	std::size_t const bins = grid.bins();

	std::vector<T> adjustment_data(dimensions * bins + 2);
	std::vector<T> random_numbers(dimensions);
	std::vector<std::size_t> bin(dimensions);

	for (std::size_t i = 0; i != calls; ++i)
	{
		for (std::size_t j = 0; j != dimensions; ++j)
		{
			random_numbers[j] = std::generate_canonical<T,
				std::numeric_limits<T>::digits>(generator);
		}

		vegas_point<T> const point(total_calls, random_numbers, bin, grid);

		// evaluate function at the specified point and multiply with its weight
		T const value = function(point) * point.weight;

		// perform kahan summation 'sum += value' - this improves precision if T is single precision
		// and many values are added
		T const y = value - compensation;
		T const t = average + y;
		compensation = (t - average) - y;
		average = t;

		T const square = value * value;

		// no kahan summation needed, because it only affects the result indirectly via the grid
		// recomputation
		averaged_squares += square;

		// save square for each bin in order to adjust the grid later
		for (std::size_t j = 0; j != dimensions; ++j)
		{
			adjustment_data[j * bins + point.bin[j]] += square;
		}
	}

	// save 'sum' and 'sum_of_squares' by rescaling the variables
	adjustment_data[dimensions * bins + 0] = average          * T(total_calls);
	adjustment_data[dimensions * bins + 1] = averaged_squares * T(total_calls) * T(total_calls);

	return vegas_iteration_result<T>(calls, grid, adjustment_data);
}

/**
 * Implements the VEGAS algorithm. This function can be used to start from an already adapted pdf,
 * e.g. one by \ref vegas_iteration_result.pdf obtained by a previous \ref vegas() call.
 */
template <typename T, typename F, typename R = std::mt19937>
inline std::vector<vegas_iteration_result<T>> vegas(
	std::vector<std::size_t> const& iteration_calls,
	F&& function,
	vegas_pdf<T> const& start_pdf,
	T alpha = T(1.5),
	R&& generator = std::mt19937()
) {
	auto pdf = start_pdf;

	// vector holding all iteration results
	std::vector<vegas_iteration_result<T>> results;
	results.reserve(iteration_calls.size());

	// perform iterations
	for (auto i = iteration_calls.begin(); i != iteration_calls.end(); ++i)
	{
		auto const result = vegas_iteration(*i, *i, pdf, function, generator);
		results.push_back(result);

		if (!vegas_callback<T>()(results))
		{
			break;
		}

		pdf = vegas_refine_pdf(alpha, pdf, result.adjustment_data);
	}

	return results;
}

/**
 * Implements the VEGAS algorithm. In particular, this function calls \ref vegas_iteration for every
 * number in `iteration_calls` determining the `calls` parameter for each iteration. After each
 * iteration the pdf is adjusted using \ref vegas_refine_pdf. The pdf refinement itself can be
 * controlled by the parameter `alpha`. The intermediate results are passed to the function set by
 * \ref vegas_callback which can e.g. be used to print them out. The callback function is able to
 * stop the integration if it returns `false`. In this case less iterations are performed than
 * requested.
 *
 * \param dimensions The number of parameters `function` accepts.
 * \param iteration_calls The number of function calls that are used to obtain a result for each
 *        iteration. `iteration_calls.size()` determines the number of iterations.
 * \param function The function that will be integrated over the hypercube. See \ref integrands for
 *        further explanation.
 * \param bins The number of bins that the grid will contain for each dimension.
 * \param alpha The \f$ \alpha \f$ parameter of VEGAS. This parameter is usually set between `1` and
 *        `2`.
 * \param generator The random number generator that will be used to generate random points from the
 *        hypercube. This generator is not seeded.
 */
template <typename T, typename F, typename R = std::mt19937>
inline std::vector<vegas_iteration_result<T>> vegas(
	std::size_t dimensions,
	std::vector<std::size_t> const& iteration_calls,
	F&& function,
	std::size_t bins = 128,
	T alpha = T(1.5),
	R&& generator = std::mt19937()
) {
	return vegas(
		iteration_calls,
		function,
		vegas_pdf<T>(dimensions, bins),
		alpha,
		generator
	);
}

/// @}

}

#endif
