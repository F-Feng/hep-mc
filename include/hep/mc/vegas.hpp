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

#include <hep/mc/linear_grid.hpp>
#include <hep/mc/mc_helper.hpp>
#include <hep/mc/mc_point.hpp>
#include <hep/mc/mc_result.hpp>

#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <functional>
#include <vector>

namespace hep
{

/// \addtogroup vegas
/// @{

/**
 * The result of a single \ref vegas_iteration.
 */
template <typename T>
struct vegas_iteration_result : public mc_result<T>
{
	/**
	 * Constructor.
	 */
	vegas_iteration_result(
		std::size_t calls,
		linear_grid<T> const& grid,
		std::vector<T> const& adjustment_data
	)
		: mc_result<T>(
			calls,
			adjustment_data[adjustment_data.size() - 2],
			adjustment_data[adjustment_data.size() - 1]
		 )
		, grid(grid)
		, adjustment_data(adjustment_data)
	{
	}

	/**
	 * The grid used to obtain this result.
	 */
	linear_grid<T> grid;

	/**
	 * The data used to adjust the `grid` for a subsequent iteration.
	 */
	std::vector<T> adjustment_data;
};

/**
 * A point from the hypercube with the additional information in which bins the
 * point lies.
 */
template <typename T>
struct vegas_point : public mc_point<T>
{
	/**
	 * Constructor.
	 */
	vegas_point(
		std::size_t total_calls,
		std::vector<T>& random_numbers,
		std::vector<std::size_t>& bin,
		linear_grid<T> const& grid
	)
		: mc_point<T>(total_calls, random_numbers)
		, bin(bin)
	{
		std::size_t const dimensions = grid.dimensions();
		std::size_t const bins = grid.bins();

		for (std::size_t i = 0; i != dimensions; ++i)
		{
			// in every dimension the grid has 'bins' number of bins, the random
			// number determines a random bin (integer part) in the grid and the
			// exact position inside the bin (remainder)
			T const position = random_numbers[i] * bins;

			// the index of the selected bin
			std::size_t const index = position;

			// save the index for later
			bin[i] = index;

			// compute the value of the previous bin
			T const previous_bin = (index == 0) ? T() : grid(i, index - 1);

			// compute the difference of both both bins
			T const difference = grid(i, index) - previous_bin;

			// this rescales the random number to conform to the importance
			// represented by the grid
			this->point[i] = previous_bin + (position - index) * difference;

			// multiply weight for each dimension
			this->weight *= difference * bins;
		}
	}

	/**
	 * The indices that determine the bins of the point in the grid.
	 */
	std::vector<std::size_t>& bin;
};

/**
 * Adjust the `grid` using `adjustment_data`. The process can be controlled by
 * the parameter `alpha`. This function's code is based on the function
 * `refine_grid` from the CUBA VEGAS implementation from Thomas Hahn.
 */
template <typename T>
inline linear_grid<T> vegas_adjust_grid(
	T alpha,
	linear_grid<T> const& grid,
	std::vector<T> const& adjustment_data
) {
	std::size_t const bins = grid.bins();
	std::size_t const dimensions = grid.dimensions();

	linear_grid<T> new_grid(dimensions, bins);

	for (std::size_t i = 0; i != dimensions; ++i)
	{
		std::vector<T> smoothed(
			adjustment_data.begin() + (i+0) * bins,
			adjustment_data.begin() + (i+1) * bins
		);

		// smooth the entries stored in grid for dimension i
		T previous = smoothed[0];
		T current = smoothed[1];
		smoothed[0] = T(0.5) * (previous + current);
		T norm = smoothed[0];

		for (std::size_t bin = 1; bin < bins - 1; ++bin)
		{
			T const sum = previous + current;
			previous = current;
			current = smoothed[bin];
			smoothed[bin] = (sum + current) / T(3.0);
			norm += smoothed[bin];
		}
		smoothed[bins - 1] = T(0.5) * (previous + current);
		norm += smoothed[bins - 1];

		// if norm is zero there is nothing to do here
		if (norm == T())
		{
			continue;
		}

		norm = T(1.0) / norm;

		// compute the importance function for each bin
		T average_per_bin = T();

		std::vector<T> imp(bins);

		for (std::size_t bin = 0; bin < bins; ++bin)
		{
			if (smoothed[bin] > T())
			{
				T const r = smoothed[bin] * norm;
				T const impfun = std::pow((r - T(1.0)) / std::log(r), alpha);
				average_per_bin += impfun;
				imp[bin] = impfun;
			}
		}
		average_per_bin /= bins;

		// redefine the size of each bin
		current = T();
		T this_bin = T();

		int bin = -1;

		for (std::size_t new_bin = 0; new_bin != bins - 1; ++new_bin)
		{
			while (this_bin < average_per_bin)
			{
				this_bin += imp[++bin];
				previous = current;
				current = grid(i, bin);
			}

			this_bin -= average_per_bin;
			T const delta = (current - previous) * this_bin;
			new_grid(i, new_bin) = current - delta / imp[bin];
		}

		new_grid(i, bins - 1) = T(1.0);
	}

	return new_grid;
}

/**
 * Integrates `function` over the unit-hypercube using `calls` function
 * evaluations with random numbers generated by `generator` which is not seeded.
 * The `grid` is used to implement importance sampling; stratified sampling is
 * not used. The dimension of the function is determined by `grid.size()`.
 *
 * If `total_calls` is larger than `calls` this means the iteration is split up
 * in multiple `vegas_iteration` calls that are typically run in parallel. In
 * this case `total_calls` is the sum of all of function evaluations used.
 */
template <typename T, typename F, typename R>
inline vegas_iteration_result<T> vegas_iteration(
	std::size_t calls,
	std::size_t total_calls,
	linear_grid<T> const& grid,
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

		// perform kahan summation 'sum += value' - this improves precision if
		// T is single precision and many values are added
		T const y = value - compensation;
		T const t = average + y;
		compensation = (t - average) - y;
		average = t;

		T const square = value * value;

		// no kahan summation needed, because it only affects the result
		// indirectly via the grid recomputation
		averaged_squares += square;

		// save square for each bin in order to adjust the grid later
		for (std::size_t j = 0; j != dimensions; ++j)
		{
			adjustment_data[j * bins + point.bin[j]] += square;
		}
	}

	// save 'sum' and 'sum_of_squares' by rescaling the variables
	adjustment_data[dimensions * bins + 0] = average * T(total_calls);
	adjustment_data[dimensions * bins + 1] =
		averaged_squares * T(total_calls) * T(total_calls);

	return vegas_iteration_result<T>(calls, grid, adjustment_data);
}

/**
 * Creates an equally-spaced VEGAS grid with the specified number of `bins` for
 * a function with `dimensions` parameters. Using this grid together with
 * \ref vegas_iteration is equivalent to an integration performed by the \ref
 * plain algorithm.
 */
template <typename T>
inline linear_grid<T> vegas_grid(std::size_t dimensions, std::size_t bins)
{
	std::vector<T> one_dimensional_grid(bins);
	for (std::size_t i = 0; i != bins; ++i)
	{
		one_dimensional_grid[i] = T(1 + i) / T(bins);
	}

	return std::vector<std::vector<T>>(dimensions, one_dimensional_grid);
}

/**
 * The default callback function. This function does nothing.
 *
 * \see vegas_callback
 */
template <typename T>
inline bool vegas_default_callback(
	std::vector<vegas_iteration_result<T>> const&
) {
	return true;
}

/**
 * Callback function that prints a detailed summary about every iteration
 * performed so far.
 *
 * \see vegas_callback
 */
template <typename T>
inline bool vegas_verbose_callback(
	std::vector<vegas_iteration_result<T>> const& results
) {
	std::cout << "iteration " << (results.size()-1) << " finished.\n";

	// print result for this iteration
	std::cout << "this iteration: N=" << results.back().calls;
	std::cout << " E=" << results.back().value << " +- ";
	std::cout << results.back().error << " (";
	std::cout << (T(100.0) * results.back().error /
		std::abs(results.back().value));
	std::cout << "%)\n";

	// compute cumulative results
	auto const result = cumulative_result<T>(results.begin(), results.end());
	T const chi = chi_square_dof<T>(results.begin(), results.end());

	// print the combined result
	std::cout << "all iterations: N=" << result.calls;
	std::cout << " E=" << result.value << " +- " << result.error;
	std::cout << " (" << (T(100.0) * result.error / std::abs(result.value));
	std::cout << "%) chi^2/dof=" << chi << "\n\n";

	std::cout.flush();

	return true;
}

/**
 * Sets the vegas `callback` function and returns it. This function is called
 * after each iteration performed by \ref vegas(). The default callback is \ref
 * vegas_default_callback. The function can e.g. be set to \ref
 * vegas_verbose_callback which prints after each iteration. If the callback
 * function returns `false` the integration is stopped.
 *
 * If this function is called without any argument, the previous function is
 * retained.
 */
template <typename T>
inline std::function<bool(std::vector<vegas_iteration_result<T>>)>
vegas_callback(
	std::function<bool(std::vector<vegas_iteration_result<T>>)> callback
		= nullptr
) {
	static std::function<bool(std::vector<vegas_iteration_result<T>>)> object
		= vegas_default_callback<T>;

	if (callback != nullptr)
	{
		object = callback;
	}

	return object;
}

/**
 * Implements the VEGAS algorithm. This function can be used to start from an
 * already adapted grid, e.g. one by \ref vegas_iteration_result.grid obtained
 * by a previous \ref vegas() call.
 */
template <typename T, typename F, typename R = std::mt19937>
inline std::vector<vegas_iteration_result<T>> vegas(
	std::vector<std::size_t> const& iteration_calls,
	F&& function,
	linear_grid<T> const& start_grid,
	T alpha = T(1.5),
	R&& generator = std::mt19937()
) {
	auto grid = start_grid;

	// vector holding all iteration results
	std::vector<vegas_iteration_result<T>> results;
	results.reserve(iteration_calls.size());

	// perform iterations
	for (auto i = iteration_calls.begin(); i != iteration_calls.end(); ++i)
	{
		auto const result = vegas_iteration(*i, *i, grid, function, generator);
		results.push_back(result);

		if (!vegas_callback<T>()(results))
		{
			break;
		}

		grid = vegas_adjust_grid(alpha, grid, result.adjustment_data);
	}

	return results;
}

/**
 * Implements the VEGAS algorithm. In particular, this function calls \ref
 * vegas_iteration for every number in `iteration_calls` determining the `calls`
 * parameter for each iteration. After each iteration the grid is adjusted using
 * \ref vegas_adjust_grid. The grid adjustment itself can be controlled by the
 * parameter `alpha`. The intermediate results are passed to the function set by
 * \ref vegas_callback which can e.g. be used to print them out. The callback
 * function is able to stop the integration if it returns `false`. In this case
 * less iterations are performed than requested.
 *
 * \param dimensions The number of parameters `function` accepts.
 * \param iteration_calls The number of function calls that are used to obtain
 *        a result for each iteration. `iteration_calls.size()` determines the
 *        number of iterations.
 * \param function The function that will be integrated over the hypercube. See
 *        \ref integrands for further explanation.
 * \param bins The number of bins that the grid will contain for each dimension.
 * \param alpha The \f$ \alpha \f$ parameter of VEGAS. This parameter should be
 *        between `1` and `2`.
 * \param generator The random number generator that will be used to generate
 *        random points from the hypercube. This generator is not seeded.
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
		vegas_grid<T>(dimensions, bins),
		alpha,
		generator
	);
}

/// @}

}

#endif
