#ifndef HEP_MC_VEGAS_HPP
#define HEP_MC_VEGAS_HPP

/*
 * hep-mc - A Template Library for Monte Carlo Integration
 * Copyright (C) 2012-2015  Christopher Schwan
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

#include "hep/mc/kahan_accumulator.hpp"
#include "hep/mc/vegas_callback.hpp"
#include "hep/mc/vegas_iteration_result.hpp"
#include "hep/mc/vegas_pdf.hpp"
#include "hep/mc/vegas_point.hpp"

#include <cstddef>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace hep
{

/// \addtogroup vegas_group
/// @{

/// Performs one VEGAS iteration. This integrates `function` over the
/// unit-hypercube using `calls` function evaluations with random numbers
/// generated by `generator`. The generator is not seeded. The `pdf` is used to
/// implement importance sampling; stratified sampling is not used. The
/// dimension of the function is determined by `pdf.size()`.
///
/// The parameter `total_calls` determines the sample size \f$ N \f$ of the
/// iteration \ref vegas_iteration performs and therefore usually has usually
/// the same value as `calls`. If VEGAS is run in parallel, then this function
/// will be called multiple times with a differently seeded generator and with
/// `calls` parameters each smaller than `total_calls` but their sum being equal
/// to `total_calls`.
template <typename T, typename F, typename D, typename R>
inline vegas_distribution_result<T> vegas_iteration(
	std::size_t calls,
	vegas_pdf<T> const& pdf,
	F&& function,
	D&& distributions,
	R&& generator
) {
	auto accumulator = make_distribution_accumulator(distributions);

	std::size_t const dimensions = pdf.dimensions();
	std::size_t const bins       = pdf.bins();

	std::vector<T> adjustment_data(dimensions * bins);
	std::vector<T> random_numbers(dimensions);
	std::vector<std::size_t> bin(dimensions);

	for (std::size_t i = 0; i != calls; ++i)
	{
		for (std::size_t j = 0; j != dimensions; ++j)
		{
			random_numbers[j] = std::generate_canonical<T,
				std::numeric_limits<T>::digits>(generator);
		}

		vegas_point<T> const point(random_numbers, bin, pdf);

		T const value = function(point) * point.weight();

		accumulator.add(point, value);

		T const square = value * value;

		// save square for each bin in order to refine the pdf later
		for (std::size_t j = 0; j != dimensions; ++j)
		{
			adjustment_data[j * bins + point.bin()[j]] += square;
		}
	}

	auto tmp = accumulator.result();
	vegas_distribution_result<T> const result(tmp, pdf, adjustment_data,
		tmp.distribution_results());

	return result;
}

template <typename T, typename F, typename R>
inline vegas_iteration_result<T> vegas_iteration(
	std::size_t calls,
	vegas_pdf<T> const& pdf,
	F&& function,
	R&& generator
) {
	return vegas_iteration<T>(
		calls,
		pdf,
		std::forward<F>(function),
		default_distribution<T>(),
		std::forward<R>(generator)
	);
}

/// Integrates `function` by performing `iteration_calls.size()` iterations of
/// the VEGAS algorithm, with as many function calls for each iteration
/// specified by the corresponding value in `iteration_calls`. The pdf
/// refinement is done using \ref vegas_refine_pdf which is called with the \f$
/// \alpha \f$-parameter given by `alpha`.
///
/// This function can be used to start from an already adapted pdf, e.g. one by
/// \ref vegas_iteration_result.pdf obtained by a previous \ref vegas call.
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
		auto const result = vegas_iteration(*i, pdf, function, generator);
		results.push_back(result);

		if (!vegas_callback<T>()(results))
		{
			break;
		}

		pdf = vegas_refine_pdf(pdf, alpha, result.adjustment_data());
	}

	return results;
}

/// Integrates `function` over the unit-hypercube with `dimensions` by
/// performing `iteration_calls.size()` iterations of the VEGAS algorithm, with
/// as many function calls for each iteration specified by the corresponding
/// value in `iteration_calls`. The pdf that is sampled from has `bins` for each
/// dimension as is refined \ref vegas_refine_pdf using the\f$ \alpha
/// \f$-parameter given by `alpha`.
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
		std::forward<F>(function),
		vegas_pdf<T>(dimensions, bins),
		alpha,
		std::forward<R>(generator)
	);
}

/// @}

}

#endif
